#pragma once

constexpr UINT WM_PDF_COVER_ITEM_SELECTED = WM_USER + 102;

struct PdfCoverGridLayout
{
    int nCols;
    int nRows;
    int nThumbW;
    int nThumbH;
    int nCellW;
    int nCellH;
    int nGridOriginX;
    int nGridOriginY;
};

class CPdfCoverViewCtrl : public CStatic
{
    DECLARE_DYNAMIC(CPdfCoverViewCtrl)

public:
    CPdfCoverViewCtrl();
    virtual ~CPdfCoverViewCtrl();

    void ClearView();
    BOOL CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId);
    void ShowPdfCovers(const CStringArray& arrPdfPaths, const CArray<int>& arrPdfIndices);
    void SetSelectedPdfIndex(int nPdfIndex);
    int GetSelectedPdfIndex() const;

protected:
    CArray<int> m_arrPdfIndices;
    CStringArray m_arrFileNames;
    CArray<HBITMAP> m_arrCoverBitmaps;
    CArray<double> m_arrPageWidths;
    CArray<double> m_arrPageHeights;
    int m_nSelectedPdfIndex;

    CStringArray m_arrStoredPdfPaths;
    CArray<int> m_arrStoredPdfIndices;
    int m_nLastCoverRenderWidth;

    void FreeCoverBitmaps();
    void ResetCoverContent();
    int GetCoverRenderWidth() const;
    void ComputeCoverGridLayout(const CRect& rcClient, int nCount, PdfCoverGridLayout& layout) const;
    int HitTestCard(const CPoint& ptClient) const;
    static CRect GetCardFrameRect(const PdfCoverGridLayout& layout, int nCol, int nRow);
    static CRect GetThumbRect(const CRect& rcCardFrame, const PdfCoverGridLayout& layout);
    static CRect GetLabelRect(const CRect& rcCardFrame);
    static CRect CalcAspectFitRect(const CRect& rcBounds, double dPageW, double dPageH);
    void DrawCoverBitmap(CDC& dc, HBITMAP hCover, const CRect& rcDest);
    void DrawCards(CDC& dc, const CRect& rcClient);

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()
};
