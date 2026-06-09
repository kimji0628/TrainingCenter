#include "pch.h"
#include "GeneratedQuestionListBox.h"

int CGeneratedQuestionListBox::GetQuestionIndexFromItemData(DWORD_PTR dwItemData)
{
    return static_cast<int>(dwItemData & 0x7FFFFFFF);
}

BOOL CGeneratedQuestionListBox::IsAdoptedItemData(DWORD_PTR dwItemData)
{
    return (dwItemData & kAdoptedItemFlag) != 0;
}

DWORD_PTR CGeneratedQuestionListBox::MakeItemData(int nQuestionIndex, BOOL bAdopted)
{
    DWORD_PTR dwData = static_cast<DWORD_PTR>(nQuestionIndex) & 0x7FFFFFFF;
    if (bAdopted)
        dwData |= kAdoptedItemFlag;
    return dwData;
}

BEGIN_MESSAGE_MAP(CGeneratedQuestionListBox, CListBox)
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

void CGeneratedQuestionListBox::OnLButtonUp(UINT nFlags, CPoint point)
{
    CListBox::OnLButtonUp(nFlags, point);

    const int nSel = GetCurSel();
    if (nSel == LB_ERR)
        return;

    CWnd* pParent = GetParent();
    if (pParent != nullptr && ::IsWindow(pParent->GetSafeHwnd()))
    {
        pParent->SendMessage(
            WM_COMMAND,
            MAKEWPARAM(static_cast<WPARAM>(GetDlgCtrlID()), LBN_SELCHANGE),
            reinterpret_cast<LPARAM>(m_hWnd));
    }
}

void CGeneratedQuestionListBox::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    if (lpMeasureItemStruct == nullptr)
        return;

    lpMeasureItemStruct->itemHeight = kItemHeight;
}

void CGeneratedQuestionListBox::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (lpDrawItemStruct == nullptr || lpDrawItemStruct->itemID == static_cast<UINT>(-1))
        return;

    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);

    CRect rcItem = lpDrawItemStruct->rcItem;
    const BOOL bSelected = (lpDrawItemStruct->itemState & ODS_SELECTED) != 0;
    const BOOL bFocused = (lpDrawItemStruct->itemState & ODS_FOCUS) != 0;
    const BOOL bAdopted = IsAdoptedItemData(lpDrawItemStruct->itemData);

    COLORREF clrBg = RGB(255, 255, 255);
    COLORREF clrText = RGB(0, 0, 0);

    if (bSelected)
    {
        if (bAdopted)
        {
            clrBg = kAdoptedSelectedBgColor;
            clrText = RGB(0, 0, 0);
        }
        else
        {
            clrBg = ::GetSysColor(COLOR_HIGHLIGHT);
            clrText = ::GetSysColor(COLOR_HIGHLIGHTTEXT);
        }
    }
    else if (bAdopted)
    {
        clrBg = kAdoptedBgColor;
    }

    dc.FillSolidRect(rcItem, clrBg);

    if (bAdopted)
    {
        CRect rcBar = rcItem;
        rcBar.right = rcBar.left + 4;
        dc.FillSolidRect(rcBar, kAdoptedBarColor);
        rcItem.left += 6;
    }
    else
    {
        rcItem.left += 2;
    }

    CString strText;
    GetText(static_cast<int>(lpDrawItemStruct->itemID), strText);

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(clrText);

    CFont* pFont = GetFont();
    CFont* pOldFont = nullptr;
    if (pFont != nullptr)
        pOldFont = dc.SelectObject(pFont);

    dc.DrawText(
        strText,
        rcItem,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    if (pOldFont != nullptr)
        dc.SelectObject(pOldFont);

    if (bFocused)
    {
        CRect rcFocus = lpDrawItemStruct->rcItem;
        dc.DrawFocusRect(rcFocus);
    }

    dc.Detach();
}
