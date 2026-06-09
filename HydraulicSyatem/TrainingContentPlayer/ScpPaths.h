#pragma once

namespace ScpPaths
{
    CStringW GetConfigFolder();
    CStringW GetLogFolder();
    CStringW GetContentPdfFolder();

    BOOL EnsureFolder(const CStringW& strFolderPath);

    CStringW ResolveConfigFile(const CStringW& strFileName);
    CStringW ResolvePromptFile(const CStringW& strPromptFileName);
    CStringW ResolveContentPath(const CStringW& strRelativePath);

    CStringW GetTimestampPrefix();
}
