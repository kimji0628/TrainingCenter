#pragma once

class CGeneratedQuestionListBox : public CListBox
{
public:
    static constexpr int kItemHeight = 24;
    static constexpr COLORREF kAdoptedBarColor = RGB(102, 187, 106);
    static constexpr COLORREF kAdoptedBgColor = RGB(225, 245, 230);
    static constexpr COLORREF kAdoptedSelectedBgColor = RGB(200, 235, 210);
    static constexpr DWORD_PTR kAdoptedItemFlag = 0x80000000;

    static int GetQuestionIndexFromItemData(DWORD_PTR dwItemData);
    static BOOL IsAdoptedItemData(DWORD_PTR dwItemData);
    static DWORD_PTR MakeItemData(int nQuestionIndex, BOOL bAdopted);

protected:
    virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()
};
