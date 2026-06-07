#pragma once

#include "PdfRenderEngine.h"

constexpr UINT WM_PDF_VIEWER_BACK_TO_LIST = WM_USER + 103;

class CPdfViewerCtrl : public CWnd
{
    DECLARE_DYNAMIC(CPdfViewerCtrl)

public:
    CPdfViewerCtrl();
    virtual ~CPdfViewerCtrl();

    BOOL CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId);
    BOOL OpenDocument(const CStringW& strPdfPath);
    void CloseDocument();
    BOOL IsDocumentOpen() const;
    BOOL HandleMouseWheel(short zDelta, const CPoint& ptScreen);
    BOOL HandleKeyDown(UINT nChar);

    void SetEmbeddedPreviewMode(BOOL bEmbedded);

protected:
    BOOL m_bEmbeddedPreview;
    PdfRenderEngine::PdfDocument m_doc;
    CStringW m_strPdfPath;
    int m_nPageCount;
    int m_nCurrentPage;

    CArray<HBITMAP> m_arrThumbBitmaps;
    CArray<int> m_arrThumbLruOrder;
    CArray<double> m_arrPageWidths;
    CArray<double> m_arrPageHeights;

    CImage m_pageImage;
    double m_dBaseScale;
    double m_dZoomFactor;
    CPoint m_ptImageTopLeft;
    BOOL m_bDragging;
    CPoint m_ptDragLast;
    int m_nThumbScrollY;
    int m_nPageRenderWidth;

    CButton m_btnBack;
    CButton m_btnPrevPage;
    CButton m_btnNextPage;
    CButton m_btnZoomIn;
    CButton m_btnZoomOut;
    CStatic m_staticPageInfo;
    CScrollBar m_thumbScrollbar;

    CRect m_rcToolbar;
    CRect m_rcThumbPane;
    CRect m_rcThumbContent;
    CRect m_rcThumbScrollBar;
    CRect m_rcPagePane;

    void FreeThumbnails();
    void ResetViewerState();
    void InitThumbCache();
    void EnsureThumbBitmap(int nPage);
    void TouchThumbLru(int nPage);
    void PruneThumbCache();
    void GetVisibleThumbPageRange(int& nFirstPage, int& nLastPage) const;
    void UpdateLayoutRects();
    void LayoutToolbarButtons();
    void LayoutThumbScrollbar();
    void CreateToolbarButtons();
    void UpdatePageInfoLabel();
    int GetPageRenderWidth() const;
    int GetThumbDisplayWidth() const;
    int GetThumbRenderWidth() const;
    int GetThumbItemHeight() const;
    int GetThumbContentHeight() const;
    void ClampThumbScrollY();
    void UpdateThumbScrollbar();
    void ScrollThumbsByPixels(int nDeltaPx);
    void ScrollThumbsByWheel(short zDelta);
    void RenderCurrentPage(BOOL bPreserveZoom = FALSE);
    void SetCurrentPage(int nPage);
    void GoToAdjacentPage(int nDirection);
    void ResetZoomPan();
    void FitPageToWindow();
    void UpdateBaseScale();
    double GetCurrentScale() const;
    void GetDrawSize(const CRect& rcPane, int& nDrawW, int& nDrawH) const;
    void CenterImageInPane(const CRect& rcPane);
    void ZoomAtPoint(double dFactor, const CPoint& ptClient);
    void ZoomInAtCenter();
    void ZoomOutAtCenter();
    BOOL ProcessWheelAction(short zDelta, const CPoint& ptClient, BOOL bCtrlDown);
    void ClampViewPosition();
    CRect GetPageDrawRect(const CRect& rcPane) const;
    int HitTestThumb(const CPoint& ptClient) const;
    void EnsureThumbVisible(int nPage);
    void DrawThumbnails(CDC& dc);
    void DrawPage(CDC& dc);
    void HandleThumbScroll(UINT nSBCode, UINT nPos);

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnBtnBack();
    afx_msg void OnBtnPrevPage();
    afx_msg void OnBtnNextPage();
    afx_msg void OnBtnZoomIn();
    afx_msg void OnBtnZoomOut();

    DECLARE_MESSAGE_MAP()
};
