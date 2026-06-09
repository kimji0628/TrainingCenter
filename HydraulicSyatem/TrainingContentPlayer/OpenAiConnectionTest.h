#pragma once

#include "OpenAiClient.h"
#include "ScpConfigReader.h"

struct OPENAI_CHAT_COMPLETION_RESULT
{
    BOOL     bTransportOk;
    BOOL     bSuccess;
    int      nHttpStatus;
    CStringW strAssistantMessage;
    CStringW strErrorMessage;
    CStringW strPossibleCause;
    CStringW strRequestLog;
    CStringW strResponseLog;
    CStringW strErrorLog;
    OPENAI_HTTP_DIAGNOSTIC diagnostic;

    OPENAI_CHAT_COMPLETION_RESULT()
        : bTransportOk(FALSE)
        , bSuccess(FALSE)
        , nHttpStatus(0)
    {
    }
};

struct OPENAI_TEST_RESULT
{
    BOOL     bSuccess;
    int      nHttpStatus;
    CStringW strAssistantMessage;
    CStringW strErrorMessage;
    CStringW strPossibleCause;
    CStringW strRequestLog;
    CStringW strResponseLog;
    CStringW strErrorLog;

    OPENAI_TEST_RESULT()
        : bSuccess(FALSE)
        , nHttpStatus(0)
    {
    }
};

namespace OpenAiConnectionTest
{
    CStringW GetTestPrompt();
    CStringW GetLogsFolderPath();
    BOOL EnsureLogsFolder();
    BOOL WriteLogFile(const CStringW& strFileName, const CStringW& strContent);
    CStringW MaskApiKeyForLog(const CStringW& strApiKey);

    OPENAI_CHAT_COMPLETION_RESULT RunChatCompletion(
        const SCP_OPENAI_CONFIG& config,
        const CStringW& strPrompt,
        const CStringW& strLogBaseName);

    OPENAI_TEST_RESULT RunConnectionTest(const SCP_OPENAI_CONFIG& config);
}
