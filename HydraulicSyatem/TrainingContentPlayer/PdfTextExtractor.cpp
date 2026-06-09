#include "pch.h"
#include "PdfTextExtractor.h"
#include "PdfRenderEngine.h"
#include "Util.h"

#include "fpdfview.h"
#include "fpdf_text.h"

namespace
{
    CStringW ExtractTextFromPage(FPDF_PAGE page)
    {
        CStringW strResult;
        if (page == nullptr)
            return strResult;

        FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
        if (textPage == nullptr)
            return strResult;

        const int nCharCount = FPDFText_CountChars(textPage);
        if (nCharCount > 0)
        {
            std::vector<unsigned short> buffer(static_cast<size_t>(nCharCount) + 1, 0);
            const int nWritten = FPDFText_GetText(
                textPage,
                0,
                nCharCount,
                buffer.data());
            if (nWritten > 1)
                strResult = CStringW(reinterpret_cast<const wchar_t*>(buffer.data()), nWritten - 1);
        }

        FPDFText_ClosePage(textPage);
        return strResult;
    }
}

CStringW PdfTextExtractor::FormatPagesForPrompt(const CPdfPageTextArray& pages)
{
    CStringW strCombined;

    for (const PDF_PAGE_TEXT& page : pages)
    {
        CStringW strSection;
        strSection.Format(L"[PAGE:%d]\r\n%s\r\n\r\n", page.nPageNo, page.strText.GetString());
        strCombined += strSection;
    }

    return strCombined;
}

BOOL PdfTextExtractor::ExtractPageRange(
    const CStringW& strPdfPath,
    BOOL bAllPages,
    int nPageStart,
    int nPageEnd,
    CPdfPageTextArray& outPages,
    CStringW& outFormattedText,
    CStringW& strError)
{
    outPages.clear();
    outFormattedText.Empty();
    strError.Empty();

    if (strPdfPath.IsEmpty())
    {
        strError = L"PDF 파일이 선택되지 않았습니다.";
        return FALSE;
    }

    const DWORD dwAttr = GetFileAttributesW(strPdfPath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        strError = L"PDF 파일을 찾을 수 없습니다.\r\n\r\n" + strPdfPath;
        return FALSE;
    }

    if (!PdfRenderEngine::EnsureInitialized())
    {
        strError = L"PDF 엔진을 초기화할 수 없습니다.";
        return FALSE;
    }

    const std::string strUtf8Path = TrainingUtil::CStringWToUtf8(strPdfPath);
    FPDF_DOCUMENT doc = FPDF_LoadDocument(strUtf8Path.c_str(), nullptr);
    if (doc == nullptr)
    {
        strError = L"PDF 파일을 열 수 없습니다.\r\n\r\n" + strPdfPath;
        return FALSE;
    }

    const int nPageCount = FPDF_GetPageCount(doc);
    if (nPageCount <= 0)
    {
        FPDF_CloseDocument(doc);
        strError = L"PDF에 페이지가 없습니다.";
        return FALSE;
    }

    int nStart = nPageStart;
    int nEnd = nPageEnd;
    if (bAllPages)
    {
        nStart = 1;
        nEnd = nPageCount;
    }

    if (nStart < 1)
        nStart = 1;
    if (nEnd > nPageCount)
        nEnd = nPageCount;
    if (nEnd < nStart)
        nEnd = nStart;

    BOOL bAnyText = FALSE;

    for (int nPage = nStart; nPage <= nEnd; ++nPage)
    {
        FPDF_PAGE page = FPDF_LoadPage(doc, nPage - 1);
        if (page == nullptr)
            continue;

        PDF_PAGE_TEXT pageText;
        pageText.nPageNo = nPage;
        pageText.strText = ExtractTextFromPage(page);
        FPDF_ClosePage(page);

        outPages.push_back(pageText);
        if (!pageText.strText.IsEmpty())
            bAnyText = TRUE;
    }

    FPDF_CloseDocument(doc);

    if (outPages.empty())
    {
        strError = L"지정한 페이지 범위에서 페이지를 읽을 수 없습니다.";
        return FALSE;
    }

    if (!bAnyText)
    {
        strError =
            L"지정한 페이지 범위에서 텍스트를 추출할 수 없습니다.\r\n\r\n"
            L"스캔 PDF이거나 텍스트 레이어가 없는 PDF일 수 있습니다.";
        return FALSE;
    }

    outFormattedText = FormatPagesForPrompt(outPages);
    return TRUE;
}

BOOL PdfTextExtractor::ExtractPageRangeText(
    const CStringW& strPdfPath,
    BOOL bAllPages,
    int nPageStart,
    int nPageEnd,
    CStringW& outText,
    CStringW& strError)
{
    CPdfPageTextArray pages;
    return ExtractPageRange(
        strPdfPath,
        bAllPages,
        nPageStart,
        nPageEnd,
        pages,
        outText,
        strError);
}
