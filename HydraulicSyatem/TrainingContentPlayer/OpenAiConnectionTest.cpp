#include "pch.h"
#include "OpenAiConnectionTest.h"
#include "Util.h"

#include <winhttp.h>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr LPCWSTR kOpenAiHost = L"api.openai.com";
    constexpr LPCWSTR kOpenAiPath = L"/v1/chat/completions";

    CStringW GetCurrentTimestamp()
    {
        SYSTEMTIME st = {};
        GetLocalTime(&st);

        CStringW strTime;
        strTime.Format(
            L"%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond);
        return strTime;
    }

    BOOL WriteUtf8LogFile(const CStringW& strFilePath, const CStringW& strContent)
    {
        try
        {
            std::ofstream ofs(strFilePath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
                return FALSE;

            const std::string strHeader = TrainingUtil::CStringWToUtf8(
                L"Timestamp: " + GetCurrentTimestamp() + L"\r\n\r\n");
            const std::string strBody = TrainingUtil::CStringWToUtf8(strContent);
            ofs.write(strHeader.data(), static_cast<std::streamsize>(strHeader.size()));
            ofs.write(strBody.data(), static_cast<std::streamsize>(strBody.size()));
            return TRUE;
        }
        catch (...)
        {
            return FALSE;
        }
    }

    CStringW ExtractAssistantMessage(const std::string& strJson)
    {
        try
        {
            const nlohmann::json jRoot = nlohmann::json::parse(strJson);
            if (!jRoot.contains("choices") || !jRoot["choices"].is_array() || jRoot["choices"].empty())
                return CStringW();

            const nlohmann::json& jChoice = jRoot["choices"][0];
            if (!jChoice.contains("message") || !jChoice["message"].is_object())
                return CStringW();

            const nlohmann::json& jMessage = jChoice["message"];
            if (!jMessage.contains("content"))
                return CStringW();

            if (jMessage["content"].is_string())
                return TrainingUtil::Utf8ToCStringW(jMessage["content"].get<std::string>());

            return CStringW();
        }
        catch (...)
        {
            return CStringW();
        }
    }

    CStringW ExtractApiErrorMessage(const std::string& strJson)
    {
        try
        {
            const nlohmann::json jRoot = nlohmann::json::parse(strJson);
            if (jRoot.contains("error") && jRoot["error"].is_object())
            {
                const nlohmann::json& jError = jRoot["error"];
                if (jError.contains("message") && jError["message"].is_string())
                    return TrainingUtil::Utf8ToCStringW(jError["message"].get<std::string>());
            }
        }
        catch (...)
        {
        }

        return CStringW();
    }

    CStringW BuildPossibleCause(int nHttpStatus, const CStringW& strApiError)
    {
        if (nHttpStatus == 401)
            return L"API Key 오류 가능성이 있습니다.";
        if (nHttpStatus == 403)
            return L"API 접근 권한 또는 계정 제한 가능성이 있습니다.";
        if (nHttpStatus == 404)
            return L"Model 이름이 잘못되었거나 지원되지 않는 모델일 수 있습니다.";
        if (nHttpStatus == 429)
            return L"요청 한도 초과 또는 크레딧 부족 가능성이 있습니다.";

        CStringW strLower = strApiError;
        strLower.MakeLower();
        if (strLower.Find(L"insufficient_quota") >= 0 ||
            strLower.Find(L"billing") >= 0 ||
            strLower.Find(L"credit") >= 0)
        {
            return L"API Key 오류 또는 크레딧 부족 가능성이 있습니다.";
        }

        if (nHttpStatus == 0)
            return L"네트워크 연결 또는 방화벽/프록시 설정을 확인하세요.";

        return L"API Key 오류, Model 설정 오류, 네트워크 문제 또는 크레딧 부족 가능성이 있습니다.";
    }

    BOOL HttpPostChatCompletion(
        const SCP_OPENAI_CONFIG& config,
        const CStringW& strPrompt,
        int& nHttpStatus,
        std::string& strResponseBody,
        CStringW& strTransportError)
    {
        nHttpStatus = 0;
        strResponseBody.clear();
        strTransportError.Empty();

        nlohmann::json jBody;
        jBody["model"] = TrainingUtil::CStringWToUtf8(config.strModel);
        jBody["messages"] = nlohmann::json::array({
            nlohmann::json::object({
                {"role", "user"},
                {"content", TrainingUtil::CStringWToUtf8(strPrompt)}
            })
        });

        const std::string strRequestBody = jBody.dump();

        HINTERNET hSession = WinHttpOpen(
            L"TrainingContentPlayer/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (hSession == nullptr)
        {
            strTransportError = L"WinHTTP 세션을 시작할 수 없습니다.";
            return FALSE;
        }

        HINTERNET hConnect = WinHttpConnect(hSession, kOpenAiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (hConnect == nullptr)
        {
            WinHttpCloseHandle(hSession);
            strTransportError = L"OpenAI 서버에 연결할 수 없습니다.";
            return FALSE;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            kOpenAiPath,
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);
        if (hRequest == nullptr)
        {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            strTransportError = L"HTTP 요청을 생성할 수 없습니다.";
            return FALSE;
        }

        const CStringW strHeaders =
            L"Content-Type: application/json\r\n"
            L"Authorization: Bearer " + config.strApiKey + L"\r\n";

        BOOL bResult = WinHttpSendRequest(
            hRequest,
            strHeaders,
            static_cast<DWORD>(-1L),
            const_cast<char*>(strRequestBody.data()),
            static_cast<DWORD>(strRequestBody.size()),
            static_cast<DWORD>(strRequestBody.size()),
            0);

        if (bResult)
            bResult = WinHttpReceiveResponse(hRequest, nullptr);

        if (!bResult)
        {
            strTransportError = L"OpenAI API 요청 전송 또는 응답 수신에 실패했습니다.";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return FALSE;
        }

        DWORD dwStatusCode = 0;
        DWORD dwStatusSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &dwStatusCode,
            &dwStatusSize,
            WINHTTP_NO_HEADER_INDEX);
        nHttpStatus = static_cast<int>(dwStatusCode);

        std::string strResponse;
        DWORD dwAvailable = 0;
        do
        {
            dwAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable))
                break;
            if (dwAvailable == 0)
                break;

            std::vector<char> buffer(dwAvailable);
            DWORD dwRead = 0;
            if (!WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwRead))
                break;

            strResponse.append(buffer.data(), buffer.data() + dwRead);
        } while (dwAvailable > 0);

        strResponseBody = std::move(strResponse);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return TRUE;
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
    return TrainingUtil::GetAppDirectory() + L"logs\\";
}

BOOL OpenAiConnectionTest::EnsureLogsFolder()
{
    const CStringW strLogs = GetLogsFolderPath();
    if (CreateDirectoryW(strLogs, nullptr))
        return TRUE;

    const DWORD dwErr = GetLastError();
    return dwErr == ERROR_ALREADY_EXISTS;
}

BOOL OpenAiConnectionTest::WriteLogFile(const CStringW& strFileName, const CStringW& strContent)
{
    EnsureLogsFolder();
    return WriteUtf8LogFile(GetLogsFolderPath() + strFileName, strContent);
}

CStringW OpenAiConnectionTest::MaskApiKeyForLog(const CStringW& strApiKey)
{
    if (strApiKey.GetLength() <= 8)
        return L"****";

    return strApiKey.Left(7) + L"****" + strApiKey.Mid(strApiKey.GetLength() - 4);
}

OPENAI_TEST_RESULT OpenAiConnectionTest::RunConnectionTest(const SCP_OPENAI_CONFIG& config)
{
    OPENAI_TEST_RESULT result;
    const CStringW strPrompt = GetTestPrompt();

    nlohmann::json jLogBody;
    jLogBody["model"] = TrainingUtil::CStringWToUtf8(config.strModel);
    jLogBody["messages"] = nlohmann::json::array({
        nlohmann::json::object({
            {"role", "user"},
            {"content", TrainingUtil::CStringWToUtf8(strPrompt)}
        })
    });

    result.strRequestLog =
        L"POST https://api.openai.com/v1/chat/completions\r\n"
        L"Authorization: Bearer " + MaskApiKeyForLog(config.strApiKey) + L"\r\n"
        L"Content-Type: application/json\r\n\r\n" +
        TrainingUtil::Utf8ToCStringW(jLogBody.dump(2));

    WriteLogFile(L"ChatGPT_Test_Request.txt", result.strRequestLog);

    std::string strResponseBody;
    CStringW strTransportError;
    const BOOL bTransportOk = HttpPostChatCompletion(
        config,
        strPrompt,
        result.nHttpStatus,
        strResponseBody,
        strTransportError);

    result.strResponseLog = TrainingUtil::Utf8ToCStringW(strResponseBody);
    if (!strResponseBody.empty())
        WriteLogFile(L"ChatGPT_Test_Response.txt", result.strResponseLog);

    if (!bTransportOk)
    {
        result.bSuccess = FALSE;
        result.strErrorMessage = strTransportError;
        result.strPossibleCause = BuildPossibleCause(0, CStringW());
        result.strErrorLog =
            L"Transport Error\r\n\r\n" + strTransportError;
        WriteLogFile(L"ChatGPT_Test_Error.txt", result.strErrorLog);
        return result;
    }

    const CStringW strApiError = ExtractApiErrorMessage(strResponseBody);
    if (result.nHttpStatus < 200 || result.nHttpStatus >= 300)
    {
        result.bSuccess = FALSE;
        result.strErrorMessage = strApiError.IsEmpty()
            ? L"OpenAI API 호출이 실패했습니다."
            : strApiError;
        result.strPossibleCause = BuildPossibleCause(result.nHttpStatus, strApiError);
        result.strErrorLog.Format(
            L"HTTP Status: %d\r\n\r\nError:\r\n%s\r\n\r\nPossible Cause:\r\n%s",
            result.nHttpStatus,
            result.strErrorMessage.GetString(),
            result.strPossibleCause.GetString());
        WriteLogFile(L"ChatGPT_Test_Error.txt", result.strErrorLog);
        return result;
    }

    result.strAssistantMessage = ExtractAssistantMessage(strResponseBody);
    if (result.strAssistantMessage.IsEmpty())
    {
        result.bSuccess = FALSE;
        result.strErrorMessage = L"응답은 수신했지만 Assistant 메시지를 해석할 수 없습니다.";
        result.strPossibleCause = L"응답 JSON 형식이 예상과 다릅니다.";
        result.strErrorLog = result.strErrorMessage + L"\r\n\r\n" + result.strResponseLog;
        WriteLogFile(L"ChatGPT_Test_Error.txt", result.strErrorLog);
        return result;
    }

    result.bSuccess = TRUE;
    return result;
}
