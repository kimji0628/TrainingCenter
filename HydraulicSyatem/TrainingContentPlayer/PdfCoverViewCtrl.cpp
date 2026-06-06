#include "pch.h"

#include "PdfCoverViewCtrl.h"

#include "PdfRenderEngine.h"

#include "Util.h"



IMPLEMENT_DYNAMIC(CPdfCoverViewCtrl, CStatic)



namespace

{

    constexpr int CARD_PADDING = 8;

    constexpr int CARD_GAP = 12;

    constexpr int LABEL_HEIGHT = 22;

    constexpr int THUMB_MIN_WIDTH = 56;

    constexpr int COVER_RENDER_MIN_WIDTH = 360;

    constexpr int COVER_RENDER_MAX_WIDTH = 1200;

    constexpr double COVER_RENDER_SUPERSAMPLE = 2.0;

    constexpr double A4_WIDTH_MM = 210.0;

    constexpr double A4_HEIGHT_MM = 297.0;

    constexpr double A4_ASPECT_WH = A4_WIDTH_MM / A4_HEIGHT_MM;

}



CPdfCoverViewCtrl::CPdfCoverViewCtrl()

    : m_nSelectedPdfIndex(-1)

{

}



CPdfCoverViewCtrl::~CPdfCoverViewCtrl()

{

    FreeCoverBitmaps();

}



void CPdfCoverViewCtrl::FreeCoverBitmaps()

{

    for (int i = 0; i < m_arrCoverBitmaps.GetSize(); ++i)

    {

        if (m_arrCoverBitmaps[i] != nullptr)

            ::DeleteObject(m_arrCoverBitmaps[i]);

    }



    m_arrCoverBitmaps.RemoveAll();

}



BOOL CPdfCoverViewCtrl::CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId)

{

    if (pParent == nullptr || pPlaceholder == nullptr ||

        !::IsWindow(pPlaceholder->GetSafeHwnd()))

    {

        return FALSE;

    }



    CRect rcHost;

    pPlaceholder->GetWindowRect(&rcHost);

    pParent->ScreenToClient(&rcHost);



    const DWORD dwStyle = WS_CHILD | WS_CLIPSIBLINGS | SS_NOTIFY;



    return Create(

        L"",

        dwStyle,

        rcHost,

        pParent,

        nHostId);

}



void CPdfCoverViewCtrl::ComputeCoverGridLayout(

    const CRect& rcClient,

    int nCount,

    PdfCoverGridLayout& layout) const

{

    layout = {};



    if (nCount <= 0 || rcClient.IsRectEmpty())

        return;



    int nBestCols = 1;

    int nBestThumbW = 0;



    for (int nTryCols = 1; nTryCols <= nCount; ++nTryCols)

    {

        const int nTryRows = (nCount + nTryCols - 1) / nTryCols;

        const int nAvailW = rcClient.Width() - (CARD_GAP * (nTryCols + 1));

        const int nAvailH = rcClient.Height() - (CARD_GAP * (nTryRows + 1));



        if (nAvailW <= 0 || nAvailH <= 0)

            continue;



        const int nMaxThumbWByWidth = (nAvailW / nTryCols) - CARD_PADDING;

        const int nMaxThumbHByHeight = (nAvailH / nTryRows) - LABEL_HEIGHT - CARD_PADDING;



        if (nMaxThumbWByWidth <= 0 || nMaxThumbHByHeight <= 0)

            continue;



        const int nMaxThumbWByHeight = static_cast<int>(nMaxThumbHByHeight * A4_ASPECT_WH + 0.5);

        const int nThumbW = min(nMaxThumbWByWidth, nMaxThumbWByHeight);



        if (nThumbW < THUMB_MIN_WIDTH)

            continue;



        if (nThumbW > nBestThumbW ||

            (nThumbW == nBestThumbW && nTryCols > nBestCols))

        {

            nBestThumbW = nThumbW;

            nBestCols = nTryCols;

        }

    }



    if (nBestThumbW <= 0)

    {

        nBestCols = 1;

        const int nTryRows = nCount;

        const int nAvailW = max(1, rcClient.Width() - (CARD_GAP * 2));

        const int nAvailH = max(1, rcClient.Height() - (CARD_GAP * (nTryRows + 1)));

        const int nMaxThumbWByWidth = max(THUMB_MIN_WIDTH, (nAvailW / nBestCols) - CARD_PADDING);

        const int nMaxThumbHByHeight = max(1, (nAvailH / nTryRows) - LABEL_HEIGHT - CARD_PADDING);

        const int nMaxThumbWByHeight = static_cast<int>(nMaxThumbHByHeight * A4_ASPECT_WH + 0.5);

        nBestThumbW = max(THUMB_MIN_WIDTH, min(nMaxThumbWByWidth, nMaxThumbWByHeight));

    }



    layout.nCols = nBestCols;

    layout.nRows = (nCount + nBestCols - 1) / nBestCols;

    layout.nThumbW = nBestThumbW;

    layout.nThumbH = max(1, static_cast<int>(nBestThumbW / A4_ASPECT_WH + 0.5));

    layout.nCellW = layout.nThumbW + CARD_PADDING;

    layout.nCellH = layout.nThumbH + LABEL_HEIGHT + CARD_PADDING;



    const int nGridW = (layout.nCols * layout.nCellW) + (CARD_GAP * (layout.nCols + 1));

    const int nGridH = (layout.nRows * layout.nCellH) + (CARD_GAP * (layout.nRows + 1));



    layout.nGridOriginX = max(0, (rcClient.Width() - nGridW) / 2);

    layout.nGridOriginY = max(0, (rcClient.Height() - nGridH) / 2);

}



int CPdfCoverViewCtrl::GetCoverRenderWidth() const

{

    CRect rcClient;

    if (GetSafeHwnd() != nullptr)

        GetClientRect(&rcClient);



    if (rcClient.IsRectEmpty())

        rcClient.SetRect(0, 0, 960, 640);



    PdfCoverGridLayout layout;

    ComputeCoverGridLayout(rcClient, static_cast<int>(m_arrPdfIndices.GetSize()), layout);



    const int nThumbW = max(THUMB_MIN_WIDTH, layout.nThumbW);



    int nDpi = 96;

    if (GetSafeHwnd() != nullptr)

    {

        CClientDC dc(const_cast<CPdfCoverViewCtrl*>(this));

        nDpi = dc.GetDeviceCaps(LOGPIXELSX);

        if (nDpi <= 0)

            nDpi = 96;

    }



    const double dDpiScale = static_cast<double>(nDpi) / 96.0;

    const int nRenderW = static_cast<int>(nThumbW * dDpiScale * COVER_RENDER_SUPERSAMPLE);



    return max(COVER_RENDER_MIN_WIDTH, min(nRenderW, COVER_RENDER_MAX_WIDTH));

}



void CPdfCoverViewCtrl::ClearView()

{

    FreeCoverBitmaps();

    m_arrPdfIndices.RemoveAll();

    m_arrFileNames.RemoveAll();

    m_arrPageWidths.RemoveAll();

    m_arrPageHeights.RemoveAll();

    m_nSelectedPdfIndex = -1;



    if (GetSafeHwnd() != nullptr)

        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

}



void CPdfCoverViewCtrl::ShowPdfCovers(

    const CStringArray& arrPdfPaths,

    const CArray<int>& arrPdfIndices)

{

    ClearView();



    const int nCoverRenderWidth = GetCoverRenderWidth();



    for (int i = 0; i < arrPdfIndices.GetSize(); ++i)

    {

        const int nPdfIndex = arrPdfIndices[i];

        if (nPdfIndex < 0 || nPdfIndex >= arrPdfPaths.GetSize())

            continue;



        const CStringW& strFullPath = arrPdfPaths[nPdfIndex];



        int nSlash = strFullPath.ReverseFind(L'\\');

        const CStringW strFileName = (nSlash >= 0) ? strFullPath.Mid(nSlash + 1) : strFullPath;



        double dPageWidth = 0.0;

        double dPageHeight = 0.0;



        CImage coverImage;

        if (!PdfRenderEngine::RenderFirstPage(

                strFullPath,

                coverImage,

                nCoverRenderWidth,

                &dPageWidth,

                &dPageHeight))

        {

            continue;

        }



        if (dPageWidth <= 0.0 || dPageHeight <= 0.0)

            continue;



        HBITMAP hCover = coverImage.Detach();

        if (hCover == nullptr)

            continue;



        m_arrPdfIndices.Add(nPdfIndex);

        m_arrFileNames.Add(strFileName);

        m_arrCoverBitmaps.Add(hCover);

        m_arrPageWidths.Add(dPageWidth);

        m_arrPageHeights.Add(dPageHeight);

    }



    if (GetSafeHwnd() != nullptr)

        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

}



void CPdfCoverViewCtrl::SetSelectedPdfIndex(int nPdfIndex)

{

    m_nSelectedPdfIndex = nPdfIndex;



    if (GetSafeHwnd() != nullptr)

        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);

}



int CPdfCoverViewCtrl::GetSelectedPdfIndex() const

{

    return m_nSelectedPdfIndex;

}



BEGIN_MESSAGE_MAP(CPdfCoverViewCtrl, CStatic)

    ON_WM_PAINT()

    ON_WM_ERASEBKGND()

    ON_WM_SIZE()

    ON_WM_LBUTTONDOWN()

END_MESSAGE_MAP()



CRect CPdfCoverViewCtrl::GetCardFrameRect(

    const PdfCoverGridLayout& layout,

    int nCol,

    int nRow)

{

    const int nLeft = layout.nGridOriginX + CARD_GAP + nCol * (layout.nCellW + CARD_GAP);

    const int nTop = layout.nGridOriginY + CARD_GAP + nRow * (layout.nCellH + CARD_GAP);

    return CRect(nLeft, nTop, nLeft + layout.nCellW, nTop + layout.nCellH);

}



CRect CPdfCoverViewCtrl::GetThumbRect(

    const CRect& rcCardFrame,

    const PdfCoverGridLayout& layout)

{

    const int nLeft = rcCardFrame.left + (layout.nCellW - layout.nThumbW) / 2;

    const int nTop = rcCardFrame.top + CARD_PADDING / 2;

    return CRect(

        nLeft,

        nTop,

        nLeft + layout.nThumbW,

        nTop + layout.nThumbH);

}



CRect CPdfCoverViewCtrl::GetLabelRect(const CRect& rcCardFrame)

{

    return CRect(

        rcCardFrame.left,

        rcCardFrame.bottom - LABEL_HEIGHT,

        rcCardFrame.right,

        rcCardFrame.bottom);

}



CRect CPdfCoverViewCtrl::CalcAspectFitRect(

    const CRect& rcBounds,

    double dPageW,

    double dPageH)

{

    if (rcBounds.IsRectEmpty() || dPageW <= 0.0 || dPageH <= 0.0)

        return rcBounds;



    const double dScale = min(

        static_cast<double>(rcBounds.Width()) / dPageW,

        static_cast<double>(rcBounds.Height()) / dPageH);

    const int nDrawW = max(1, static_cast<int>(dPageW * dScale + 0.5));

    const int nDrawH = max(1, static_cast<int>(dPageH * dScale + 0.5));

    const int nLeft = rcBounds.left + (rcBounds.Width() - nDrawW) / 2;

    const int nTop = rcBounds.top + (rcBounds.Height() - nDrawH) / 2;



    return CRect(nLeft, nTop, nLeft + nDrawW, nTop + nDrawH);

}



void CPdfCoverViewCtrl::DrawCoverBitmap(CDC& dc, HBITMAP hCover, const CRect& rcDest)

{

    if (hCover == nullptr || rcDest.IsRectEmpty())

        return;



    CImage coverImage;

    coverImage.Attach(hCover);



    const int nImgW = coverImage.GetWidth();

    const int nImgH = coverImage.GetHeight();

    if (nImgW > 0 && nImgH > 0)

    {

        const int nOldStretchMode = dc.SetStretchBltMode(STRETCH_HALFTONE);

        dc.SetBrushOrg(0, 0);



        if (rcDest.Width() == nImgW && rcDest.Height() == nImgH)

        {

            coverImage.BitBlt(dc.m_hDC, rcDest.left, rcDest.top);

        }

        else

        {

            coverImage.StretchBlt(

                dc.m_hDC,

                rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),

                0, 0, nImgW, nImgH,

                SRCCOPY);

        }



        dc.SetStretchBltMode(nOldStretchMode);

    }



    coverImage.Detach();

}



int CPdfCoverViewCtrl::HitTestCard(const CPoint& ptClient) const

{

    CRect rcClient;

    GetClientRect(&rcClient);



    PdfCoverGridLayout layout;

    ComputeCoverGridLayout(rcClient, static_cast<int>(m_arrPdfIndices.GetSize()), layout);



    const int nCount = static_cast<int>(m_arrPdfIndices.GetSize());

    for (int i = 0; i < nCount; ++i)

    {

        const CRect rcCard = GetCardFrameRect(layout, i % layout.nCols, i / layout.nCols);

        if (rcCard.PtInRect(ptClient))

            return i;

    }



    return -1;

}



void CPdfCoverViewCtrl::DrawCards(CDC& dc, const CRect& rcClient)

{

    CFont fontLabel;

    fontLabel.CreatePointFont(80, L"Malgun Gothic");

    CFont* pOldFont = dc.SelectObject(&fontLabel);

    dc.SetBkMode(TRANSPARENT);

    dc.SetTextColor(RGB(35, 45, 65));



    const int nCount = static_cast<int>(m_arrPdfIndices.GetSize());

    PdfCoverGridLayout layout;

    ComputeCoverGridLayout(rcClient, nCount, layout);



    for (int i = 0; i < nCount; ++i)

    {

        const CRect rcCardFrame = GetCardFrameRect(layout, i % layout.nCols, i / layout.nCols);

        const CRect rcThumb = GetThumbRect(rcCardFrame, layout);

        CRect rcLabel = GetLabelRect(rcCardFrame);



        const BOOL bSelected = (m_arrPdfIndices[i] == m_nSelectedPdfIndex);

        CPen penFrame;

        penFrame.CreatePen(

            PS_SOLID,

            bSelected ? 2 : 1,

            bSelected ? RGB(30, 120, 210) : RGB(180, 190, 205));

        CPen* pOldPen = dc.SelectObject(&penFrame);

        CBrush brushFrame;

        brushFrame.CreateSolidBrush(RGB(250, 252, 255));

        CBrush* pOldBrush = dc.SelectObject(&brushFrame);

        dc.Rectangle(&rcCardFrame);

        dc.SelectObject(pOldBrush);

        dc.SelectObject(pOldPen);



        dc.FillSolidRect(&rcThumb, RGB(255, 255, 255));



        if (i < m_arrPageWidths.GetSize() && i < m_arrPageHeights.GetSize())

        {

            const CRect rcFit = CalcAspectFitRect(

                rcThumb,

                m_arrPageWidths[i],

                m_arrPageHeights[i]);



            DrawCoverBitmap(dc, m_arrCoverBitmaps[i], rcFit);

        }



        dc.DrawText(m_arrFileNames[i], &rcLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    }



    dc.SelectObject(pOldFont);

}



void CPdfCoverViewCtrl::OnPaint()

{

    CPaintDC dc(this);

    CRect rcClient;

    GetClientRect(&rcClient);

    DrawCards(dc, rcClient);

}



BOOL CPdfCoverViewCtrl::OnEraseBkgnd(CDC* pDC)

{

    CRect rcClient;

    GetClientRect(&rcClient);

    pDC->FillSolidRect(&rcClient, RGB(235, 240, 248));

    return TRUE;

}



void CPdfCoverViewCtrl::OnSize(UINT nType, int cx, int cy)

{

    CStatic::OnSize(nType, cx, cy);



    if (GetSafeHwnd() != nullptr)

        RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);

}



void CPdfCoverViewCtrl::OnLButtonDown(UINT nFlags, CPoint point)

{

    const int nCard = HitTestCard(point);

    if (nCard >= 0 && nCard < m_arrPdfIndices.GetSize())

    {

        const int nPdfIndex = m_arrPdfIndices[nCard];

        SetSelectedPdfIndex(nPdfIndex);



        CWnd* pParent = GetParent();

        if (pParent != nullptr)

            pParent->PostMessage(WM_PDF_COVER_ITEM_SELECTED, static_cast<WPARAM>(nPdfIndex), 0);

    }



    CStatic::OnLButtonDown(nFlags, point);

}


