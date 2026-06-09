#pragma once

#include <vector>

struct PDF_PAGE_TEXT
{
    int      nPageNo;
    CStringW strText;

    PDF_PAGE_TEXT()
        : nPageNo(0)
    {
    }
};

using CPdfPageTextArray = std::vector<PDF_PAGE_TEXT>;

namespace PdfTextExtractor
{
    CStringW FormatPagesForPrompt(const CPdfPageTextArray& pages);

    BOOL ExtractPageRange(
        const CStringW& strPdfPath,
        BOOL bAllPages,
        int nPageStart,
        int nPageEnd,
        CPdfPageTextArray& outPages,
        CStringW& outFormattedText,
        CStringW& strError);

    BOOL ExtractPageRangeText(
        const CStringW& strPdfPath,
        BOOL bAllPages,
        int nPageStart,
        int nPageEnd,
        CStringW& outText,
        CStringW& strError);
}
