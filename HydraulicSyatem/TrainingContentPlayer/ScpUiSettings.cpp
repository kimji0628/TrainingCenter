#include "pch.h"
#include "ScpUiSettings.h"
#include "ScpPaths.h"
#include "Util.h"

#include <fstream>

namespace
{
    CStringW Trim(const CStringW& strText)
    {
        CStringW str = strText;
        str.Trim();
        return str;
    }

    BOOL ParseRatioValue(const CStringW& strValue, double& outRatio)
    {
        const CStringW strTrimmed = Trim(strValue);
        if (strTrimmed.IsEmpty())
            return FALSE;

        outRatio = _wtof(strTrimmed);
        if (outRatio <= 0.0 || outRatio >= 1.0)
            return FALSE;

        return TRUE;
    }

    BOOL ReadSettingValue(const CStringW& strKey, double& outRatio)
    {
        outRatio = 0.0;

        const CStringW strPath = ScpUiSettings::GetSettingsFilePath();
        if (strPath.IsEmpty())
            return FALSE;

        DWORD dwAttr = GetFileAttributesW(strPath);
        if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
            return FALSE;

        std::string strUtf8;
        if (!TrainingUtil::ReadTextFileUtf8(strPath, strUtf8))
            return FALSE;

        CStringW strContent = TrainingUtil::Utf8ToCStringW(strUtf8);
        strContent.Replace(L"\r\n", L"\n");
        strContent.Replace(L"\r", L"\n");

        int nPos = 0;
        CStringW strLine = strContent.Tokenize(L"\n", nPos);
        while (!strLine.IsEmpty() || nPos >= 0)
        {
            strLine = Trim(strLine);
            if (!strLine.IsEmpty() && strLine[0] != L'#' && strLine[0] != L'[')
            {
                const int nEq = strLine.Find(L'=');
                if (nEq > 0)
                {
                    const CStringW strLineKey = Trim(strLine.Left(nEq));
                    if (strLineKey.CompareNoCase(strKey) == 0)
                        return ParseRatioValue(strLine.Mid(nEq + 1), outRatio);
                }
            }

            if (nPos < 0)
                break;
            strLine = strContent.Tokenize(L"\n", nPos);
        }

        return FALSE;
    }

    BOOL WriteSettingsFile(double dGeneratedRatio, double dPdfRatio)
    {
        CStringW strContent;
        strContent.Format(
            L"# SCP UI Settings\r\n"
            L"[QuizGen]\r\n"
            L"GeneratedListSplitRatio=%.4f\r\n"
            L"PdfSplitRatio=%.4f\r\n",
            dGeneratedRatio,
            dPdfRatio);

        const CStringW strPath = ScpUiSettings::GetSettingsFilePath();
        ScpPaths::EnsureFolder(ScpPaths::GetConfigFolder());

        try
        {
            std::ofstream ofs(strPath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
                return FALSE;

            const std::string strUtf8 = TrainingUtil::CStringWToUtf8(strContent);
            ofs.write(strUtf8.data(), static_cast<std::streamsize>(strUtf8.size()));
            return TRUE;
        }
        catch (...)
        {
            return FALSE;
        }
    }
}

CStringW ScpUiSettings::GetSettingsFilePath()
{
    return ScpPaths::GetConfigFolder() + L"SCP_UI_Settings.txt";
}

BOOL ScpUiSettings::LoadGeneratedListSplitRatio(double& outRatio)
{
    outRatio = kGeneratedListSplitDefault;
    double dLoaded = 0.0;
    if (!ReadSettingValue(L"GeneratedListSplitRatio", dLoaded))
        return FALSE;

    outRatio = dLoaded;
    return TRUE;
}

BOOL ScpUiSettings::LoadPdfSplitRatio(double& outRatio)
{
    outRatio = kPdfSplitDefault;
    double dLoaded = 0.0;
    if (!ReadSettingValue(L"PdfSplitRatio", dLoaded))
        return FALSE;

    outRatio = dLoaded;
    return TRUE;
}

BOOL ScpUiSettings::SaveQuizGenLayout(double dGeneratedListSplitRatio, double dPdfSplitRatio)
{
    dGeneratedListSplitRatio = max(0.05, min(0.95, dGeneratedListSplitRatio));
    dPdfSplitRatio = max(0.05, min(0.95, dPdfSplitRatio));
    return WriteSettingsFile(dGeneratedListSplitRatio, dPdfSplitRatio);
}
