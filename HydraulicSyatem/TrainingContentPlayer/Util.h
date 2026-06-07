#pragma once

// ============================================================================
// Util.h - Common utility functions (Unicode / CStringW based)
// ============================================================================

namespace TrainingUtil
{
    // Returns application directory path (trailing backslash included)
    CStringW GetAppDirectory();

    // Locate Data folder (prefers most recently modified JSON source)
    CStringW GetDataFolder();

    // Progress.json path relative to the selected Data folder parent
    CStringW GetProgressFilePath(const CStringW& strDataFolder);

    // UTF-8 (std::string) -> CStringW (Unicode) conversion
    CStringW Utf8ToCStringW(const std::string& strUtf8);

    // CStringW (Unicode) -> UTF-8 (std::string) conversion
    std::string CStringWToUtf8(const CStringW& str);

    // Read entire UTF-8 text file into std::string
    BOOL ReadTextFileUtf8(const CStringW& strFilePath, std::string& strContent);

    // Extract UTF-8 JSON string field and convert to CStringW
    CStringW JsonGetStringW(const nlohmann::json& jObject, const char* pszKey);

    // Resolve relative path against application directory
    CStringW ResolveAppPath(const CStringW& strRelativePath);

    // Find all JSON files in Data folder
    void FindJsonFiles(const CStringW& strDataFolder, CStringArray& arrFiles);

    // Pdf folder path (exe dir\\PDF\\)
    CStringW GetPdfFolder();

    // Create ./PDF when missing
    BOOL EnsurePdfFolder();

    // Find all PDF files in PDF folder and subfolders (sorted by relative path)
    void FindPdfFiles(const CStringW& strPdfFolder, CStringArray& arrFiles);

    // Images folder path (Bin\\Images\\)
    CStringW GetImageFolder();

    // Find all image files in Images folder and subfolders (sorted by relative path)
    void FindImageFiles(const CStringW& strImageFolder, CStringArray& arrFiles);

    // Apply Korean-compatible font to a control
    void ApplyKoreanFont(CWnd* pWnd, int nPointSize = 90);

    // Extract YouTube video ID from watch/youtu.be/embed URL
    CStringW ExtractYoutubeVideoId(const CStringW& strUrl);

    // Build direct YouTube embed URL
    CStringW GetYoutubeEmbedUrl(const CStringW& strUrl);

    // Get folder for YouTube player virtual host HTML files
    CStringW GetYoutubePlayerFolder();

    // Write iframe player HTML and return https:// virtual host navigation URL
    BOOL PrepareYoutubePlayerPage(const CStringW& strUrl, CStringW& strNavigateUrl);

    BOOL IsUrlVideo(const CStringW& strVideo);
    BOOL IsSupportedLocalVideo(const CStringW& strVideo);
    BOOL PrepareLocalVideoPlayerPage(const CStringW& strRelativePath, CStringW& strNavigateUrl);
    CStringW GetVideoMediaVirtualHostUrl(const CStringW& strRelativePath);

    // Resolve installed Edge WebView2 runtime folder (empty if not found)
    CStringW GetWebView2BrowserExecutableFolder();
}
