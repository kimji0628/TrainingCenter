#pragma once

#include "QuestionItem.h"
#include "ScpConfigReader.h"

enum class OPENAI_GENERATE_STAGE
{
    None = 0,
    Config,
    PdfExtract,
    PromptFile,
    ApiCall,
    ChatGptResponse,
    ParseResponse,
    InsufficientQuestions
};

struct OPENAI_GENERATE_REQUEST
{
    SCP_OPENAI_CONFIG config;
    CStringW          strPdfPath;
    BOOL              bAllPages;
    int               nPageStart;
    int               nPageEnd;
    int               nQuestionCount;

    OPENAI_GENERATE_REQUEST()
        : bAllPages(TRUE)
        , nPageStart(1)
        , nPageEnd(1)
        , nQuestionCount(5)
    {
    }
};

struct OPENAI_GENERATE_RESULT
{
    BOOL                 bSuccess;
    OPENAI_GENERATE_STAGE stage;
    int                  nHttpStatus;
    int                  nRequestedCount;
    CQuestionItemArray   questions;
    CStringW             strAssistantMessage;
    CStringW             strErrorMessage;
    CStringW             strPossibleCause;
    CStringW             strRequestLog;
    CStringW             strResponseLog;
    CStringW             strErrorLog;

    OPENAI_GENERATE_RESULT()
        : bSuccess(FALSE)
        , stage(OPENAI_GENERATE_STAGE::None)
        , nHttpStatus(0)
        , nRequestedCount(0)
    {
    }
};

namespace OpenAiQuestionGenerator
{
    CStringW StageToUserMessage(OPENAI_GENERATE_STAGE stage);
    OPENAI_GENERATE_RESULT Run(const OPENAI_GENERATE_REQUEST& request);
}
