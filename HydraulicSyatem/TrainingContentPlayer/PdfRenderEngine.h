#pragma once

// ============================================================================
// PdfRenderEngine.h - PDFium based first-page rendering (no shell thumbnail)
// ============================================================================

namespace PdfRenderEngine
{
    BOOL EnsureInitialized();
    void Shutdown();

    BOOL RenderFirstPage(
        const CStringW& strPdfPath,
        CImage& outImage,
        int nCoverWidth = 280,
        double* pOutPageWidth = nullptr,
        double* pOutPageHeight = nullptr);
}
