#include "pch.h"
#include "OpenAiClient.h"
#include "ScpPaths.h"
#include "Util.h"

#include <winhttp.h>
#include <fstream>

#pragma comment(lib, "winhttp.lib")

namespace
{
    constexpr LPCWSTR kOpenAiHost = L"api.openai.com";
    constexpr LPCWSTR kOpenAiPath = L"/v1/chat/completions";
    constexpr LPCWSTR kOpenAiUrl = L"https://api.openai.com/v1/chat/completions";
    constexpr DWORD kConnectTimeoutMs = 60000;
    constexpr DWORD kSendTimeoutMs = 300000;
    constexpr DWORD kReceiveTimeoutMs = 600000;

    void SetHttpTimeouts(HINTERNET hRequest)
    {
        if (hRequest == nullptr)
            return;

        DWORD dwConnectTimeout = kConnectTimeoutMs;
        DWORD dwSendTimeout = kSendTimeoutMs;
        DWORD dwReceiveTimeout = kReceiveTimeoutMs;

        WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &dwConnectTimeout, sizeof(dwConnectTimeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT, &dwSendTimeout, sizeof(dwSendTimeout));
        WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &dwReceiveTimeout, sizeof(dwReceiveTimeout));
    }

    void FillDiagnosticBase(
        OPENAI_HTTP_DIAGNOSTIC& diag,
        const SCP_OPENAI_CONFIG& config,
        size_t nRequestBodySize)
    {
        diag.strUrl = kOpenAiUrl;
        diag.strModel = config.strModel;
        diag.nRequestBodySize = nRequestBodySize;
        diag.strTransportLibrary = L"WinHTTP";
        diag.nHttpStatus = 0;
        diag.dwWin32Error = 0;
    }

    void SetDiagnosticFailure(
        OPENAI_HTTP_DIAGNOSTIC& diag,
        const CStringW& strStage,
        DWORD dwError)
    {
        diag.strFailureStage = strStage;
        diag.dwWin32Error = dwError;
        diag.strTransportErrorText = OpenAiClient::FormatWin32Error(dwError);
    }
}

CStringW OPENAI_HTTP_DIAGNOSTIC::FormatErrorLog() const
{
    CStringW strLog;
    strLog.Format(
        L"Transport Library: %s\r\n"
        L"(참고: 본 프로젝트는 libcurl 미사용, WinHTTP 사용)\r\n"
        L"CURLcode: N/A\r\n"
        L"curl_easy_strerror: N/A (WinHTTP 오류로 대체)\r\n"
        L"\r\n"
        L"URL: %s\r\n"
        L"Model: %s\r\n"
        L"Request Body Size: %zu bytes\r\n"
        L"Failure Stage: %s\r\n"
        L"Win32 Error Code: %lu\r\n"
        L"WinHTTP Error Text: %s\r\n"
        L"HTTP Response Code: %d\r\n",
        strTransportLibrary.GetString(),
        strUrl.GetString(),
        strModel.GetString(),
        nRequestBodySize,
        strFailureStage.IsEmpty() ? L"(none)" : strFailureStage.GetString(),
        dwWin32Error,
        strTransportErrorText.IsEmpty() ? L"(none)" : strTransportErrorText.GetString(),
        nHttpStatus);

    return strLog;
}

CStringW OpenAiClient::GetCurrentTimestamp()
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

CStringW OpenAiClient::MaskApiKeyForLog(const CStringW& strApiKey)
{
    if (strApiKey.GetLength() <= 8)
        return L"****";

    return strApiKey.Left(7) + L"****" + strApiKey.Mid(strApiKey.GetLength() - 4);
}

CStringW OpenAiClient::FormatWin32Error(DWORD dwError)
{
    if (dwError == 0)
        return L"(none)";

    if (dwError == ERROR_WINHTTP_TIMEOUT)
        return L"ERROR_WINHTTP_TIMEOUT (요청/응답 대기 시간 초과)";

    if (dwError == ERROR_WINHTTP_CONNECTION_ERROR)
        return L"ERROR_WINHTTP_CONNECTION_ERROR (서버 연결 실패)";

    if (dwError == ERROR_WINHTTP_SECURE_FAILURE)
        return L"ERROR_WINHTTP_SECURE_FAILURE (SSL/TLS 보안 연결 실패)";

    LPWSTR pszBuffer = nullptr;
    const DWORD dwFlags =
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS;

    const DWORD dwLen = FormatMessageW(
        dwFlags,
        nullptr,
        dwError,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&pszBuffer),
        0,
        nullptr);

    CStringW strMessage;
    if (dwLen > 0 && pszBuffer != nullptr)
    {
        strMessage = pszBuffer;
        strMessage.Trim();
    }

    if (pszBuffer != nullptr)
        LocalFree(pszBuffer);

    if (strMessage.IsEmpty())
    {
        strMessage.Format(L"Win32 Error %lu", dwError);
    }

    return strMessage;
}

CStringW OpenAiClient::ExtractAssistantMessage(const std::string& strJson)
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

CStringW OpenAiClient::ExtractApiErrorMessage(const std::string& strJson)
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

CStringW OpenAiClient::BuildPossibleCause(int nHttpStatus, const CStringW& strApiError)
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
        return L"네트워크 연결, 방화벽/프록시 설정 또는 응답 대기 시간 초과를 확인하세요.";

    return L"API Key 오류, Model 설정 오류, 네트워크 문제 또는 크레딧 부족 가능성이 있습니다.";
}

BOOL OpenAiClient::WriteUtf8LogFile(const CStringW& strFilePath, const CStringW& strContent)
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

BOOL OpenAiClient::WriteLogFile(const CStringW& strFileName, const CStringW& strContent)
{
    ScpPaths::EnsureFolder(ScpPaths::GetLogFolder());
    return WriteUtf8LogFile(ScpPaths::GetLogFolder() + strFileName, strContent);
}

BOOL OpenAiClient::WriteErrorLog(const CStringW& strContent)
{
    const CStringW strEntry =
        L"[" + GetCurrentTimestamp() + L"]\r\n" + strContent + L"\r\n\r\n";
    const CStringW strPath = ScpPaths::GetLogFolder() + L"ErrorLog.txt";

    try
    {
        std::ofstream ofs(strPath, std::ios::binary | std::ios::app);
        if (!ofs.is_open())
            return FALSE;

        const std::string strUtf8 = TrainingUtil::CStringWToUtf8(strEntry);
        ofs.write(strUtf8.data(), static_cast<std::streamsize>(strUtf8.size()));
        return TRUE;
    }
    catch (...)
    {
        return FALSE;
    }
}

BOOL OpenAiClient::HttpPostChatCompletion(
    const SCP_OPENAI_CONFIG& config,
    const CStringW& strPrompt,
    int& nHttpStatus,
    std::string& strResponseBody,
    CStringW& strTransportError,
    OPENAI_HTTP_DIAGNOSTIC* pDiagnostic)
{
    nHttpStatus = 0;
    strResponseBody.clear();
    strTransportError.Empty();

    OPENAI_HTTP_DIAGNOSTIC localDiagnostic;
    OPENAI_HTTP_DIAGNOSTIC& diag = (pDiagnostic != nullptr) ? *pDiagnostic : localDiagnostic;

    nlohmann::json jBody;
    jBody["model"] = TrainingUtil::CStringWToUtf8(config.strModel);
    jBody["messages"] = nlohmann::json::array({
        nlohmann::json::object({
            {"role", "user"},
            {"content", TrainingUtil::CStringWToUtf8(strPrompt)}
        })
    });

    const std::string strRequestBody = jBody.dump();
    FillDiagnosticBase(diag, config, strRequestBody.size());

    if (strRequestBody.empty())
    {
        strTransportError = L"요청 본문을 생성할 수 없습니다.";
        SetDiagnosticFailure(diag, L"BuildRequestBody", ERROR_INVALID_DATA);
        return FALSE;
    }

    if (strRequestBody.size() > static_cast<size_t>(MAXDWORD))
    {
        strTransportError = L"요청 본문이 너무 큽니다.";
        SetDiagnosticFailure(diag, L"BuildRequestBody", ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    const DWORD dwBodyLen = static_cast<DWORD>(strRequestBody.size());

    HINTERNET hSession = WinHttpOpen(
        L"TrainingContentPlayer/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (hSession == nullptr)
    {
        const DWORD dwErr = GetLastError();
        strTransportError = L"WinHTTP 세션을 시작할 수 없습니다.";
        SetDiagnosticFailure(diag, L"WinHttpOpen", dwErr);
        return FALSE;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, kOpenAiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConnect == nullptr)
    {
        const DWORD dwErr = GetLastError();
        WinHttpCloseHandle(hSession);
        strTransportError = L"OpenAI 서버에 연결할 수 없습니다.";
        SetDiagnosticFailure(diag, L"WinHttpConnect", dwErr);
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
        const DWORD dwErr = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        strTransportError = L"HTTP 요청을 생성할 수 없습니다.";
        SetDiagnosticFailure(diag, L"WinHttpOpenRequest", dwErr);
        return FALSE;
    }

    SetHttpTimeouts(hRequest);

    const CStringW strHeaders =
        L"Content-Type: application/json\r\n"
        L"Authorization: Bearer " + config.strApiKey + L"\r\n";

    BOOL bResult = WinHttpSendRequest(
        hRequest,
        strHeaders,
        static_cast<DWORD>(-1L),
        WINHTTP_NO_REQUEST_DATA,
        0,
        dwBodyLen,
        0);

    if (bResult)
    {
        DWORD dwWritten = 0;
        bResult = WinHttpWriteData(
            hRequest,
            strRequestBody.data(),
            dwBodyLen,
            &dwWritten);
        if (bResult && dwWritten != dwBodyLen)
            bResult = FALSE;
    }

    if (!bResult)
    {
        const DWORD dwErr = GetLastError();
        strTransportError = L"OpenAI API 요청 전송에 실패했습니다.";
        SetDiagnosticFailure(diag, L"WinHttpSendRequest/WinHttpWriteData", dwErr);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    bResult = WinHttpReceiveResponse(hRequest, nullptr);
    if (!bResult)
    {
        const DWORD dwErr = GetLastError();
        strTransportError = L"OpenAI API 응답 수신에 실패했습니다.";
        SetDiagnosticFailure(diag, L"WinHttpReceiveResponse", dwErr);
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
    diag.nHttpStatus = nHttpStatus;

    std::string strResponse;
    DWORD dwAvailable = 0;
    do
    {
        dwAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable))
        {
            const DWORD dwErr = GetLastError();
            strTransportError = L"OpenAI API 응답 데이터를 읽을 수 없습니다.";
            SetDiagnosticFailure(diag, L"WinHttpQueryDataAvailable", dwErr);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return FALSE;
        }
        if (dwAvailable == 0)
            break;

        std::vector<char> buffer(dwAvailable);
        DWORD dwRead = 0;
        if (!WinHttpReadData(hRequest, buffer.data(), dwAvailable, &dwRead))
        {
            const DWORD dwErr = GetLastError();
            strTransportError = L"OpenAI API 응답 본문을 읽는 중 실패했습니다.";
            SetDiagnosticFailure(diag, L"WinHttpReadData", dwErr);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return FALSE;
        }

        strResponse.append(buffer.data(), buffer.data() + dwRead);
    } while (dwAvailable > 0);

    strResponseBody = std::move(strResponse);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return TRUE;
}
