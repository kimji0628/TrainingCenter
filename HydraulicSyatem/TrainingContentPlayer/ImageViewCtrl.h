#pragma once

constexpr UINT WM_IMAGE_VIEW_ITEM_SELECTED = WM_USER + 101;

class CImageViewCtrl : public CStatic
{
    DECLARE_DYNAMIC(CImageViewCtrl)

public:
    CImageViewCtrl();
    virtual ~CImageViewCtrl();

    void ClearView();
    void ShowImageGrid(const CStringArray& arrImagePaths, const CArray<int>& arrIndices);
    void ShowSingleImage(const CStringW& strImagePath);
    void ShowSingleImagePreview(CImage& image);
    BOOL IsSingleImageMode() const;
    BOOL HandleMouseWheel(short zDelta, const CPoint& ptScreen);
    BOOL CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId);

protected:
    enum class ViewMode { None, Grid, Single };

    ViewMode m_mode;
    CImage m_image;
    CStringArray m_arrGridPaths;
    CArray<int> m_arrGridIndices;

    double m_dBaseScale;
    double m_dZoomFactor;
    CPoint m_ptImageTopLeft;
    BOOL m_bDragging;
    CPoint m_ptDragLast;

    void ResetZoomPan();
    void UpdateBaseScale();
    double GetCurrentScale() const;
    void GetDrawSize(const CRect& rcClient, int& nDrawW, int& nDrawH) const;
    void CenterImageInView(const CRect& rcClient);
    void ZoomAtPoint(double dFactor, const CPoint& ptClient);
    void ClampViewPosition();
    CRect GetSingleImageDrawRect(const CRect& rcClient) const;

    BOOL GetGridImageDrawRect(int nGridSlot, const CRect& rcClient, CRect& rcDraw) const;
    int HitTestGridImage(const CPoint& ptClient) const;
    void DrawSingleImage(CDC& dc, const CRect& rcClient);
    void DrawImageGrid(CDC& dc, const CRect& rcClient);

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);

    DECLARE_MESSAGE_MAP()
};
