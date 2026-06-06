#include "pch.h"
#include "PdfRenderEngine.h"
#include "Util.h"
#include "fpdfview.h"
#pragma comment(lib, "pdfium.dll.lib")
namespace
{
    BOOL g_bPdfiumInitialized = FALSE;
    BOOL CopyBitmapToCImage(FPDF_BITMAP bitmap, CImage& outImage)
    {
        if (bitmap == nullptr)
            return FALSE;
        const int nWidth = FPDFBitmap_GetWidth(bitmap);
        const int nHeight = FPDFBitmap_GetHeight(bitmap);
        if (nWidth <= 0 || nHeight <= 0)
            return FALSE;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = nWidth;
        bmi.bmiHeader.biHeight = -nHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        HDC hScreen = ::GetDC(nullptr);
        if (hScreen == nullptr)
            return FALSE;
        void* pBits = nullptr;
        HBITMAP hBitmap = ::CreateDIBSection(
            hScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        ::ReleaseDC(nullptr, hScreen);
        if (hBitmap == nullptr || pBits == nullptr)
            return FALSE;
        const BYTE* pSrc = static_cast<const BYTE*>(FPDFBitmap_GetBuffer(bitmap));
        const int nSrcStride = FPDFBitmap_GetStride(bitmap);
        BYTE* pDst = static_cast<BYTE*>(pBits);
        for (int y = 0; y < nHeight; ++y)
        {
            const BYTE* pSrcRow = pSrc + (y * nSrcStride);
            BYTE* pDstRow = pDst + (y * nWidth * 4);
            for (int x = 0; x < nWidth; ++x)
            {
                const int nSrcOffset = x * 4;
                const int nDstOffset = x * 4;
                pDstRow[nDstOffset + 0] = pSrcRow[nSrcOffset + 2];
                pDstRow[nDstOffset + 1] = pSrcRow[nSrcOffset + 1];
                pDstRow[nDstOffset + 2] = pSrcRow[nSrcOffset + 0];
                pDstRow[nDstOffset + 3] = pSrcRow[nSrcOffset + 3];
            }
        }
        outImage.Destroy();
        outImage.Attach(hBitmap);
        return !outImage.IsNull();
    }
    BOOL RenderLoadedPage(
        FPDF_PAGE page,
        int nBmpWidth,
        CImage& outImage,
        double* pOutPageWidth,
        double* pOutPageHeight)
    {
        outImage.Destroy();
        if (page == nullptr || nBmpWidth < 16)
            return FALSE;
        const double dPageWidth = FPDF_GetPageWidthF(page);
        const double dPageHeight = FPDF_GetPageHeightF(page);
        if (dPageWidth <= 0.0 || dPageHeight <= 0.0)
            return FALSE;
        const int nBmpHeight = max(1, static_cast<int>((dPageHeight / dPageWidth) * nBmpWidth));
        FPDF_BITMAP bitmap = FPDFBitmap_Create(nBmpWidth, nBmpHeight, 0);
        if (bitmap == nullptr)
            return FALSE;
        FPDFBitmap_FillRect(bitmap, 0, 0, nBmpWidth, nBmpHeight, 0xFFFFFFFF);
        const int nRenderFlags = FPDF_ANNOT | FPDF_LCD_TEXT;
        FPDF_RenderPageBitmap(
            bitmap,
            page,
            0,
            0,
            nBmpWidth,
            nBmpHeight,
            0,
            nRenderFlags);
        if (pOutPageWidth != nullptr)
            *pOutPageWidth = dPageWidth;
        if (pOutPageHeight != nullptr)
            *pOutPageHeight = dPageHeight;
        const BOOL bOk = CopyBitmapToCImage(bitmap, outImage);
        FPDFBitmap_Destroy(bitmap);
        return bOk;
    }
}
BOOL PdfRenderEngine::EnsureInitialized()
{
    if (g_bPdfiumInitialized)
        return TRUE;
    FPDF_InitLibrary();
    g_bPdfiumInitialized = TRUE;
    return TRUE;
}
void PdfRenderEngine::Shutdown()
{
    if (!g_bPdfiumInitialized)
        return;
    FPDF_DestroyLibrary();
    g_bPdfiumInitialized = FALSE;
}
BOOL PdfRenderEngine::RenderFirstPage(
    const CStringW& strPdfPath,
    CImage& outImage,
    int nCoverWidth,
    double* pOutPageWidth,
    double* pOutPageHeight)
{
    outImage.Destroy();
    if (strPdfPath.IsEmpty() ||
        ::GetFileAttributesW(strPdfPath) == INVALID_FILE_ATTRIBUTES ||
        nCoverWidth < 64)
    {
        return FALSE;
    }
    if (!EnsureInitialized())
        return FALSE;
    const std::string strUtf8Path = TrainingUtil::CStringWToUtf8(strPdfPath);
    FPDF_DOCUMENT doc = FPDF_LoadDocument(strUtf8Path.c_str(), nullptr);
    if (doc == nullptr)
        return FALSE;
    FPDF_PAGE page = FPDF_LoadPage(doc, 0);
    if (page == nullptr)
    {
        FPDF_CloseDocument(doc);
        return FALSE;
    }
    const BOOL bOk = RenderLoadedPage(page, nCoverWidth, outImage, pOutPageWidth, pOutPageHeight);
    FPDF_ClosePage(page);
    FPDF_CloseDocument(doc);
    return bOk;
}
PdfRenderEngine::PdfDocument::PdfDocument()
    : m_pDoc(nullptr)
{
}
PdfRenderEngine::PdfDocument::~PdfDocument()
{
    Close();
}
void PdfRenderEngine::PdfDocument::Close()
{
    if (m_pDoc != nullptr)
    {
        FPDF_CloseDocument(m_pDoc);
        m_pDoc = nullptr;
    }
    m_strPath.Empty();
}
BOOL PdfRenderEngine::PdfDocument::Open(const CStringW& strPdfPath)
{
    Close();
    if (strPdfPath.IsEmpty() ||
        ::GetFileAttributesW(strPdfPath) == INVALID_FILE_ATTRIBUTES)
    {
        return FALSE;
    }
    if (!EnsureInitialized())
        return FALSE;
    const std::string strUtf8Path = TrainingUtil::CStringWToUtf8(strPdfPath);
    m_pDoc = FPDF_LoadDocument(strUtf8Path.c_str(), nullptr);
    if (m_pDoc == nullptr)
        return FALSE;
    m_strPath = strPdfPath;
    return TRUE;
}
BOOL PdfRenderEngine::PdfDocument::IsOpen() const
{
    return m_pDoc != nullptr;
}
int PdfRenderEngine::PdfDocument::GetPageCount() const
{
    if (m_pDoc == nullptr)
        return 0;
    return FPDF_GetPageCount(m_pDoc);
}
BOOL PdfRenderEngine::PdfDocument::GetPageSize(
    int nPageIndex,
    double& dWidth,
    double& dHeight) const
{
    dWidth = 0.0;
    dHeight = 0.0;
    if (m_pDoc == nullptr || nPageIndex < 0 || nPageIndex >= GetPageCount())
        return FALSE;
    FPDF_PAGE page = FPDF_LoadPage(m_pDoc, nPageIndex);
    if (page == nullptr)
        return FALSE;
    dWidth = FPDF_GetPageWidthF(page);
    dHeight = FPDF_GetPageHeightF(page);
    FPDF_ClosePage(page);
    return dWidth > 0.0 && dHeight > 0.0;
}
BOOL PdfRenderEngine::PdfDocument::RenderPage(
    int nPageIndex,
    int nRenderWidth,
    CImage& outImage) const
{
    if (m_pDoc == nullptr || nPageIndex < 0 || nPageIndex >= GetPageCount() || nRenderWidth < 16)
        return FALSE;
    FPDF_PAGE page = FPDF_LoadPage(m_pDoc, nPageIndex);
    if (page == nullptr)
        return FALSE;
    const BOOL bOk = RenderLoadedPage(page, nRenderWidth, outImage, nullptr, nullptr);
    FPDF_ClosePage(page);
    return bOk;
}
