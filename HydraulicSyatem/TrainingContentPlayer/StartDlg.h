#pragma once

#include "Resource.h"

// ============================================================================
// StartDlg.h - 메인 창 위 안내 팝업 (Class Wizard: CDialogEx)
// ============================================================================

class CStartDlg : public CDialogEx
{
public:
    CStartDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_START_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedBtnStart();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);

    DECLARE_MESSAGE_MAP()

private:
    CBrush m_brushBackground;
    CFont m_fontTitleKo;
    CFont m_fontTitleEn;
    CFont m_fontDesc;
    CFont m_fontFooter;
};
