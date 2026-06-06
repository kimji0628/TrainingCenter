#include "pch.h"
#include "PdfViewerCtrl.h"
#include "Resource.h"
#include "Util.h"
IMPLEMENT_DYNAMIC(CPdfViewerCtrl, CWnd)
namespace
{
    constexpr int TOOLBAR_HEIGHT = 36;
    constexpr int TOOLBAR_GAP = 8;
    constexpr int THUMB_PANE_WIDTH = 132;
    constexpr int PANE_GAP = 8;
    constexpr int THUMB_GAP = 8;
    constexpr int THUMB_LABEL_HEIGHT = 18;
    constexpr int THUMB_MIN_RENDER_WIDTH = 72;
    constexpr double THUMB_RENDER_SUPERSAMPLE = 2.5;
    constexpr int PAGE_RENDER_MIN_WIDTH = 480;
    constexpr int PAGE_RENDER_MAX_WIDTH = 4096;
    constexpr double PAGE_RENDER_SUPERSAMPLE = 3.0;
    constexpr double DEFAULT_ZOOM_FACTOR = 1.0;
    constexpr double MIN_ZOOM_FACTOR = 0.2;
    constexpr double MAX_ZOOM_FACTOR = 8.0;
    constexpr double ZOOM_STEP = 1.15;
    constexpr double A4_ASPECT_WH = 210.0 / 297.0;
    int RoundToInt(double dValue)
    {
        return static_cast<int>(dValue + 0.5);
    }
    BOOL IsCtrlDown()
    {
        return (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    }
}
CPdfViewerCtrl::CPdfViewerCtrl()
    : m_nPageCount(0)
    , m_nCurrentPage(-1)
    , m_dBaseScale(1.0)
    , m_dZoomFactor(DEFAULT_ZOOM_FACTOR)
    , m_ptImageTopLeft(0, 0)
    , m_bDragging(FALSE)
    , m_nThumbScrollY(0)
    , m_nPageRenderWidth(0)
{
}
CPdfViewerCtrl::~CPdfViewerCtrl()
{
    CloseDocument();
}
BOOL CPdfViewerCtrl::CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId)
{
    if (pParent == nullptr || pPlaceholder == nullptr ||
        !::IsWindow(pPlaceholder->GetSafeHwnd()))
    {
        return FALSE;
    }
    CRect rcHost;
    pPlaceholder->GetWindowRect(&rcHost);
    pParent->ScreenToClient(&rcHost);
    const DWORD dwStyle = WS_CHILD | WS_CLIPSIBLINGS;
    return Create(
        nullptr,
        nullptr,
        dwStyle,
        rcHost,
        pParent,
        nHostId);
}
int CPdfViewerCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;
    CreateToolbarButtons();
    return 0;
}
void CPdfViewerCtrl::CreateToolbarButtons()
{
    const DWORD dwStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    m_btnBack.Create(L"목록으로 돌아가기", dwStyle, CRect(0, 0, 0, 0), this, IDC_PDF_BTN_BACK);
    m_btnPrevPage.Create(L"이전 페이지", dwStyle, CRect(0, 0, 0, 0), this, IDC_PDF_BTN_PREV_PAGE);
    m_btnNextPage.Create(L"다음 페이지", dwStyle, CRect(0, 0, 0, 0), this, IDC_PDF_BTN_NEXT_PAGE);
    m_btnZoomIn.Create(L"확대(+)", dwStyle, CRect(0, 0, 0, 0), this, IDC_PDF_BTN_ZOOM_IN);
    m_btnZoomOut.Create(L"축소(-)", dwStyle, CRect(0, 0, 0, 0), this, IDC_PDF_BTN_ZOOM_OUT);
    TrainingUtil::ApplyKoreanFont(&m_btnBack);
    TrainingUtil::ApplyKoreanFont(&m_btnPrevPage);
    TrainingUtil::ApplyKoreanFont(&m_btnNextPage);
    TrainingUtil::ApplyKoreanFont(&m_btnZoomIn);
    TrainingUtil::ApplyKoreanFont(&m_btnZoomOut);

    m_staticPageInfo.Create(L"", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER,
        CRect(0, 0, 0, 0), this, IDC_PDF_STATIC_PAGE_INFO);
    TrainingUtil::ApplyKoreanFont(&m_staticPageInfo);
}
void CPdfViewerCtrl::FreeThumbnails()
{
    for (int i = 0; i < m_arrThumbBitmaps.GetSize(); ++i)
    {
        if (m_arrThumbBitmaps[i] != nullptr)
            ::DeleteObject(m_arrThumbBitmaps[i]);
    }
    m_arrThumbBitmaps.RemoveAll();
}
void CPdfViewerCtrl::ResetViewerState()
{
    m_doc.Close();
    m_strPdfPath.Empty();
    m_nPageCount = 0;
    m_nCurrentPage = -1;
    m_nThumbScrollY = 0;
    m_nPageRenderWidth = 0;
    FreeThumbnails();
    m_arrPageWidths.RemoveAll();
    m_arrPageHeights.RemoveAll();
    m_pageImage.Destroy();
    m_dBaseScale = 1.0;
    m_dZoomFactor = DEFAULT_ZOOM_FACTOR;
    m_ptImageTopLeft = CPoint(0, 0);
    m_bDragging = FALSE;
    if (GetCapture() == this)
        ReleaseCapture();
}
void CPdfViewerCtrl::CloseDocument()
{
    ResetViewerState();
    UpdatePageInfoLabel();
    if (GetSafeHwnd() != nullptr)
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}
BOOL CPdfViewerCtrl::IsDocumentOpen() const
{
    return m_doc.IsOpen() && m_nPageCount > 0;
}
BOOL CPdfViewerCtrl::OpenDocument(const CStringW& strPdfPath)
{
    CloseDocument();
    if (!m_doc.Open(strPdfPath))
        return FALSE;
    m_strPdfPath = strPdfPath;
    m_nPageCount = m_doc.GetPageCount();
    if (m_nPageCount <= 0)
    {
        CloseDocument();
        return FALSE;
    }
    m_arrPageWidths.SetSize(m_nPageCount);
    m_arrPageHeights.SetSize(m_nPageCount);
    for (int i = 0; i < m_nPageCount; ++i)
    {
        double dW = 0.0;
        double dH = 0.0;
        if (!m_doc.GetPageSize(i, dW, dH))
        {
            CloseDocument();
            return FALSE;
        }
        m_arrPageWidths[i] = dW;
        m_arrPageHeights[i] = dH;
    }
    BuildThumbnails();
    SetCurrentPage(0);
    if (GetSafeHwnd() != nullptr)
    {
        SetFocus();
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    }
    return TRUE;
}
int CPdfViewerCtrl::GetThumbDisplayWidth() const
{
    return max(THUMB_MIN_RENDER_WIDTH, THUMB_PANE_WIDTH - (THUMB_GAP * 2));
}
int CPdfViewerCtrl::GetThumbRenderWidth() const
{
    const int nDisplayW = GetThumbDisplayWidth();
    int nDpi = 96;
    if (GetSafeHwnd() != nullptr)
    {
        CClientDC dc(const_cast<CPdfViewerCtrl*>(this));
        nDpi = dc.GetDeviceCaps(LOGPIXELSX);
        if (nDpi <= 0)
            nDpi = 96;
    }
    const double dDpiScale = static_cast<double>(nDpi) / 96.0;
    return max(
        nDisplayW,
        RoundToInt(nDisplayW * dDpiScale * THUMB_RENDER_SUPERSAMPLE));
}
int CPdfViewerCtrl::GetThumbItemHeight() const
{
    const int nThumbW = GetThumbDisplayWidth();
    const int nThumbH = max(1, RoundToInt(nThumbW / A4_ASPECT_WH));
    return nThumbH + THUMB_LABEL_HEIGHT + THUMB_GAP;
}
int CPdfViewerCtrl::GetThumbContentHeight() const
{
    return m_nPageCount * GetThumbItemHeight();
}
void CPdfViewerCtrl::BuildThumbnails()
{
    FreeThumbnails();
    m_arrThumbBitmaps.SetSize(m_nPageCount);
    const int nThumbW = GetThumbRenderWidth();
    for (int i = 0; i < m_nPageCount; ++i)
    {
        CImage thumbImage;
        if (!m_doc.RenderPage(i, nThumbW, thumbImage))
        {
            m_arrThumbBitmaps[i] = nullptr;
            continue;
        }
        m_arrThumbBitmaps[i] = thumbImage.Detach();
    }
}
int CPdfViewerCtrl::GetPageRenderWidth() const
{
    if (m_rcPagePane.IsRectEmpty())
        return PAGE_RENDER_MIN_WIDTH;
    int nDpi = 96;
    if (GetSafeHwnd() != nullptr)
    {
        CClientDC dc(const_cast<CPdfViewerCtrl*>(this));
        nDpi = dc.GetDeviceCaps(LOGPIXELSX);
        if (nDpi <= 0)
            nDpi = 96;
    }
    const double dDpiScale = static_cast<double>(nDpi) / 96.0;
    const int nPaneW = max(1, m_rcPagePane.Width());
    const int nRenderW = static_cast<int>(
        nPaneW * dDpiScale * PAGE_RENDER_SUPERSAMPLE + 0.5);
    const int nClamped = max(nPaneW, min(nRenderW, PAGE_RENDER_MAX_WIDTH));
    return max(PAGE_RENDER_MIN_WIDTH, nClamped);
}
void CPdfViewerCtrl::RenderCurrentPage(BOOL bPreserveZoom)
{
    m_pageImage.Destroy();
    if (!IsDocumentOpen() || m_nCurrentPage < 0 || m_nCurrentPage >= m_nPageCount)
        return;
    const int nRenderW = GetPageRenderWidth();
    m_nPageRenderWidth = nRenderW;
    CImage pageImage;
    if (!m_doc.RenderPage(m_nCurrentPage, nRenderW, pageImage))
        return;
    HBITMAP hBitmap = pageImage.Detach();
    if (hBitmap == nullptr)
        return;
    m_pageImage.Attach(hBitmap);
    if (bPreserveZoom)
    {
        UpdateBaseScale();
        CenterImageInPane(m_rcPagePane);
        ClampViewPosition();
    }
    else
    {
        ResetZoomPan();
    }
}
void CPdfViewerCtrl::SetCurrentPage(int nPage)
{
    if (!IsDocumentOpen())
        return;
    nPage = max(0, min(m_nPageCount - 1, nPage));
    if (nPage == m_nCurrentPage && !m_pageImage.IsNull())
        return;
    const BOOL bPreserveZoom = !m_pageImage.IsNull();
    m_nCurrentPage = nPage;
    RenderCurrentPage(bPreserveZoom);
    EnsureThumbVisible(nPage);
    UpdatePageInfoLabel();
    if (GetSafeHwnd() != nullptr)
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
}
void CPdfViewerCtrl::GoToAdjacentPage(int nDirection)
{
    if (!IsDocumentOpen() || nDirection == 0)
        return;
    SetCurrentPage(m_nCurrentPage + nDirection);
}
void CPdfViewerCtrl::UpdateLayoutRects()
{
    CRect rcClient;
    GetClientRect(&rcClient);
    m_rcToolbar = CRect(
        0,
        0,
        rcClient.right,
        min(TOOLBAR_HEIGHT, rcClient.Height()));
    const int nBodyTop = m_rcToolbar.bottom + PANE_GAP;
    m_rcThumbPane = CRect(
        PANE_GAP,
        nBodyTop,
        min(THUMB_PANE_WIDTH + PANE_GAP, rcClient.right),
        rcClient.bottom - PANE_GAP);
    m_rcPagePane = CRect(
        m_rcThumbPane.right + PANE_GAP,
        nBodyTop,
        rcClient.right - PANE_GAP,
        rcClient.bottom - PANE_GAP);
    if (m_rcPagePane.left >= m_rcPagePane.right)
        m_rcPagePane.SetRectEmpty();
}
void CPdfViewerCtrl::LayoutToolbarButtons()
{
    if (!::IsWindow(m_btnBack.GetSafeHwnd()))
        return;
    const int nTop = TOOLBAR_GAP / 2;
    const int nHeight = max(24, TOOLBAR_HEIGHT - TOOLBAR_GAP);
    int nLeft = TOOLBAR_GAP;
    auto placeButton = [&](CButton& btn, int nWidth)
    {
        if (!::IsWindow(btn.GetSafeHwnd()))
            return;
        btn.SetWindowPos(
            nullptr,
            nLeft,
            nTop,
            nWidth,
            nHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
        nLeft += nWidth + TOOLBAR_GAP;
    };
    placeButton(m_btnBack, 140);
    placeButton(m_btnPrevPage, 96);
    placeButton(m_btnNextPage, 96);
    placeButton(m_btnZoomOut, 72);
    placeButton(m_btnZoomIn, 72);

    if (::IsWindow(m_staticPageInfo.GetSafeHwnd()))
    {
        const int nLabelWidth = 96;
        m_staticPageInfo.SetWindowPos(
            nullptr,
            nLeft,
            nTop,
            nLabelWidth,
            nHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CPdfViewerCtrl::UpdatePageInfoLabel()
{
    if (!::IsWindow(m_staticPageInfo.GetSafeHwnd()))
        return;

    CString strLabel;
    if (IsDocumentOpen() && m_nCurrentPage >= 0)
        strLabel.Format(L"%d / %d", m_nCurrentPage + 1, m_nPageCount);
    else
        strLabel = L"- / -";

    m_staticPageInfo.SetWindowText(strLabel);
}

void CPdfViewerCtrl::UpdateBaseScale()
{
    m_dBaseScale = 1.0;
    if (m_pageImage.IsNull() || m_rcPagePane.IsRectEmpty())
        return;
    const int nImgW = m_pageImage.GetWidth();
    const int nImgH = m_pageImage.GetHeight();
    if (nImgW <= 0 || nImgH <= 0)
        return;
    m_dBaseScale = min(
        static_cast<double>(m_rcPagePane.Width()) / nImgW,
        static_cast<double>(m_rcPagePane.Height()) / nImgH);
}
double CPdfViewerCtrl::GetCurrentScale() const
{
    return m_dBaseScale * m_dZoomFactor;
}
void CPdfViewerCtrl::GetDrawSize(const CRect& rcPane, int& nDrawW, int& nDrawH) const
{
    nDrawW = 0;
    nDrawH = 0;
    if (m_pageImage.IsNull())
        return;
    const double dScale = GetCurrentScale();
    nDrawW = max(1, RoundToInt(m_pageImage.GetWidth() * dScale));
    nDrawH = max(1, RoundToInt(m_pageImage.GetHeight() * dScale));
}
void CPdfViewerCtrl::CenterImageInPane(const CRect& rcPane)
{
    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(rcPane, nDrawW, nDrawH);
    m_ptImageTopLeft.x = rcPane.left + (rcPane.Width() - nDrawW) / 2;
    m_ptImageTopLeft.y = rcPane.top + (rcPane.Height() - nDrawH) / 2;
}
void CPdfViewerCtrl::ResetZoomPan()
{
    m_dZoomFactor = DEFAULT_ZOOM_FACTOR;
    UpdateBaseScale();
    CenterImageInPane(m_rcPagePane);
}
void CPdfViewerCtrl::FitPageToWindow()
{
    if (m_pageImage.IsNull())
        return;
    ResetZoomPan();
    Invalidate();
}
void CPdfViewerCtrl::ClampViewPosition()
{
    if (m_pageImage.IsNull() || m_rcPagePane.IsRectEmpty())
        return;
    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(m_rcPagePane, nDrawW, nDrawH);
    const int nMinX = min(m_rcPagePane.left, m_rcPagePane.right - nDrawW);
    const int nMaxX = max(m_rcPagePane.left, m_rcPagePane.right - nDrawW);
    const int nMinY = min(m_rcPagePane.top, m_rcPagePane.bottom - nDrawH);
    const int nMaxY = max(m_rcPagePane.top, m_rcPagePane.bottom - nDrawH);
    m_ptImageTopLeft.x = max(nMinX, min(nMaxX, m_ptImageTopLeft.x));
    m_ptImageTopLeft.y = max(nMinY, min(nMaxY, m_ptImageTopLeft.y));
}
CRect CPdfViewerCtrl::GetPageDrawRect(const CRect& rcPane) const
{
    if (m_pageImage.IsNull())
        return CRect(0, 0, 0, 0);
    int nDrawW = 0;
    int nDrawH = 0;
    GetDrawSize(rcPane, nDrawW, nDrawH);
    return CRect(
        m_ptImageTopLeft.x,
        m_ptImageTopLeft.y,
        m_ptImageTopLeft.x + nDrawW,
        m_ptImageTopLeft.y + nDrawH);
}
void CPdfViewerCtrl::ZoomAtPoint(double dFactor, const CPoint& ptClient)
{
    if (m_pageImage.IsNull() || m_rcPagePane.IsRectEmpty())
        return;
    CPoint ptZoom = ptClient;
    if (!m_rcPagePane.PtInRect(ptZoom))
        ptZoom = m_rcPagePane.CenterPoint();
    const double dOldScale = GetCurrentScale();
    CRect rcDraw = GetPageDrawRect(m_rcPagePane);
    if (rcDraw.Width() <= 0 || rcDraw.Height() <= 0 || dOldScale <= 0.0)
        return;
    const double dImgX = (ptZoom.x - rcDraw.left) / dOldScale;
    const double dImgY = (ptZoom.y - rcDraw.top) / dOldScale;
    m_dZoomFactor *= dFactor;
    m_dZoomFactor = max(MIN_ZOOM_FACTOR, min(MAX_ZOOM_FACTOR, m_dZoomFactor));
    const double dNewScale = GetCurrentScale();
    m_ptImageTopLeft.x = ptZoom.x - RoundToInt(dImgX * dNewScale);
    m_ptImageTopLeft.y = ptZoom.y - RoundToInt(dImgY * dNewScale);
    ClampViewPosition();
}
void CPdfViewerCtrl::ZoomInAtCenter()
{
    if (m_rcPagePane.IsRectEmpty())
        return;
    ZoomAtPoint(ZOOM_STEP, m_rcPagePane.CenterPoint());
    Invalidate();
}
void CPdfViewerCtrl::ZoomOutAtCenter()
{
    if (m_rcPagePane.IsRectEmpty())
        return;
    ZoomAtPoint(1.0 / ZOOM_STEP, m_rcPagePane.CenterPoint());
    Invalidate();
}
BOOL CPdfViewerCtrl::ProcessWheelAction(short zDelta, const CPoint& ptClient, BOOL bCtrlDown)
{
    if (!IsDocumentOpen() || GetSafeHwnd() == nullptr || zDelta == 0)
        return FALSE;
    CRect rcClient;
    GetClientRect(&rcClient);
    if (!rcClient.PtInRect(ptClient))
        return FALSE;
    if (bCtrlDown)
    {
        CPoint ptZoom = m_rcPagePane.CenterPoint();
        if (m_rcPagePane.PtInRect(ptClient))
            ptZoom = ptClient;
        const double dFactor = (zDelta > 0) ? ZOOM_STEP : (1.0 / ZOOM_STEP);
        ZoomAtPoint(dFactor, ptZoom);
        Invalidate();
        return TRUE;
    }
    if (zDelta > 0)
        GoToAdjacentPage(-1);
    else
        GoToAdjacentPage(1);
    return TRUE;
}
BOOL CPdfViewerCtrl::HandleMouseWheel(short zDelta, const CPoint& ptScreen)
{
    if (GetSafeHwnd() == nullptr)
        return FALSE;
    CRect rcWindow;
    GetWindowRect(&rcWindow);
    if (!rcWindow.PtInRect(ptScreen))
        return FALSE;
    CPoint ptClient = ptScreen;
    ScreenToClient(&ptClient);
    return ProcessWheelAction(zDelta, ptClient, IsCtrlDown());
}
BOOL CPdfViewerCtrl::HandleKeyDown(UINT nChar)
{
    if (!IsDocumentOpen())
        return FALSE;
    if (IsCtrlDown())
    {
        switch (nChar)
        {
        case VK_ADD:
        case VK_OEM_PLUS:
        case static_cast<UINT>('+'):
        case static_cast<UINT>('='):
            ZoomInAtCenter();
            return TRUE;
        case VK_SUBTRACT:
        case VK_OEM_MINUS:
        case static_cast<UINT>('-'):
            ZoomOutAtCenter();
            return TRUE;
        case static_cast<UINT>('0'):
            FitPageToWindow();
            return TRUE;
        default:
            break;
        }
        return FALSE;
    }
    switch (nChar)
    {
    case VK_UP:
    case VK_PRIOR:
        GoToAdjacentPage(-1);
        return TRUE;
    case VK_DOWN:
    case VK_NEXT:
        GoToAdjacentPage(1);
        return TRUE;
    case VK_HOME:
        SetCurrentPage(0);
        return TRUE;
    case VK_END:
        SetCurrentPage(m_nPageCount - 1);
        return TRUE;
    default:
        return FALSE;
    }
}
void CPdfViewerCtrl::EnsureThumbVisible(int nPage)
{
    if (!IsDocumentOpen() || m_rcThumbPane.IsRectEmpty())
        return;
    const int nItemTop = nPage * GetThumbItemHeight();
    const int nItemBottom = nItemTop + GetThumbItemHeight();
    if (nItemTop < m_nThumbScrollY)
        m_nThumbScrollY = nItemTop;
    else if (nItemBottom > m_nThumbScrollY + m_rcThumbPane.Height())
        m_nThumbScrollY = nItemBottom - m_rcThumbPane.Height();
    const int nMaxScroll = max(0, GetThumbContentHeight() - m_rcThumbPane.Height());
    m_nThumbScrollY = max(0, min(nMaxScroll, m_nThumbScrollY));
}
int CPdfViewerCtrl::HitTestThumb(const CPoint& ptClient) const
{
    if (!m_rcThumbPane.PtInRect(ptClient) || m_nPageCount <= 0)
        return -1;
    const int nLocalY = ptClient.y - m_rcThumbPane.top + m_nThumbScrollY;
    const int nItemH = GetThumbItemHeight();
    const int nIndex = nLocalY / nItemH;
    if (nIndex < 0 || nIndex >= m_nPageCount)
        return -1;
    return nIndex;
}
void CPdfViewerCtrl::DrawThumbnails(CDC& dc)
{
    if (!IsDocumentOpen() || m_rcThumbPane.IsRectEmpty())
        return;
    CRgn clipRgn;
    clipRgn.CreateRectRgnIndirect(&m_rcThumbPane);
    dc.SelectClipRgn(&clipRgn);
    dc.FillSolidRect(&m_rcThumbPane, RGB(245, 247, 252));
    CFont fontLabel;
    fontLabel.CreatePointFont(75, L"Malgun Gothic");
    CFont* pOldFont = dc.SelectObject(&fontLabel);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(45, 55, 75));
    const int nThumbW = GetThumbDisplayWidth();
    const int nThumbH = max(1, RoundToInt(nThumbW / A4_ASPECT_WH));
    const int nItemH = GetThumbItemHeight();
    const int nLeft = m_rcThumbPane.left + THUMB_GAP;
    for (int i = 0; i < m_nPageCount; ++i)
    {
        const int nTop = m_rcThumbPane.top + (i * nItemH) - m_nThumbScrollY;
        const int nBottom = nTop + nItemH;
        if (nBottom < m_rcThumbPane.top || nTop > m_rcThumbPane.bottom)
            continue;
        const BOOL bSelected = (i == m_nCurrentPage);
        CPen penFrame;
        penFrame.CreatePen(
            PS_SOLID,
            bSelected ? 2 : 1,
            bSelected ? RGB(30, 120, 210) : RGB(180, 190, 205));
        CPen* pOldPen = dc.SelectObject(&penFrame);
        CBrush brushFrame;
        brushFrame.CreateSolidBrush(RGB(255, 255, 255));
        CBrush* pOldBrush = dc.SelectObject(&brushFrame);
        CRect rcThumbFrame(
            nLeft,
            nTop,
            nLeft + nThumbW,
            nTop + nThumbH);
        dc.Rectangle(&rcThumbFrame);
        dc.SelectObject(pOldBrush);
        dc.SelectObject(pOldPen);
        HBITMAP hThumb = (i < m_arrThumbBitmaps.GetSize()) ? m_arrThumbBitmaps[i] : nullptr;
        if (hThumb != nullptr)
        {
            CImage thumbImage;
            thumbImage.Attach(hThumb);
            const int nImgW = thumbImage.GetWidth();
            const int nImgH = thumbImage.GetHeight();
            if (nImgW > 0 && nImgH > 0)
            {
                const double dScale = min(
                    static_cast<double>(rcThumbFrame.Width() - 4) / nImgW,
                    static_cast<double>(rcThumbFrame.Height() - 4) / nImgH);
                const int nDrawW = max(1, RoundToInt(nImgW * dScale));
                const int nDrawH = max(1, RoundToInt(nImgH * dScale));
                const int nOffsetX = rcThumbFrame.left + 2 + (rcThumbFrame.Width() - 4 - nDrawW) / 2;
                const int nOffsetY = rcThumbFrame.top + 2 + (rcThumbFrame.Height() - 4 - nDrawH) / 2;
                const int nOldStretchMode = dc.SetStretchBltMode(STRETCH_HALFTONE);
                thumbImage.StretchBlt(
                    dc.m_hDC,
                    nOffsetX, nOffsetY, nDrawW, nDrawH,
                    0, 0, nImgW, nImgH,
                    SRCCOPY);
                dc.SetStretchBltMode(nOldStretchMode);
            }
            thumbImage.Detach();
        }
        CRect rcLabel(
            nLeft,
            rcThumbFrame.bottom,
            nLeft + nThumbW,
            rcThumbFrame.bottom + THUMB_LABEL_HEIGHT);
        CStringW strLabel;
        strLabel.Format(L"%d", i + 1);
        dc.DrawText(strLabel, &rcLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    dc.SelectObject(pOldFont);
    dc.SelectClipRgn(nullptr);
}
void CPdfViewerCtrl::DrawPage(CDC& dc)
{
    if (m_rcPagePane.IsRectEmpty())
        return;
    dc.FillSolidRect(&m_rcPagePane, RGB(228, 232, 240));
    if (m_pageImage.IsNull())
        return;
    if (!m_bDragging)
        ClampViewPosition();
    CRgn clipRgn;
    clipRgn.CreateRectRgnIndirect(&m_rcPagePane);
    dc.SelectClipRgn(&clipRgn);
    CRect rcDraw = GetPageDrawRect(m_rcPagePane);
    CRect rcDest;
    if (!rcDest.IntersectRect(&rcDraw, &m_rcPagePane))
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
    const int nImgW = m_pageImage.GetWidth();
    const int nImgH = m_pageImage.GetHeight();
    int nSrcX = RoundToInt((rcDest.left - rcDraw.left) / dScale);
    int nSrcY = RoundToInt((rcDest.top - rcDraw.top) / dScale);
    int nSrcW = max(1, RoundToInt(rcDest.Width() / dScale));
    int nSrcH = max(1, RoundToInt(rcDest.Height() / dScale));
    nSrcX = max(0, min(nImgW - 1, nSrcX));
    nSrcY = max(0, min(nImgH - 1, nSrcY));
    nSrcW = max(1, min(nImgW - nSrcX, nSrcW));
    nSrcH = max(1, min(nImgH - nSrcY, nSrcH));
    const BOOL bDownscale =
        nSrcW >= rcDest.Width() && nSrcH >= rcDest.Height();
    const int nStretchMode = bDownscale ? STRETCH_HALFTONE : COLORONCOLOR;
    const int nOldStretchMode = dc.SetStretchBltMode(nStretchMode);
    m_pageImage.StretchBlt(
        dc.m_hDC,
        rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
        nSrcX, nSrcY, nSrcW, nSrcH,
        SRCCOPY);
    dc.SetStretchBltMode(nOldStretchMode);
    dc.SelectClipRgn(nullptr);
}
BEGIN_MESSAGE_MAP(CPdfViewerCtrl, CWnd)
    ON_WM_CREATE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_COMMAND(IDC_PDF_BTN_BACK, OnBtnBack)
    ON_COMMAND(IDC_PDF_BTN_PREV_PAGE, OnBtnPrevPage)
    ON_COMMAND(IDC_PDF_BTN_NEXT_PAGE, OnBtnNextPage)
    ON_COMMAND(IDC_PDF_BTN_ZOOM_IN, OnBtnZoomIn)
    ON_COMMAND(IDC_PDF_BTN_ZOOM_OUT, OnBtnZoomOut)
END_MESSAGE_MAP()
void CPdfViewerCtrl::OnPaint()
{
    CPaintDC dc(this);
    DrawThumbnails(dc);
    DrawPage(dc);
}
BOOL CPdfViewerCtrl::OnEraseBkgnd(CDC* pDC)
{
    CRect rcClient;
    GetClientRect(&rcClient);
    pDC->FillSolidRect(&rcClient, RGB(235, 240, 248));
    return TRUE;
}
void CPdfViewerCtrl::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateLayoutRects();
    LayoutToolbarButtons();
    if (IsDocumentOpen())
    {
        if (!m_pageImage.IsNull())
        {
            UpdateBaseScale();
            ClampViewPosition();
        }
        else if (m_nCurrentPage >= 0)
        {
            RenderCurrentPage(FALSE);
        }
    }
    if (GetSafeHwnd() != nullptr)
        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
}
void CPdfViewerCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    SetFocus();
    const int nThumb = HitTestThumb(point);
    if (nThumb >= 0)
    {
        SetCurrentPage(nThumb);
        CWnd::OnLButtonDown(nFlags, point);
        return;
    }
    if (m_rcPagePane.PtInRect(point) && !m_pageImage.IsNull())
    {
        m_bDragging = TRUE;
        m_ptDragLast = point;
        SetCapture();
    }
    CWnd::OnLButtonDown(nFlags, point);
}
void CPdfViewerCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        m_bDragging = FALSE;
        if (GetCapture() == this)
            ReleaseCapture();
    }
    CWnd::OnLButtonUp(nFlags, point);
}
void CPdfViewerCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging && (nFlags & MK_LBUTTON))
    {
        m_ptImageTopLeft.x += point.x - m_ptDragLast.x;
        m_ptImageTopLeft.y += point.y - m_ptDragLast.y;
        m_ptDragLast = point;
        ClampViewPosition();
        Invalidate();
    }
    CWnd::OnMouseMove(nFlags, point);
}
BOOL CPdfViewerCtrl::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    const BOOL bCtrlDown = IsCtrlDown() || ((nFlags & MK_CONTROL) != 0);
    if (ProcessWheelAction(zDelta, pt, bCtrlDown))
        return TRUE;
    return CWnd::OnMouseWheel(nFlags, zDelta, pt);
}
void CPdfViewerCtrl::OnBtnBack()
{
    CWnd* pParent = GetParent();
    if (pParent != nullptr)
        pParent->PostMessage(WM_PDF_VIEWER_BACK_TO_LIST, 0, 0);
}
void CPdfViewerCtrl::OnBtnPrevPage()
{
    GoToAdjacentPage(-1);
}
void CPdfViewerCtrl::OnBtnNextPage()
{
    GoToAdjacentPage(1);
}
void CPdfViewerCtrl::OnBtnZoomIn()
{
    ZoomInAtCenter();
}
void CPdfViewerCtrl::OnBtnZoomOut()
{
    ZoomOutAtCenter();
}
