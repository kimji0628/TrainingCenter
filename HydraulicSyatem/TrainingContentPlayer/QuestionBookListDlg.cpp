#include "pch.h"
#include "QuestionBookListDlg.h"
#include "Resource.h"
#include "Util.h"

CQuestionBookListDlg::CQuestionBookListDlg(CWnd* pParent)
    : CDialogEx(IDD_QUESTION_BOOK_LIST_DIALOG, pParent)
    , m_nSelectedIndex(-1)
{
}

void CQuestionBookListDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_QBOOK_LIST, m_listBooks);
}

BEGIN_MESSAGE_MAP(CQuestionBookListDlg, CDialogEx)
    ON_BN_CLICKED(IDC_QBOOK_LIST_OPEN, &CQuestionBookListDlg::OnBnClickedOpen)
    ON_BN_CLICKED(IDC_QBOOK_LIST_DELETE, &CQuestionBookListDlg::OnBnClickedDelete)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_QBOOK_LIST, &CQuestionBookListDlg::OnListItemChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_QBOOK_LIST, &CQuestionBookListDlg::OnListDblClk)
END_MESSAGE_MAP()

BOOL CQuestionBookListDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    TrainingUtil::ApplyKoreanFont(this);
    SetWindowText(L"저장된 문제집");
    SetDlgItemText(IDC_QBOOK_LIST_OPEN, L"열기");
    SetDlgItemText(IDC_QBOOK_LIST_DELETE, L"삭제");
    SetDlgItemText(IDCANCEL, L"닫기");

    m_listBooks.SetExtendedStyle(
        m_listBooks.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    m_listBooks.InsertColumn(0, L"문제집 이름", LVCFMT_LEFT, 220);
    m_listBooks.InsertColumn(1, L"저장 날짜", LVCFMT_LEFT, 140);
    m_listBooks.InsertColumn(2, L"문제 개수", LVCFMT_RIGHT, 80);

    CStringW strError;
    if (!QuestionBookStorage::ListQuestionBooks(m_arrBooks, strError))
    {
        AfxMessageBox(strError, MB_OK | MB_ICONWARNING);
        return TRUE;
    }

    RefreshList();
    UpdateButtons();
    return TRUE;
}

void CQuestionBookListDlg::RefreshList()
{
    m_listBooks.DeleteAllItems();

    for (int i = 0; i < static_cast<int>(m_arrBooks.size()); ++i)
    {
        const QUESTION_BOOK_INFO& info = m_arrBooks[static_cast<size_t>(i)];

        const int nItem = m_listBooks.InsertItem(i, info.strName);
        if (nItem < 0)
            continue;

        m_listBooks.SetItemText(nItem, 1, info.strSavedAt);

        CStringW strCount;
        strCount.Format(L"%d문제", info.nQuestionCount);
        m_listBooks.SetItemText(nItem, 2, strCount);
        m_listBooks.SetItemData(nItem, static_cast<DWORD_PTR>(i));
    }

    if (m_listBooks.GetItemCount() > 0)
        m_listBooks.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

void CQuestionBookListDlg::UpdateButtons()
{
    const BOOL bHasSelection = GetSelectedListIndex() >= 0;
    const BOOL bHasItems = !m_arrBooks.empty();

    if (CWnd* pOpen = GetDlgItem(IDC_QBOOK_LIST_OPEN))
        pOpen->EnableWindow(bHasSelection);
    if (CWnd* pDelete = GetDlgItem(IDC_QBOOK_LIST_DELETE))
        pDelete->EnableWindow(bHasSelection && bHasItems);
}

int CQuestionBookListDlg::GetSelectedListIndex() const
{
    int nItem = -1;
    for (int i = 0; i < m_listBooks.GetItemCount(); ++i)
    {
        if ((m_listBooks.GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0)
        {
            nItem = i;
            break;
        }
    }

    if (nItem < 0)
        return -1;

    const DWORD_PTR dwData = m_listBooks.GetItemData(nItem);
    return static_cast<int>(dwData);
}

BOOL CQuestionBookListDlg::ConfirmDeleteSelected()
{
    const int nIndex = GetSelectedListIndex();
    if (nIndex < 0 || nIndex >= static_cast<int>(m_arrBooks.size()))
        return FALSE;

    const QUESTION_BOOK_INFO& info = m_arrBooks[static_cast<size_t>(nIndex)];
    CStringW strMsg;
    strMsg.Format(
        L"정말 삭제하시겠습니까?\r\n\r\n문제집: %s\r\n문제 수: %d",
        info.strName.GetString(),
        info.nQuestionCount);

    return AfxMessageBox(strMsg, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
}

void CQuestionBookListDlg::OnBnClickedOpen()
{
    const int nIndex = GetSelectedListIndex();
    if (nIndex < 0 || nIndex >= static_cast<int>(m_arrBooks.size()))
    {
        AfxMessageBox(L"열 문제집을 선택하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_strSelectedFilePath = m_arrBooks[static_cast<size_t>(nIndex)].strFilePath;
    CDialogEx::OnOK();
}

void CQuestionBookListDlg::OnBnClickedDelete()
{
    const int nIndex = GetSelectedListIndex();
    if (nIndex < 0 || nIndex >= static_cast<int>(m_arrBooks.size()))
    {
        AfxMessageBox(L"삭제할 문제집을 선택하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!ConfirmDeleteSelected())
        return;

    const CStringW strFilePath = m_arrBooks[static_cast<size_t>(nIndex)].strFilePath;
    CStringW strError;
    if (!QuestionBookStorage::DeleteQuestionBook(strFilePath, strError))
    {
        AfxMessageBox(strError, MB_OK | MB_ICONWARNING);
        return;
    }

    m_arrBooks.erase(m_arrBooks.begin() + nIndex);
    RefreshList();
    UpdateButtons();
}

void CQuestionBookListDlg::OnListItemChanged(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    UpdateButtons();
    if (pResult != nullptr)
        *pResult = 0;
}

void CQuestionBookListDlg::OnListDblClk(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    OnBnClickedOpen();
    if (pResult != nullptr)
        *pResult = 0;
}
