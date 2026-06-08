#include "pch.h"
#include "ScpConfigReader.h"
#include "Util.h"

namespace
{
    CStringW Trim(const CStringW& strText)
    {
        CStringW str = strText;
        str.Trim();
        return str;
    }

    BOOL ParseKeyValueLine(const CStringW& strLine, CStringW& strKey, CStringW& strValue)
    {
        const int nEq = strLine.Find(L'=');
        if (nEq <= 0)
            return FALSE;

        strKey = Trim(strLine.Left(nEq));
        strValue = strLine.Mid(nEq + 1);
        strValue.Trim();
        return !strKey.IsEmpty();
    }

    void ApplyOpenAiField(SCP_OPENAI_CONFIG& config, const CStringW& strKey, const CStringW& strValue)
    {
        if (strKey.CompareNoCase(L"ApiKey") == 0)
            config.strApiKey = strValue;
        else if (strKey.CompareNoCase(L"Model") == 0)
            config.strModel = strValue;
    }
}

CStringW ScpConfigReader::GetConfigFilePath()
{
    return TrainingUtil::ResolveAppPath(L"SCP_Config.txt");
}

BOOL ScpConfigReader::LoadOpenAiConfig(SCP_OPENAI_CONFIG& outConfig, CStringW& strError)
{
    outConfig = SCP_OPENAI_CONFIG();
    strError.Empty();

    const CStringW strPath = GetConfigFilePath();
    if (strPath.IsEmpty())
    {
        strError = L"SCP_Config.txt 파일을 찾을 수 없습니다.";
        return FALSE;
    }

    DWORD dwAttr = GetFileAttributesW(strPath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        strError = L"SCP_Config.txt 파일을 찾을 수 없습니다.";
        return FALSE;
    }

    std::string strUtf8;
    if (!TrainingUtil::ReadTextFileUtf8(strPath, strUtf8))
    {
        strError = L"SCP_Config.txt 파일을 읽을 수 없습니다.";
        return FALSE;
    }

    CStringW strContent = TrainingUtil::Utf8ToCStringW(strUtf8);
    strContent.Replace(L"\r\n", L"\n");
    strContent.Replace(L"\r", L"\n");

    BOOL bInOpenAiSection = FALSE;
    int nPos = 0;
    CStringW strLine = strContent.Tokenize(L"\n", nPos);

    while (!strLine.IsEmpty() || nPos >= 0)
    {
        strLine = Trim(strLine);

        if (!strLine.IsEmpty() && strLine[0] == L'#' )
        {
            if (nPos < 0)
                break;
            strLine = strContent.Tokenize(L"\n", nPos);
            continue;
        }

        if (!strLine.IsEmpty() && strLine[0] == L'[' && strLine[strLine.GetLength() - 1] == L']')
        {
            CStringW strSection = strLine.Mid(1, strLine.GetLength() - 2);
            strSection.Trim();
            bInOpenAiSection = (strSection.CompareNoCase(L"OpenAI") == 0);
        }
        else if (!strLine.IsEmpty())
        {
            CStringW strKey, strValue;
            if (ParseKeyValueLine(strLine, strKey, strValue))
            {
                if (bInOpenAiSection || outConfig.strApiKey.IsEmpty() || outConfig.strModel.IsEmpty())
                    ApplyOpenAiField(outConfig, strKey, strValue);
            }
            else if (outConfig.strApiKey.IsEmpty() &&
                     strLine.GetLength() > 3 &&
                     strLine.Left(3).CompareNoCase(L"sk-") == 0)
            {
                outConfig.strApiKey = strLine;
            }
        }

        if (nPos < 0)
            break;
        strLine = strContent.Tokenize(L"\n", nPos);
    }

    if (!outConfig.IsValid())
    {
        strError = L"OpenAI API Key 또는 Model 설정이 없습니다.";
        return FALSE;
    }

    return TRUE;
}
