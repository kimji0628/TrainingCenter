#include "pch.h"
#include "ImageViewCtrl.h"

IMPLEMENT_DYNAMIC(CImageViewCtrl, CStatic)

namespace
{
    constexpr double DEFAULT_ZOOM_FACTOR = 1.0;
    constexpr double MIN_ZOOM_FACTOR = 0.2;
    constexpr double MAX_ZOOM_FACTOR = 8.0;
    constexpr double ZOOM_STEP = 1.15;

    int RoundToInt(double dValue)
    {
        return static_cast<int>(dValue + 0.5);
    }

    HCURSOR LoadHandCursor()
    {
        HCURSOR hCursor = ::LoadCursor(nullptr, IDC_HAND);
        if (hCursor == nullptr)
            hCursor = ::LoadCursor(nullptr, MAKEINTRESOURCE(32649));
        if (hCursor == nullptr)
            hCursor = ::LoadCursor(nullptr, IDC_SIZEALL);
        return hCursor;
    }
}

CImageViewCtrl::CImageViewCtrl()
    : m_mode(ViewMode::None)
    , m_dBaseScale(1.0)
    , m_dZoomFactor(1.0)
    , m_ptImageTopLeft(0, 0)
    , m_bDragging(FALSE)
{
}

CImageViewCtrl::~CImageViewCtrl()
{
    const HWND hWnd = GetSafeHwnd();
    if (hWnd != nullptr && ::GetCapture() == hWnd)
        ::ReleaseCapture();

    SetBitmap(nullptr);
}

BOOL CImageViewCtrl::CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId)
{
    if (pParent == nullptr || pPlaceholder == nullptr ||
        !::IsWindow(pPlaceholder->GetSafeHwnd()))
    {
        return FALSE;
    }

    CRect rcHost;
    pPlaceholder->GetWindowRect(&rcHost);
    pParent->ScreenToClient(&rcHost);
    pPlaceholder->ShowWindow(SW_HIDE);

    const DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_NOTIFY;

    return Create(
        L"",
        dwStyle,
        rcHost,
        pParent,
        nHostId);
}

BEGIN_MESSAGE_MAP(CImageViewCtrl, CStatic)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

BOOL CImageViewCtrl::IsSingleImageMode() const
{
    return m_mode == ViewMode::Single;
}

void CImageViewCtrl::ClearView()
{
    m_mode = ViewMode::None;
    SetBitmap(nullptr);
    m_image.Destroy();
    m_arrGridPaths.RemoveAll();
    m_arrGridIndices.RemoveAll();
    m_bDragging = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();

    if (GetSafeHwnd() != nullptr)
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

double CImageViewCtrl::GetCurrentScale() const
{
    return m_dBaseScale * m_dZoomFactor;
}

void CImageViewCtrl::GetDrawSize(const CRect& rcClient, int& nDrawW, int& nDrawH) const
{
    nDrawW = 0;
    nDrawH = 0;

    if (m_image.IsNull())
        return;

    const double dScale = GetCurrentScale();
    nDrawW = max(1, RoundToInt(m_image.GetWidth() * dScale));
    nDrawH = max(1, RoundToInt(m_image.GetHeight() * dScale));
}

void CImageViewCtrl::CenterImageInView(const CRect& rcClient)
{
    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(rcClient, nDrawW, nDrawH);
    m_ptImageTopLeft.x = (rcClient.Width() - nDrawW) / 2;
    m_ptImageTopLeft.y = (rcClient.Height() - nDrawH) / 2;
}

void CImageViewCtrl::UpdateBaseScale()
{
    m_dBaseScale = 1.0;
    if (!m_image.IsNull())
    {
        CRect rcClient;
        GetClientRect(&rcClient);
        const int nImgW = m_image.GetWidth();
        const int nImgH = m_image.GetHeight();
        if (nImgW > 0 && nImgH > 0 && rcClient.Width() > 0 && rcClient.Height() > 0)
        {
            m_dBaseScale = min(
                static_cast<double>(rcClient.Width()) / nImgW,
                static_cast<double>(rcClient.Height()) / nImgH);
        }
    }
}

void CImageViewCtrl::ResetZoomPan()
{
    m_dZoomFactor = DEFAULT_ZOOM_FACTOR;

    CRect rcClient;
    GetClientRect(&rcClient);
    UpdateBaseScale();
    CenterImageInView(rcClient);
}

void CImageViewCtrl::ClampViewPosition()
{
    if (m_image.IsNull())
        return;

    CRect rcClient;
    GetClientRect(&rcClient);
    if (rcClient.IsRectEmpty())
        return;

    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(rcClient, nDrawW, nDrawH);

    const int nMinX = min(0, rcClient.Width() - nDrawW);
    const int nMaxX = max(0, rcClient.Width() - nDrawW);
    const int nMinY = min(0, rcClient.Height() - nDrawH);
    const int nMaxY = max(0, rcClient.Height() - nDrawH);

    m_ptImageTopLeft.x = max(nMinX, min(nMaxX, m_ptImageTopLeft.x));
    m_ptImageTopLeft.y = max(nMinY, min(nMaxY, m_ptImageTopLeft.y));
}

CRect CImageViewCtrl::GetSingleImageDrawRect(const CRect& rcClient) const
{
    if (m_image.IsNull())
        return CRect(0, 0, 0, 0);

    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(rcClient, nDrawW, nDrawH);

    return CRect(
        m_ptImageTopLeft.x,
        m_ptImageTopLeft.y,
        m_ptImageTopLeft.x + nDrawW,
        m_ptImageTopLeft.y + nDrawH);
}

void CImageViewCtrl::ZoomAtPoint(double dFactor, const CPoint& ptClient)
{
    if (m_image.IsNull())
        return;

    CRect rcClient;
    GetClientRect(&rcClient);

    const double dOldScale = GetCurrentScale();
    CRect rcDraw = GetSingleImageDrawRect(rcClient);
    if (rcDraw.Width() <= 0 || rcDraw.Height() <= 0 || dOldScale <= 0.0)
        return;

    const double dImgX = (ptClient.x - rcDraw.left) / dOldScale;
    const double dImgY = (ptClient.y - rcDraw.top) / dOldScale;

    m_dZoomFactor *= dFactor;
    m_dZoomFactor = max(MIN_ZOOM_FACTOR, min(MAX_ZOOM_FACTOR, m_dZoomFactor));

    const double dNewScale = GetCurrentScale();
    m_ptImageTopLeft.x = ptClient.x - RoundToInt(dImgX * dNewScale);
    m_ptImageTopLeft.y = ptClient.y - RoundToInt(dImgY * dNewScale);
    ClampViewPosition();
}

BOOL CImageViewCtrl::HandleMouseWheel(short zDelta, const CPoint& ptScreen)
{
    if (m_mode != ViewMode::Single || m_image.IsNull())
        return FALSE;

    CRect rcWindow;
    GetWindowRect(&rcWindow);
    if (!rcWindow.PtInRect(ptScreen))
        return FALSE;

    CPoint ptClient = ptScreen;
    ScreenToClient(&ptClient);

    const double dFactor = (zDelta > 0) ? ZOOM_STEP : (1.0 / ZOOM_STEP);
    ZoomAtPoint(dFactor, ptClient);
    Invalidate();
    return TRUE;
}

void CImageViewCtrl::ShowSingleImage(const CStringW& strImagePath)
{
    m_mode = ViewMode::None;
    SetBitmap(nullptr);
    m_image.Destroy();
    m_arrGridPaths.RemoveAll();
    m_arrGridIndices.RemoveAll();
    m_bDragging = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();

    if (strImagePath.IsEmpty() || GetSafeHwnd() == nullptr)
    {
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        return;
    }

    if (FAILED(m_image.Load(strImagePath)))
    {
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        return;
    }

    m_mode = ViewMode::Single;
    ResetZoomPan();
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void CImageViewCtrl::ShowSingleImagePreview(CImage& image)
{
    m_mode = ViewMode::None;
    SetBitmap(nullptr);
    m_image.Destroy();
    m_arrGridPaths.RemoveAll();
    m_arrGridIndices.RemoveAll();
    m_bDragging = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();

    if (image.IsNull() || GetSafeHwnd() == nullptr)
    {
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
        return;
    }

    HBITMAP hBitmap = image.Detach();
    m_image.Attach(hBitmap);
    m_mode = ViewMode::Single;
    ResetZoomPan();
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

void CImageViewCtrl::ShowImageGrid(
    const CStringArray& arrImagePaths,
    const CArray<int>& arrIndices)
{
    m_mode = ViewMode::None;
    SetBitmap(nullptr);
    m_image.Destroy();
    m_arrGridPaths.RemoveAll();
    m_arrGridIndices.RemoveAll();
    m_bDragging = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();

    for (int i = 0; i < arrIndices.GetSize(); ++i)
        m_arrGridIndices.Add(arrIndices[i]);

    m_arrGridPaths.Copy(arrImagePaths);
    m_mode = ViewMode::Grid;

    if (GetSafeHwnd() != nullptr)
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}

BOOL CImageViewCtrl::GetGridImageDrawRect(int nGridSlot, const CRect& rcClient, CRect& rcDraw) const
{
    rcDraw.SetRectEmpty();

    const int nCount = static_cast<int>(m_arrGridIndices.GetSize());
    if (nGridSlot < 0 || nGridSlot >= nCount)
        return FALSE;

    const int nImageIndex = m_arrGridIndices[nGridSlot];
    if (nImageIndex < 0 || nImageIndex >= m_arrGridPaths.GetSize())
        return FALSE;

    CImage image;
    if (FAILED(image.Load(m_arrGridPaths[nImageIndex])))
        return FALSE;

    const int nImgW = image.GetWidth();
    const int nImgH = image.GetHeight();
    if (nImgW <= 0 || nImgH <= 0)
        return FALSE;

    int nCols = 1;
    while (nCols * nCols < nCount)
        ++nCols;

    const int nRows = (nCount + nCols - 1) / nCols;
    const int nCellW = max(1, rcClient.Width() / nCols);
    const int nCellH = max(1, rcClient.Height() / nRows);
    const int nPadding = 4;

    const int nCol = nGridSlot % nCols;
    const int nRow = nGridSlot / nCols;
    const int nCellLeft = nCol * nCellW + nPadding;
    const int nCellTop = nRow * nCellH + nPadding;
    const int nAvailW = max(1, nCellW - (nPadding * 2));
    const int nAvailH = max(1, nCellH - (nPadding * 2));

    const double dScale = min(
        static_cast<double>(nAvailW) / nImgW,
        static_cast<double>(nAvailH) / nImgH);
    const int nDrawW = max(1, static_cast<int>(nImgW * dScale));
    const int nDrawH = max(1, static_cast<int>(nImgH * dScale));
    const int nOffsetX = nCellLeft + (nAvailW - nDrawW) / 2;
    const int nOffsetY = nCellTop + (nAvailH - nDrawH) / 2;

    rcDraw.SetRect(nOffsetX, nOffsetY, nOffsetX + nDrawW, nOffsetY + nDrawH);
    return TRUE;
}

int CImageViewCtrl::HitTestGridImage(const CPoint& ptClient) const
{
    if (m_mode != ViewMode::Grid)
        return -1;

    CRect rcClient;
    GetClientRect(&rcClient);

    const int nCount = static_cast<int>(m_arrGridIndices.GetSize());
    for (int i = 0; i < nCount; ++i)
    {
        CRect rcDraw;
        if (GetGridImageDrawRect(i, rcClient, rcDraw) && rcDraw.PtInRect(ptClient))
            return i;
    }

    return -1;
}

void CImageViewCtrl::DrawSingleImage(CDC& dc, const CRect& rcClient)
{
    if (m_image.IsNull())
        return;

    CRgn clipRgn;
    clipRgn.CreateRectRgnIndirect(&rcClient);
    dc.SelectClipRgn(&clipRgn);

    CRect rcDraw = GetSingleImageDrawRect(rcClient);
    CRect rcDest;
    if (!rcDest.IntersectRect(&rcDraw, &rcClient))
    {
        dc.SelectClipRgn(nullptr);
        return;
    }

    const double dScale = GetCurrentScale();
    if (dScale <= 0.0)
    {
        dc.SelectClipRgn(nullptr);
        return;
    }

    const int nImgW = m_image.GetWidth();
    const int nImgH = m_image.GetHeight();
    int nSrcX = RoundToInt((rcDest.left - rcDraw.left) / dScale);
    int nSrcY = RoundToInt((rcDest.top - rcDraw.top) / dScale);
    int nSrcW = max(1, RoundToInt(rcDest.Width() / dScale));
    int nSrcH = max(1, RoundToInt(rcDest.Height() / dScale));

    nSrcX = max(0, min(nImgW - 1, nSrcX));
    nSrcY = max(0, min(nImgH - 1, nSrcY));
    nSrcW = max(1, min(nImgW - nSrcX, nSrcW));
    nSrcH = max(1, min(nImgH - nSrcY, nSrcH));

    m_image.StretchBlt(
        dc.m_hDC,
        rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
        nSrcX, nSrcY, nSrcW, nSrcH,
        SRCCOPY);

    dc.SelectClipRgn(nullptr);
}

void CImageViewCtrl::DrawImageGrid(CDC& dc, const CRect& rcClient)
{
    CRgn clipRgn;
    clipRgn.CreateRectRgnIndirect(&rcClient);
    dc.SelectClipRgn(&clipRgn);

    const int nCount = static_cast<int>(m_arrGridIndices.GetSize());
    for (int i = 0; i < nCount; ++i)
    {
        const int nImageIndex = m_arrGridIndices[i];
        if (nImageIndex < 0 || nImageIndex >= m_arrGridPaths.GetSize())
            continue;

        CRect rcDraw;
        if (!GetGridImageDrawRect(i, rcClient, rcDraw))
            continue;

        CRect rcDest;
        if (!rcDest.IntersectRect(&rcDraw, &rcClient))
            continue;

        CImage image;
        if (FAILED(image.Load(m_arrGridPaths[nImageIndex])))
            continue;

        const int nImgW = image.GetWidth();
        const int nImgH = image.GetHeight();
        if (nImgW <= 0 || nImgH <= 0)
            continue;

        const double dScaleX = static_cast<double>(rcDraw.Width()) / nImgW;
        const double dScaleY = static_cast<double>(rcDraw.Height()) / nImgH;
        const double dScale = min(dScaleX, dScaleY);

        int nSrcX = RoundToInt((rcDest.left - rcDraw.left) / dScale);
        int nSrcY = RoundToInt((rcDest.top - rcDraw.top) / dScale);
        int nSrcW = max(1, RoundToInt(rcDest.Width() / dScale));
        int nSrcH = max(1, RoundToInt(rcDest.Height() / dScale));

        nSrcX = max(0, min(nImgW - 1, nSrcX));
        nSrcY = max(0, min(nImgH - 1, nSrcY));
        nSrcW = max(1, min(nImgW - nSrcX, nSrcW));
        nSrcH = max(1, min(nImgH - nSrcY, nSrcH));

        image.StretchBlt(
            dc.m_hDC,
            rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
            nSrcX, nSrcY, nSrcW, nSrcH,
            SRCCOPY);
    }

    dc.SelectClipRgn(nullptr);
}

void CImageViewCtrl::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);
    dc.FillSolidRect(&rcClient, RGB(255, 255, 255));

    if (m_mode == ViewMode::Single)
    {
        if (!m_bDragging)
            ClampViewPosition();

        DrawSingleImage(dc, rcClient);
    }
    else if (m_mode == ViewMode::Grid)
        DrawImageGrid(dc, rcClient);
}

BOOL CImageViewCtrl::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CImageViewCtrl::OnSize(UINT nType, int cx, int cy)
{
    CStatic::OnSize(nType, cx, cy);

    if (m_mode == ViewMode::Single)
    {
        UpdateBaseScale();
        ClampViewPosition();
        Invalidate();
    }
    else if (m_mode == ViewMode::Grid)
        Invalidate();
}

void CImageViewCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (m_mode == ViewMode::Grid)
    {
        const int nGridSlot = HitTestGridImage(point);
        if (nGridSlot >= 0 && nGridSlot < m_arrGridIndices.GetSize())
        {
            const int nImageIndex = m_arrGridIndices[nGridSlot];
            CWnd* pParent = GetParent();
            if (pParent != nullptr)
            {
                pParent->SendMessage(
                    WM_IMAGE_VIEW_ITEM_SELECTED,
                    static_cast<WPARAM>(nImageIndex),
                    0);
            }
        }
    }
    else if (m_mode == ViewMode::Single && !m_image.IsNull())
    {
        m_bDragging = TRUE;
        m_ptDragLast = point;
        SetCapture();
        SetFocus();
    }

    CStatic::OnLButtonDown(nFlags, point);
}

void CImageViewCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        m_bDragging = FALSE;
        if (GetCapture() == this)
            ReleaseCapture();

        ClampViewPosition();
        Invalidate();
    }

    CStatic::OnLButtonUp(nFlags, point);
}

void CImageViewCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging && (nFlags & MK_LBUTTON))
    {
        CPoint ptDelta = point - m_ptDragLast;
        if (ptDelta.x != 0 || ptDelta.y != 0)
        {
            m_ptImageTopLeft += ptDelta;
            m_ptDragLast = point;
            ClampViewPosition();
            Invalidate();
        }
    }

    CStatic::OnMouseMove(nFlags, point);
}

BOOL CImageViewCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest == HTCLIENT)
    {
        if (m_mode == ViewMode::Single && !m_image.IsNull())
        {
            HCURSOR hCursor = m_bDragging
                ? ::LoadCursor(nullptr, IDC_SIZEALL)
                : LoadHandCursor();
            if (hCursor != nullptr)
            {
                ::SetCursor(hCursor);
                return TRUE;
            }
        }
        else if (m_mode == ViewMode::Grid)
        {
            CPoint pt;
            GetCursorPos(&pt);
            ScreenToClient(&pt);

            if (HitTestGridImage(pt) >= 0)
            {
                HCURSOR hCursor = LoadHandCursor();
                if (hCursor != nullptr)
                {
                    ::SetCursor(hCursor);
                    return TRUE;
                }
            }
        }
    }

    return CStatic::OnSetCursor(pWnd, nHitTest, message);
}

BOOL CImageViewCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (HandleMouseWheel(zDelta, pt))
        return TRUE;

    return CStatic::OnMouseWheel(nFlags, zDelta, pt);
}
