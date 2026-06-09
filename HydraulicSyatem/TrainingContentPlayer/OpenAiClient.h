#pragma once

#include "ScpConfigReader.h"

struct OPENAI_HTTP_DIAGNOSTIC
{
    CStringW strUrl;
    CStringW strModel;
    size_t   nRequestBodySize;
    int      nHttpStatus;
    DWORD    dwWin32Error;
    CStringW strFailureStage;
    CStringW strTransportLibrary;
    CStringW strTransportErrorText;

    OPENAI_HTTP_DIAGNOSTIC()
        : nRequestBodySize(0)
        , nHttpStatus(0)
        , dwWin32Error(0)
    {
    }

    CStringW FormatErrorLog() const;
};

namespace OpenAiClient
{
    CStringW GetCurrentTimestamp();
    CStringW MaskApiKeyForLog(const CStringW& strApiKey);
    CStringW ExtractAssistantMessage(const std::string& strJson);
    CStringW ExtractApiErrorMessage(const std::string& strJson);
    CStringW BuildPossibleCause(int nHttpStatus, const CStringW& strApiError);
    CStringW FormatWin32Error(DWORD dwError);

    BOOL WriteUtf8LogFile(const CStringW& strFilePath, const CStringW& strContent);
    BOOL WriteLogFile(const CStringW& strFileName, const CStringW& strContent);
    BOOL WriteErrorLog(const CStringW& strContent);

    BOOL HttpPostChatCompletion(
        const SCP_OPENAI_CONFIG& config,
        const CStringW& strPrompt,
        int& nHttpStatus,
        std::string& strResponseBody,
        CStringW& strTransportError,
        OPENAI_HTTP_DIAGNOSTIC* pDiagnostic = nullptr);
}
