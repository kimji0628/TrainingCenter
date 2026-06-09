#include "pch.h"
#include "OpenAiQuestionGenerator.h"
#include "OpenAiConnectionTest.h"
#include "PdfTextExtractor.h"
#include "PromptLoader.h"
#include "QuestionTestLoader.h"
#include "OpenAiClient.h"
#include "Util.h"

namespace
{
    CStringW GetPdfFileName(const CStringW& strPdfPath)
    {
        int nSlash = strPdfPath.ReverseFind(L'\\');
        return (nSlash >= 0) ? strPdfPath.Mid(nSlash + 1) : strPdfPath;
    }

    void Fail(
        OPENAI_GENERATE_RESULT& result,
        OPENAI_GENERATE_STAGE stage,
        const CStringW& strMessage,
        const CStringW& strPossibleCause = CStringW(),
        const CStringW& strExtraLog = CStringW())
    {
        result.bSuccess = FALSE;
        result.stage = stage;
        result.strErrorMessage = strMessage;
        result.strPossibleCause = strPossibleCause;
        result.strErrorLog =
            L"Stage: " + OpenAiQuestionGenerator::StageToUserMessage(stage) +
            L"\r\n\r\n" + strMessage;

        if (!result.strPossibleCause.IsEmpty())
            result.strErrorLog += L"\r\n\r\nPossible Cause:\r\n" + result.strPossibleCause;

        if (!strExtraLog.IsEmpty())
            result.strErrorLog += L"\r\n\r\n" + strExtraLog;

        OpenAiClient::WriteErrorLog(result.strErrorLog);
    }
}

CStringW OpenAiQuestionGenerator::StageToUserMessage(OPENAI_GENERATE_STAGE stage)
{
    switch (stage)
    {
    case OPENAI_GENERATE_STAGE::Config:
        return L"설정 파일 오류";
    case OPENAI_GENERATE_STAGE::PdfExtract:
        return L"PDF 텍스트 추출 실패";
    case OPENAI_GENERATE_STAGE::PromptFile:
        return L"프롬프트 파일 읽기 실패";
    case OPENAI_GENERATE_STAGE::ApiCall:
        return L"API 호출 실패";
    case OPENAI_GENERATE_STAGE::ChatGptResponse:
        return L"ChatGPT 응답 실패";
    case OPENAI_GENERATE_STAGE::ParseResponse:
        return L"응답 파싱 실패";
    case OPENAI_GENERATE_STAGE::InsufficientQuestions:
        return L"생성된 문제 수 부족";
    default:
        return L"알 수 없는 오류";
    }
}

OPENAI_GENERATE_RESULT OpenAiQuestionGenerator::Run(const OPENAI_GENERATE_REQUEST& request)
{
    OPENAI_GENERATE_RESULT result;
    result.nRequestedCount = request.nQuestionCount;

    if (!request.config.IsValid())
    {
        Fail(result, OPENAI_GENERATE_STAGE::Config, L"OpenAI API Key 또는 Model 설정이 없습니다.");
        return result;
    }

    CStringW strPdfText;
    CStringW strPdfError;
    if (!PdfTextExtractor::ExtractPageRangeText(
            request.strPdfPath,
            request.bAllPages,
            request.nPageStart,
            request.nPageEnd,
            strPdfText,
            strPdfError))
    {
        Fail(result, OPENAI_GENERATE_STAGE::PdfExtract, strPdfError);
        return result;
    }

    const CStringW strPromptPath = PromptLoader::GetQuestionPromptFilePath();
    CStringW strPromptTemplate;
    CStringW strPromptError;
    if (!PromptLoader::LoadPromptFile(strPromptPath, strPromptTemplate, strPromptError))
    {
        Fail(result, OPENAI_GENERATE_STAGE::PromptFile, strPromptError);
        return result;
    }

    int nPageStart = request.nPageStart;
    int nPageEnd = request.nPageEnd;
    if (request.bAllPages)
    {
        nPageStart = 1;
        nPageEnd = request.nPageEnd;
    }

    const CStringW strPrompt = PromptLoader::BuildQuestionGenerationPrompt(
        strPromptTemplate,
        strPdfText,
        request.nQuestionCount,
        nPageStart,
        nPageEnd,
        GetPdfFileName(request.strPdfPath));

    const OPENAI_CHAT_COMPLETION_RESULT chatResult =
        OpenAiConnectionTest::RunChatCompletion(
            request.config,
            strPrompt,
            L"ChatGPT_Generate");

    result.nHttpStatus = chatResult.nHttpStatus;
    result.strAssistantMessage = chatResult.strAssistantMessage;
    result.strRequestLog = chatResult.strRequestLog;
    result.strResponseLog = chatResult.strResponseLog;
    result.strErrorLog = chatResult.strErrorLog;

    if (!chatResult.bTransportOk)
    {
        Fail(
            result,
            OPENAI_GENERATE_STAGE::ApiCall,
            chatResult.strErrorMessage,
            chatResult.strPossibleCause,
            chatResult.diagnostic.FormatErrorLog());
        return result;
    }

    if (!chatResult.bSuccess)
    {
        const OPENAI_GENERATE_STAGE stage =
            (chatResult.nHttpStatus >= 200 && chatResult.nHttpStatus < 300)
                ? OPENAI_GENERATE_STAGE::ChatGptResponse
                : OPENAI_GENERATE_STAGE::ApiCall;
        Fail(
            result,
            stage,
            chatResult.strErrorMessage,
            chatResult.strPossibleCause,
            chatResult.diagnostic.FormatErrorLog());
        return result;
    }

    CStringW strParseError;
    if (!QuestionTestLoader::ParseFromContent(
            result.strAssistantMessage,
            result.questions,
            strParseError))
    {
        Fail(
            result,
            OPENAI_GENERATE_STAGE::ParseResponse,
            strParseError,
            L"ChatGPT 응답이 QSTART/QEND 형식을 따르지 않았을 수 있습니다.");
        return result;
    }

    if (static_cast<int>(result.questions.size()) < request.nQuestionCount)
    {
        CStringW strMessage;
        strMessage.Format(
            L"요청한 문제 %d개 중 %d개만 생성되었습니다.",
            request.nQuestionCount,
            static_cast<int>(result.questions.size()));
        Fail(result, OPENAI_GENERATE_STAGE::InsufficientQuestions, strMessage);
        return result;
    }

    result.bSuccess = TRUE;
    result.stage = OPENAI_GENERATE_STAGE::None;
    return result;
}
