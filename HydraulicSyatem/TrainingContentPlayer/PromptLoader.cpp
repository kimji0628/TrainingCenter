#include "pch.h"
#include "PromptLoader.h"
#include "ScpPaths.h"
#include "Util.h"

namespace
{
    CStringW ReplaceAll(const CStringW& strSource, const CStringW& strFind, const CStringW& strReplace)
    {
        if (strFind.IsEmpty())
            return strSource;

        CStringW strResult = strSource;
        int nPos = 0;
        while ((nPos = strResult.Find(strFind, nPos)) >= 0)
        {
            strResult.Delete(nPos, strFind.GetLength());
            strResult.Insert(nPos, strReplace);
            nPos += strReplace.GetLength();
        }
        return strResult;
    }

    int FindPlaceholder(const CStringW& strText, const CStringW& strName, int nStart)
    {
        const CStringW strBrace = L"{" + strName + L"}";
        int nPos = strText.Find(strBrace, nStart);
        if (nPos >= 0)
            return nPos;

        return strText.Find(L"{{" + strName + L"}}", nStart);
    }

    void ReplacePlaceholder(CStringW& strText, const CStringW& strName, const CStringW& strValue)
    {
        const CStringW arrFind[] = {
            L"{" + strName + L"}",
            L"{{" + strName + L"}}"
        };

        for (const CStringW& strFind : arrFind)
            strText = ReplaceAll(strText, strFind, strValue);
    }
}

CStringW PromptLoader::GetQuestionPromptFilePath()
{
    return ScpPaths::ResolvePromptFile(L"Prompt_Question.txt");
}

BOOL PromptLoader::LoadPromptFile(const CStringW& strFilePath, CStringW& outContent, CStringW& strError)
{
    outContent.Empty();
    strError.Empty();

    if (strFilePath.IsEmpty())
    {
        strError = L"프롬프트 파일 경로가 비어 있습니다.";
        return FALSE;
    }

    const DWORD dwAttr = GetFileAttributesW(strFilePath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        strError = L"프롬프트 파일을 찾을 수 없습니다.\r\n\r\n" + strFilePath;
        return FALSE;
    }

    std::string strUtf8;
    if (!TrainingUtil::ReadTextFileUtf8(strFilePath, strUtf8))
    {
        strError = L"프롬프트 파일을 읽을 수 없습니다.\r\n\r\n" + strFilePath;
        return FALSE;
    }

    outContent = TrainingUtil::Utf8ToCStringW(strUtf8);
    if (outContent.IsEmpty())
    {
        strError = L"프롬프트 파일 내용이 비어 있습니다.";
        return FALSE;
    }

    return TRUE;
}

CStringW PromptLoader::BuildQuestionGenerationPrompt(
    const CStringW& strTemplate,
    const CStringW& strPdfText,
    int nQuestionCount,
    int nPageStart,
    int nPageEnd,
    const CStringW& strPdfFileName)
{
    CStringW strPrompt = strTemplate;

    CStringW strCount, strStart, strEnd;
    strCount.Format(L"%d", nQuestionCount);
    strStart.Format(L"%d", nPageStart);
    strEnd.Format(L"%d", nPageEnd);

    ReplacePlaceholder(strPrompt, L"QUESTION_COUNT", strCount);
    ReplacePlaceholder(strPrompt, L"PDF_TEXT", strPdfText);
    ReplacePlaceholder(strPrompt, L"PAGE_START", strStart);
    ReplacePlaceholder(strPrompt, L"PAGE_END", strEnd);
    ReplacePlaceholder(strPrompt, L"PDF_FILE", strPdfFileName);

    if (FindPlaceholder(strPrompt, L"QUESTION_COUNT", 0) < 0 &&
        FindPlaceholder(strPrompt, L"PDF_TEXT", 0) < 0)
    {
        CStringW strAppend;
        strAppend.Format(
            L"\r\n\r\n---\r\n"
            L"요청 문제 수: %d\r\n"
            L"PDF 파일: %s\r\n"
            L"페이지 범위: %d ~ %d\r\n"
            L"---\r\n\r\n"
            L"[PDF 텍스트]\r\n%s",
            nQuestionCount,
            strPdfFileName.GetString(),
            nPageStart,
            nPageEnd,
            strPdfText.GetString());
        strPrompt += strAppend;
    }

    return strPrompt;
}
