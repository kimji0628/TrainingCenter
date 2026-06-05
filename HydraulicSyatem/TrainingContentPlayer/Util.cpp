#include "pch.h"
#include "Util.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <ShlObj.h>

// ============================================================================
// Util.cpp - Common utility functions implementation
// ============================================================================

namespace
{
    CFont g_fontKorean;
}

static FILETIME GetNewestJsonWriteTime(const CStringW& strDataFolder)
{
    FILETIME ftNewest = {};
    BOOL bFound = FALSE;

    CStringArray arrFiles;
    TrainingUtil::FindJsonFiles(strDataFolder, arrFiles);

    for (int i = 0; i < arrFiles.GetSize(); ++i)
    {
        WIN32_FILE_ATTRIBUTE_DATA fad = {};
        if (!GetFileAttributesExW(arrFiles[i], GetFileExInfoStandard, &fad))
            continue;

        if (!bFound || CompareFileTime(&fad.ftLastWriteTime, &ftNewest) > 0)
        {
            ftNewest = fad.ftLastWriteTime;
            bFound = TRUE;
        }
    }

    return ftNewest;
}

static void AppendDataFolderCandidate(std::vector<CStringW>& candidates, const CStringW& strPath)
{
    for (const CStringW& existing : candidates)
    {
        if (existing.CompareNoCase(strPath) == 0)
            return;
    }
    candidates.push_back(strPath);
}

CStringW TrainingUtil::GetDataFolder()
{
    CStringW strAppDir = GetAppDirectory();
    std::vector<CStringW> candidates;
    AppendDataFolderCandidate(candidates, strAppDir + L"Data");

    CStringW strParent = strAppDir;
    for (int nLevel = 0; nLevel < 6; ++nLevel)
    {
        if (!strParent.IsEmpty() && strParent[strParent.GetLength() - 1] == L'\\')
            strParent = strParent.Left(strParent.GetLength() - 1);

        int nPos = strParent.ReverseFind(L'\\');
        if (nPos < 0)
            break;

        strParent = strParent.Left(nPos + 1);
        AppendDataFolderCandidate(candidates, strParent + L"Data");
    }

    CStringW strBestFolder;
    FILETIME ftBest = {};
    BOOL bHasBest = FALSE;

    for (const CStringW& strCandidate : candidates)
    {
        CStringArray arrFiles;
        FindJsonFiles(strCandidate, arrFiles);
        if (arrFiles.GetSize() == 0)
            continue;

        FILETIME ftCandidate = GetNewestJsonWriteTime(strCandidate);
        if (!bHasBest || CompareFileTime(&ftCandidate, &ftBest) > 0)
        {
            ftBest = ftCandidate;
            strBestFolder = strCandidate;
            bHasBest = TRUE;
        }
    }

    return bHasBest ? strBestFolder : (strAppDir + L"Data");
}

CStringW TrainingUtil::GetProgressFilePath(const CStringW& strDataFolder)
{
    CStringW strBase = strDataFolder;
    if (!strBase.IsEmpty() && strBase[strBase.GetLength() - 1] == L'\\')
        strBase = strBase.Left(strBase.GetLength() - 1);

    int nPos = strBase.ReverseFind(L'\\');
    CStringW strParent = (nPos >= 0) ? strBase.Left(nPos + 1) : GetAppDirectory();
    return strParent + L"Progress\\Progress.json";
}

CStringW TrainingUtil::GetAppDirectory()
{
    WCHAR szPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(nullptr, szPath, MAX_PATH);

    CStringW strPath(szPath);
    int nPos = strPath.ReverseFind(L'\\');
    if (nPos >= 0)
    {
        strPath = strPath.Left(nPos + 1);
    }
    return strPath;
}

CStringW TrainingUtil::Utf8ToCStringW(const std::string& strUtf8)
{
    if (strUtf8.empty())
        return CStringW();

    // UTF-8 byte sequence -> UTF-16 (Unicode) using Windows API
    int nWideLen = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        strUtf8.data(),
        static_cast<int>(strUtf8.size()),
        nullptr,
        0);

    if (nWideLen <= 0)
        return CStringW();

    CStringW strResult;
    LPWSTR pszBuffer = strResult.GetBuffer(nWideLen);
    int nConverted = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        strUtf8.data(),
        static_cast<int>(strUtf8.size()),
        pszBuffer,
        nWideLen);
    strResult.ReleaseBuffer(nConverted > 0 ? nConverted : 0);

    return strResult;
}

std::string TrainingUtil::CStringWToUtf8(const CStringW& str)
{
    if (str.IsEmpty())
        return std::string();

    int nUtf8Len = WideCharToMultiByte(
        CP_UTF8,
        0,
        str,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);

    if (nUtf8Len <= 0)
        return std::string();

    std::vector<char> buffer(static_cast<size_t>(nUtf8Len));
    WideCharToMultiByte(
        CP_UTF8,
        0,
        str,
        -1,
        buffer.data(),
        nUtf8Len,
        nullptr,
        nullptr);

    return std::string(buffer.data());
}

BOOL TrainingUtil::ReadTextFileUtf8(const CStringW& strFilePath, std::string& strContent)
{
    strContent.clear();

    CFile file;
    if (!file.Open(strFilePath, CFile::modeRead | CFile::shareDenyWrite | CFile::typeBinary))
        return FALSE;

    ULONGLONG nFileSize = file.GetLength();
    if (nFileSize == 0)
    {
        file.Close();
        return TRUE;
    }

    if (nFileSize > static_cast<ULONGLONG>(INT_MAX))
        return FALSE;

    std::vector<char> buffer(static_cast<size_t>(nFileSize));
    UINT nRead = file.Read(buffer.data(), static_cast<UINT>(nFileSize));
    file.Close();

    if (nRead == 0)
        return TRUE;

    size_t nStart = 0;
    // Remove UTF-8 BOM if present
    if (nRead >= 3 &&
        static_cast<unsigned char>(buffer[0]) == 0xEF &&
        static_cast<unsigned char>(buffer[1]) == 0xBB &&
        static_cast<unsigned char>(buffer[2]) == 0xBF)
    {
        nStart = 3;
    }

    strContent.assign(buffer.data() + nStart, nRead - nStart);
    return TRUE;
}

CStringW TrainingUtil::JsonGetStringW(const nlohmann::json& jObject, const char* pszKey)
{
    if (!jObject.contains(pszKey))
        return CStringW();

    const nlohmann::json& jValue = jObject.at(pszKey);
    if (!jValue.is_string())
        return CStringW();

    const std::string& strUtf8 = jValue.get_ref<const std::string&>();
    return Utf8ToCStringW(strUtf8);
}

CStringW TrainingUtil::ResolveAppPath(const CStringW& strRelativePath)
{
    if (strRelativePath.IsEmpty())
        return CStringW();

    CStringW strPath = strRelativePath;
    strPath.Replace(L'/', L'\\');

    if (strPath.GetLength() >= 2 && strPath[1] == L':')
        return strPath;

    if (strPath.GetLength() >= 2 &&
        ((strPath[0] == L'\\' && strPath[1] == L'\\') ||
         (strPath[0] == L'/' && strPath[1] == L'/')))
    {
        return strPath;
    }

    return GetAppDirectory() + strPath;
}

CStringW TrainingUtil::GetPdfFolder()
{
    return GetAppDirectory() + L"Pdf\\";
}

namespace
{
    CStringW NormalizeFolderPath(const CStringW& strFolder)
    {
        CStringW strPath = strFolder;
        if (!strPath.IsEmpty() && strPath[strPath.GetLength() - 1] != L'\\')
            strPath += L'\\';
        return strPath;
    }

    CStringW GetRelativePathFromRoot(const CStringW& strRootFolder, const CStringW& strFullPath)
    {
        CStringW strRoot = NormalizeFolderPath(strRootFolder);
        if (strFullPath.GetLength() >= strRoot.GetLength() &&
            _wcsnicmp(strFullPath, strRoot, strRoot.GetLength()) == 0)
        {
            return strFullPath.Mid(strRoot.GetLength());
        }

        int nPos = strFullPath.ReverseFind(L'\\');
        return (nPos >= 0) ? strFullPath.Mid(nPos + 1) : strFullPath;
    }

    void CollectPdfFilesRecursive(const CStringW& strFolder, CStringArray& arrFiles)
    {
        CStringW strSearch = NormalizeFolderPath(strFolder);

        CFileFind finder;
        BOOL bWorking = finder.FindFile(strSearch + L"*.pdf");
        while (bWorking)
        {
            bWorking = finder.FindNextFile();
            if (!finder.IsDots() && !finder.IsDirectory())
                arrFiles.Add(finder.GetFilePath());
        }
        finder.Close();

        bWorking = finder.FindFile(strSearch + L"*");
        while (bWorking)
        {
            bWorking = finder.FindNextFile();
            if (!finder.IsDots() && finder.IsDirectory())
                CollectPdfFilesRecursive(finder.GetFilePath(), arrFiles);
        }
        finder.Close();
    }

    void SortFilesByRelativePath(
        const CStringW& strRootFolder,
        CStringArray& arrFiles)
    {
        if (arrFiles.GetSize() <= 1)
            return;

        for (int i = 0; i < arrFiles.GetSize() - 1; ++i)
        {
            for (int j = i + 1; j < arrFiles.GetSize(); ++j)
            {
                CStringW strPathI = GetRelativePathFromRoot(strRootFolder, arrFiles[i]);
                CStringW strPathJ = GetRelativePathFromRoot(strRootFolder, arrFiles[j]);

                if (strPathI.CompareNoCase(strPathJ) > 0)
                {
                    CStringW strTemp = arrFiles[i];
                    arrFiles[i] = arrFiles[j];
                    arrFiles[j] = strTemp;
                }
            }
        }
    }
}

void TrainingUtil::FindPdfFiles(const CStringW& strPdfFolder, CStringArray& arrFiles)
{
    arrFiles.RemoveAll();

    if (GetFileAttributesW(strPdfFolder) == INVALID_FILE_ATTRIBUTES)
        return;

    CollectPdfFilesRecursive(strPdfFolder, arrFiles);
    SortFilesByRelativePath(strPdfFolder, arrFiles);
}

namespace
{
    BOOL IsImageFileName(const CStringW& strFileName)
    {
        int nDot = strFileName.ReverseFind(L'.');
        if (nDot < 0)
            return FALSE;

        CStringW strExt = strFileName.Mid(nDot);
        return strExt.CompareNoCase(L".png") == 0 ||
            strExt.CompareNoCase(L".jpg") == 0 ||
            strExt.CompareNoCase(L".jpeg") == 0 ||
            strExt.CompareNoCase(L".gif") == 0 ||
            strExt.CompareNoCase(L".bmp") == 0 ||
            strExt.CompareNoCase(L".webp") == 0;
    }

    void CollectImageFilesRecursive(const CStringW& strFolder, CStringArray& arrFiles)
    {
        CStringW strSearch = NormalizeFolderPath(strFolder);

        CFileFind finder;
        BOOL bWorking = finder.FindFile(strSearch + L"*.*");
        while (bWorking)
        {
            bWorking = finder.FindNextFile();
            if (!finder.IsDots() && !finder.IsDirectory() &&
                IsImageFileName(finder.GetFileName()))
            {
                arrFiles.Add(finder.GetFilePath());
            }
        }
        finder.Close();

        bWorking = finder.FindFile(strSearch + L"*");
        while (bWorking)
        {
            bWorking = finder.FindNextFile();
            if (!finder.IsDots() && finder.IsDirectory())
                CollectImageFilesRecursive(finder.GetFilePath(), arrFiles);
        }
        finder.Close();
    }
}

CStringW TrainingUtil::GetImageFolder()
{
    return GetAppDirectory() + L"Images\\";
}

void TrainingUtil::FindImageFiles(const CStringW& strImageFolder, CStringArray& arrFiles)
{
    arrFiles.RemoveAll();

    if (GetFileAttributesW(strImageFolder) == INVALID_FILE_ATTRIBUTES)
        return;

    CollectImageFilesRecursive(strImageFolder, arrFiles);
    SortFilesByRelativePath(strImageFolder, arrFiles);
}

void TrainingUtil::FindJsonFiles(const CStringW& strDataFolder, CStringArray& arrFiles)
{
    arrFiles.RemoveAll();

    CStringW strSearch = strDataFolder;
    if (strSearch.Right(1) != L"\\")
        strSearch += L"\\";
    strSearch += L"*.json";

    CFileFind finder;
    BOOL bWorking = finder.FindFile(strSearch);
    while (bWorking)
    {
        bWorking = finder.FindNextFile();
        if (!finder.IsDots() && !finder.IsDirectory())
        {
            arrFiles.Add(finder.GetFilePath());
        }
    }
    finder.Close();
}

CStringW TrainingUtil::ExtractYoutubeVideoId(const CStringW& strUrl)
{
    CStringW url = strUrl;
    url.Trim();
    if (url.IsEmpty())
        return CStringW();

    int nEmbedPos = url.Find(L"youtube.com/embed/");
    if (nEmbedPos >= 0)
    {
        CStringW id = url.Mid(nEmbedPos + 18);
        int nSep = id.FindOneOf(L"?&/");
        if (nSep >= 0)
            id = id.Left(nSep);
        return id;
    }

    int nShortPos = url.Find(L"youtu.be/");
    if (nShortPos >= 0)
    {
        CStringW id = url.Mid(nShortPos + 9);
        int nSep = id.FindOneOf(L"?&/");
        if (nSep >= 0)
            id = id.Left(nSep);
        return id;
    }

    int nWatchPos = url.Find(L"watch?v=");
    if (nWatchPos >= 0)
    {
        CStringW id = url.Mid(nWatchPos + 8);
        int nSep = id.FindOneOf(L"?&/");
        if (nSep >= 0)
            id = id.Left(nSep);
        return id;
    }

    int nVPos = url.Find(L"v=");
    if (nVPos >= 0)
    {
        CStringW id = url.Mid(nVPos + 2);
        int nSep = id.FindOneOf(L"?&/");
        if (nSep >= 0)
            id = id.Left(nSep);
        return id;
    }

    return CStringW();
}

CStringW TrainingUtil::GetYoutubeEmbedUrl(const CStringW& strUrl)
{
    CStringW videoId = ExtractYoutubeVideoId(strUrl);
    if (videoId.IsEmpty())
        return CStringW();

    CStringW embedUrl;
    embedUrl.Format(
        L"https://www.youtube.com/embed/%s?autoplay=1&playsinline=1&rel=0&enablejsapi=1"
        L"&origin=https%%3A%%2F%%2Ftrainingcontentplayer.local",
        videoId.GetString());
    return embedUrl;
}

CStringW TrainingUtil::GetYoutubePlayerFolder()
{
    WCHAR szFolder[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, szFolder);
    ::wcscat_s(szFolder, L"TrainingContentPlayer\\YoutubePlayer\\");
    ::SHCreateDirectoryExW(nullptr, szFolder, nullptr);
    return CStringW(szFolder);
}

BOOL TrainingUtil::PrepareYoutubePlayerPage(const CStringW& strUrl, CStringW& strNavigateUrl)
{
    strNavigateUrl.Empty();

    CStringW videoId = ExtractYoutubeVideoId(strUrl);
    if (videoId.IsEmpty())
        return FALSE;

    CStringW strFolder = GetYoutubePlayerFolder();
    CStringW strHtmlPath = strFolder + L"player.html";

    CStringW html;
    html.Format(
        L"<!DOCTYPE html><html><head><meta charset='utf-8'>"
        L"<style>html,body{margin:0;padding:0;width:100%%;height:100%%;background:#000;overflow:hidden;}"
        L"body{position:fixed;inset:0;}"
        L"iframe{position:absolute;top:0;left:0;border:0;width:100%%;height:100%%;}</style></head><body>"
        L"<iframe src='https://www.youtube.com/embed/%s?autoplay=1&playsinline=1&rel=0&enablejsapi=1"
        L"&origin=https%%3A%%2F%%2Ftrainingcontentplayer.local'"
        L" referrerpolicy='strict-origin-when-cross-origin'"
        L" allow='accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share'"
        L" allowfullscreen></iframe></body></html>",
        videoId.GetString());

    try
    {
        std::string strUtf8 = CStringWToUtf8(html);
        CFile file;
        if (!file.Open(strHtmlPath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
            return FALSE;

        file.Write(strUtf8.data(), static_cast<UINT>(strUtf8.size()));
        file.Close();
    }
    catch (...)
    {
        return FALSE;
    }

    strNavigateUrl.Format(
        L"https://trainingcontentplayer.local/player.html?v=%s&t=%lu",
        videoId.GetString(),
        static_cast<unsigned long>(::GetTickCount()));
    return TRUE;
}

CStringW TrainingUtil::GetWebView2BrowserExecutableFolder()
{
    LPWSTR pszVersion = nullptr;
    HRESULT hr = ::GetAvailableCoreWebView2BrowserVersionString(nullptr, &pszVersion);
    if (FAILED(hr) || pszVersion == nullptr || pszVersion[0] == L'\0')
    {
        if (pszVersion != nullptr)
            ::CoTaskMemFree(pszVersion);
        return CStringW();
    }

    CStringW strVersion(pszVersion);
    ::CoTaskMemFree(pszVersion);

    static const WCHAR* kRoots[] = {
        L"C:\\Program Files (x86)\\Microsoft\\EdgeWebView\\Application\\",
        L"C:\\Program Files\\Microsoft\\EdgeWebView\\Application\\"
    };

    for (const WCHAR* pszRoot : kRoots)
    {
        CStringW strFolder(pszRoot);
        strFolder += strVersion;
        if (::GetFileAttributesW(strFolder) != INVALID_FILE_ATTRIBUTES)
            return strFolder;
    }

    return CStringW();
}

void TrainingUtil::ApplyKoreanFont(CWnd* pWnd, int nPointSize)
{
    if (pWnd == nullptr || !::IsWindow(pWnd->GetSafeHwnd()))
        return;

    if (g_fontKorean.GetSafeHandle() == nullptr)
    {
        g_fontKorean.CreatePointFont(nPointSize, L"맑은 고딕");
        if (g_fontKorean.GetSafeHandle() == nullptr)
        {
            g_fontKorean.CreatePointFont(nPointSize, L"Malgun Gothic");
        }
    }

    pWnd->SetFont(&g_fontKorean);
}
