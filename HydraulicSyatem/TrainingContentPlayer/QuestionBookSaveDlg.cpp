#include "pch.h"
#include "QuestionBookSaveDlg.h"
#include "QuestionBookStorage.h"
#include "Resource.h"
#include "Util.h"

CQuestionBookSaveDlg::CQuestionBookSaveDlg(CWnd* pParent)
    : CDialogEx(IDD_QUESTION_BOOK_SAVE_DIALOG, pParent)
{
}

void CQuestionBookSaveDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_QBOOK_SAVE_NAME, m_strBookName);
}

BEGIN_MESSAGE_MAP(CQuestionBookSaveDlg, CDialogEx)
    ON_BN_CLICKED(IDOK, &CQuestionBookSaveDlg::OnBnClickedOk)
END_MESSAGE_MAP()

BOOL CQuestionBookSaveDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    TrainingUtil::ApplyKoreanFont(this);
    SetWindowText(L"최종 저장");
    SetDlgItemText(IDC_QBOOK_SAVE_LABEL, L"문제집 이름 :");
    SetDlgItemText(IDOK, L"저장");
    SetDlgItemText(IDCANCEL, L"취소");

    CWnd* pEdit = GetDlgItem(IDC_QBOOK_SAVE_NAME);
    if (pEdit != nullptr)
        pEdit->SetFocus();

    return FALSE;
}

void CQuestionBookSaveDlg::OnBnClickedOk()
{
    UpdateData(TRUE);

    const CStringW strSafeName = QuestionBookStorage::SanitizeBookName(m_strBookName);
    if (strSafeName.IsEmpty())
    {
        AfxMessageBox(L"문제집 이름을 입력하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_strBookName = strSafeName;
    CDialogEx::OnOK();
}
