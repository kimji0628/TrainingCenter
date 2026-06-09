#pragma once

#include "Resource.h"
#include "QuestionBookStorage.h"

class CQuestionBookListDlg : public CDialogEx
{
public:
    CQuestionBookListDlg(CWnd* pParent = nullptr);

    CStringW GetSelectedFilePath() const { return m_strSelectedFilePath; }

    enum { IDD = IDD_QUESTION_BOOK_LIST_DIALOG };

protected:
    CListCtrl m_listBooks;
    CQuestionBookInfoArray m_arrBooks;
    CStringW m_strSelectedFilePath;
    int m_nSelectedIndex;

    void RefreshList();
    void UpdateButtons();
    int GetSelectedListIndex() const;
    BOOL ConfirmDeleteSelected();

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedOpen();
    afx_msg void OnBnClickedDelete();
    afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListDblClk(NMHDR* pNMHDR, LRESULT* pResult);

    DECLARE_MESSAGE_MAP()
};
