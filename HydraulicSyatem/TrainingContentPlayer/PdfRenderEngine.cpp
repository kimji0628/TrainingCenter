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

    const double dPageWidth = FPDF_GetPageWidthF(page);
    const double dPageHeight = FPDF_GetPageHeightF(page);
    if (dPageWidth <= 0.0 || dPageHeight <= 0.0)
    {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return FALSE;
    }

    const int nBmpWidth = nCoverWidth;
    const int nBmpHeight = max(1, static_cast<int>((dPageHeight / dPageWidth) * nBmpWidth));

    FPDF_BITMAP bitmap = FPDFBitmap_Create(nBmpWidth, nBmpHeight, 0);
    if (bitmap == nullptr)
    {
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        return FALSE;
    }

    FPDFBitmap_FillRect(bitmap, 0, 0, nBmpWidth, nBmpHeight, 0xFFFFFFFF);
    const int nRenderFlags = FPDF_ANNOT | FPDF_LCD_TEXT | FPDF_RENDER_FORCEHALFTONE;

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
    FPDF_ClosePage(page);
    FPDF_CloseDocument(doc);

    return bOk;
}
