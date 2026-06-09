#include "pch.h"
#include "ScpPaths.h"
#include "Util.h"

namespace
{
    BOOL FileExists(const CStringW& strPath)
    {
        if (strPath.IsEmpty())
            return FALSE;

        const DWORD dwAttr = GetFileAttributesW(strPath);
        return dwAttr != INVALID_FILE_ATTRIBUTES && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY);
    }
}

CStringW ScpPaths::GetConfigFolder()
{
    return TrainingUtil::GetAppDirectory() + L"Config\\";
}

CStringW ScpPaths::GetLogFolder()
{
    const CStringW strPreferred = TrainingUtil::GetAppDirectory() + L"Log\\";
    if (EnsureFolder(strPreferred))
        return strPreferred;

    const CStringW strLegacy = TrainingUtil::GetAppDirectory() + L"logs\\";
    EnsureFolder(strLegacy);
    return strLegacy;
}

CStringW ScpPaths::GetContentPdfFolder()
{
    const CStringW strPreferred = TrainingUtil::GetAppDirectory() + L"Content\\PDF\\";
    if (FileExists(strPreferred) || EnsureFolder(strPreferred))
        return strPreferred;

    return TrainingUtil::ResolveAppPath(L"PDF\\");
}

BOOL ScpPaths::EnsureFolder(const CStringW& strFolderPath)
{
    if (strFolderPath.IsEmpty())
        return FALSE;

    if (CreateDirectoryW(strFolderPath, nullptr))
        return TRUE;

    const DWORD dwErr = GetLastError();
    return dwErr == ERROR_ALREADY_EXISTS;
}

CStringW ScpPaths::ResolveConfigFile(const CStringW& strFileName)
{
    const CStringW strPreferred = GetConfigFolder() + strFileName;
    if (FileExists(strPreferred))
        return strPreferred;

    return TrainingUtil::ResolveAppPath(strFileName);
}

CStringW ScpPaths::ResolvePromptFile(const CStringW& strPromptFileName)
{
    return ResolveConfigFile(strPromptFileName);
}

CStringW ScpPaths::ResolveContentPath(const CStringW& strRelativePath)
{
    const CStringW strPreferred =
        TrainingUtil::GetAppDirectory() + L"Content\\" + strRelativePath;
    if (FileExists(strPreferred))
        return strPreferred;

    return TrainingUtil::ResolveAppPath(strRelativePath);
}

CStringW ScpPaths::GetTimestampPrefix()
{
    SYSTEMTIME st = {};
    GetLocalTime(&st);

    CStringW strPrefix;
    strPrefix.Format(
        L"%04d%02d%02d_%02d%02d%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return strPrefix;
}
