#include "pch.h"
#include "OpenAiConnectionTest.h"
#include "ScpPaths.h"
#include "Util.h"

namespace
{
    CStringW BuildRequestLog(const SCP_OPENAI_CONFIG& config, const CStringW& strPrompt)
    {
        nlohmann::json jLogBody;
        jLogBody["model"] = TrainingUtil::CStringWToUtf8(config.strModel);
        jLogBody["messages"] = nlohmann::json::array({
            nlohmann::json::object({
                {"role", "user"},
                {"content", TrainingUtil::CStringWToUtf8(strPrompt)}
            })
        });

        return
            L"POST https://api.openai.com/v1/chat/completions\r\n"
            L"Authorization: Bearer " + OpenAiConnectionTest::MaskApiKeyForLog(config.strApiKey) + L"\r\n"
            L"Content-Type: application/json\r\n\r\n" +
            TrainingUtil::Utf8ToCStringW(jLogBody.dump(2));
    }

    void WriteNamedLogs(
        const CStringW& strLogBaseName,
        const CStringW& strRequestLog,
        const CStringW& strResponseLog,
        const CStringW& strErrorLog,
        BOOL bWriteError)
    {
        const CStringW strRequestFile = strLogBaseName + L"_Request.txt";
        const CStringW strResponseFile = strLogBaseName + L"_Response.txt";
        const CStringW strErrorFile = strLogBaseName + L"_Error.txt";

        OpenAiConnectionTest::WriteLogFile(strRequestFile, strRequestLog);
        if (!strResponseLog.IsEmpty())
            OpenAiConnectionTest::WriteLogFile(strResponseFile, strResponseLog);

        const CStringW strPrefix = ScpPaths::GetTimestampPrefix();
        OpenAiConnectionTest::WriteLogFile(strPrefix + L"_Request.txt", strRequestLog);
        if (!strResponseLog.IsEmpty())
            OpenAiConnectionTest::WriteLogFile(strPrefix + L"_Response.txt", strResponseLog);

        if (bWriteError && !strErrorLog.IsEmpty())
        {
            OpenAiConnectionTest::WriteLogFile(strErrorFile, strErrorLog);
            OpenAiClient::WriteErrorLog(strErrorLog);
        }
    }
}

CStringW OpenAiConnectionTest::GetTestPrompt()
{
    return
        L"안녕하세요. SCP와 ChatGPT 연동 시험입니다. "
        L"정상 연결되었으면 'SCP 연동 성공'이라고 답해 주세요.";
}

CStringW OpenAiConnectionTest::GetLogsFolderPath()
{
    return ScpPaths::GetLogFolder();
}

BOOL OpenAiConnectionTest::EnsureLogsFolder()
{
    return ScpPaths::EnsureFolder(GetLogsFolderPath());
}

BOOL OpenAiConnectionTest::WriteLogFile(const CStringW& strFileName, const CStringW& strContent)
{
    return OpenAiClient::WriteLogFile(strFileName, strContent);
}

CStringW OpenAiConnectionTest::MaskApiKeyForLog(const CStringW& strApiKey)
{
    return OpenAiClient::MaskApiKeyForLog(strApiKey);
}

OPENAI_CHAT_COMPLETION_RESULT OpenAiConnectionTest::RunChatCompletion(
    const SCP_OPENAI_CONFIG& config,
    const CStringW& strPrompt,
    const CStringW& strLogBaseName)
{
    OPENAI_CHAT_COMPLETION_RESULT result;
    result.strRequestLog = BuildRequestLog(config, strPrompt);
    WriteNamedLogs(strLogBaseName, result.strRequestLog, CStringW(), CStringW(), FALSE);

    std::string strResponseBody;
    CStringW strTransportError;
    result.bTransportOk = OpenAiClient::HttpPostChatCompletion(
        config,
        strPrompt,
        result.nHttpStatus,
        strResponseBody,
        strTransportError,
        &result.diagnostic);

    result.strResponseLog = TrainingUtil::Utf8ToCStringW(strResponseBody);
    WriteNamedLogs(strLogBaseName, result.strRequestLog, result.strResponseLog, CStringW(), FALSE);

    if (!result.bTransportOk)
    {
        result.strErrorMessage = strTransportError;
        result.strPossibleCause = OpenAiClient::BuildPossibleCause(0, CStringW());
        result.strErrorLog =
            L"Transport Error\r\n\r\n" +
            result.diagnostic.FormatErrorLog() +
            L"\r\nMessage:\r\n" + strTransportError;
        WriteNamedLogs(strLogBaseName, result.strRequestLog, result.strResponseLog, result.strErrorLog, TRUE);
        return result;
    }

    const CStringW strApiError = OpenAiClient::ExtractApiErrorMessage(strResponseBody);
    if (result.nHttpStatus < 200 || result.nHttpStatus >= 300)
    {
        result.strErrorMessage = strApiError.IsEmpty()
            ? L"OpenAI API 호출이 실패했습니다."
            : strApiError;
        result.strPossibleCause = OpenAiClient::BuildPossibleCause(result.nHttpStatus, strApiError);
        CStringW strHttpStatus;
        strHttpStatus.Format(L"%d", result.nHttpStatus);
        result.strErrorLog =
            result.diagnostic.FormatErrorLog() +
            L"\r\n\r\nHTTP Status: " + strHttpStatus +
            L"\r\n\r\nError:\r\n" + result.strErrorMessage +
            L"\r\n\r\nPossible Cause:\r\n" + result.strPossibleCause;
        WriteNamedLogs(strLogBaseName, result.strRequestLog, result.strResponseLog, result.strErrorLog, TRUE);
        return result;
    }

    result.strAssistantMessage = OpenAiClient::ExtractAssistantMessage(strResponseBody);
    if (result.strAssistantMessage.IsEmpty())
    {
        result.strErrorMessage = L"응답은 수신했지만 Assistant 메시지를 해석할 수 없습니다.";
        result.strPossibleCause = L"응답 JSON 형식이 예상과 다릅니다.";
        result.strErrorLog =
            result.diagnostic.FormatErrorLog() +
            L"\r\n\r\n" + result.strErrorMessage +
            L"\r\n\r\n" + result.strResponseLog;
        WriteNamedLogs(strLogBaseName, result.strRequestLog, result.strResponseLog, result.strErrorLog, TRUE);
        return result;
    }

    result.bSuccess = TRUE;
    return result;
}

OPENAI_TEST_RESULT OpenAiConnectionTest::RunConnectionTest(const SCP_OPENAI_CONFIG& config)
{
    OPENAI_TEST_RESULT result;
    const OPENAI_CHAT_COMPLETION_RESULT chatResult =
        RunChatCompletion(config, GetTestPrompt(), L"ChatGPT_Test");

    result.nHttpStatus = chatResult.nHttpStatus;
    result.strAssistantMessage = chatResult.strAssistantMessage;
    result.strErrorMessage = chatResult.strErrorMessage;
    result.strPossibleCause = chatResult.strPossibleCause;
    result.strRequestLog = chatResult.strRequestLog;
    result.strResponseLog = chatResult.strResponseLog;
    result.strErrorLog = chatResult.strErrorLog;
    result.bSuccess = chatResult.bSuccess;
    return result;
}
