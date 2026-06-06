#include "pch.h"
#include "StartDlg.h"
#include "Util.h"

// ============================================================================
// StartDlg.cpp - 메인 창 위 안내 팝업
// ============================================================================

CStartDlg::CStartDlg(CWnd* pParent)
    : CDialogEx(IDD_START_DIALOG, pParent)
{
}

void CStartDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CStartDlg, CDialogEx)
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_BN_CLICKED(IDC_BTN_START, &CStartDlg::OnBnClickedBtnStart)
END_MESSAGE_MAP()

BOOL CStartDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_brushBackground.CreateSolidBrush(RGB(235, 245, 255));

    m_fontTitleKo.CreatePointFont(135, L"Malgun Gothic");
    m_fontTitleEn.CreatePointFont(119, L"Malgun Gothic");
    m_fontDesc.CreatePointFont(105, L"Malgun Gothic");
    m_fontFooter.CreatePointFont(92, L"Malgun Gothic");

    CStatic* pIcon = static_cast<CStatic*>(GetDlgItem(IDC_START_ICON));
    if (pIcon != nullptr)
    {
        HICON hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
        if (hIcon != nullptr)
            pIcon->SetIcon(hIcon);
    }

    auto setFont = [this](UINT nID, CFont& font)
    {
        CWnd* pWnd = GetDlgItem(nID);
        if (pWnd != nullptr)
            pWnd->SetFont(&font);
    };

    setFont(IDC_START_TITLE_KO, m_fontTitleKo);
    setFont(IDC_START_TITLE_EN, m_fontTitleEn);
    setFont(IDC_START_DESC, m_fontDesc);
    setFont(IDC_START_COMPANY, m_fontFooter);
    setFont(IDC_START_VERSION, m_fontFooter);

    SetDlgItemText(IDC_START_TITLE_KO, L"AI/AX 스마트 강의 플레이어");
    SetDlgItemText(IDC_START_TITLE_EN, L"Smart Lecture Player");
    SetDlgItemText(IDC_START_DESC, L"교육 콘텐츠 통합 재생 도구");
    SetDlgItemText(IDC_START_COMPANY, L"Developed by EZTech");
    SetDlgItemText(IDC_START_VERSION, L"Version 1.0");
    SetDlgItemText(IDC_BTN_START, L"시작하기");

    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_START), 122);

    return TRUE;
}

void CStartDlg::OnBnClickedBtnStart()
{
    CWnd* pParent = GetParent();
    if (pParent != nullptr && ::IsWindow(pParent->GetSafeHwnd()))
        pParent->ShowWindow(SW_MAXIMIZE);

    EndDialog(IDOK);
}

BOOL CStartDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);
    pDC->FillRect(&rc, &m_brushBackground);
    return TRUE;
}

HBRUSH CStartDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_DLG || nCtlColor == CTLCOLOR_STATIC)
    {
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetTextColor(RGB(45, 55, 75));
        return static_cast<HBRUSH>(m_brushBackground.GetSafeHandle());
    }

    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}
