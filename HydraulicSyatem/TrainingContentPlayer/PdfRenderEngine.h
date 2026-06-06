#pragma once



#include "fpdfview.h"



// ============================================================================

// PdfRenderEngine.h - PDFium based rendering (no shell thumbnail)

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



    class PdfDocument

    {

    public:

        PdfDocument();

        ~PdfDocument();



        PdfDocument(const PdfDocument&) = delete;

        PdfDocument& operator=(const PdfDocument&) = delete;



        void Close();

        BOOL Open(const CStringW& strPdfPath);

        BOOL IsOpen() const;



        int GetPageCount() const;

        BOOL GetPageSize(int nPageIndex, double& dWidth, double& dHeight) const;

        BOOL RenderPage(int nPageIndex, int nRenderWidth, CImage& outImage) const;



    private:

        FPDF_DOCUMENT m_pDoc;

        CStringW m_strPath;

    };

}


