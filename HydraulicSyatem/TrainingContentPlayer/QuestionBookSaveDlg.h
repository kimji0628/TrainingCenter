#pragma once

#include "Resource.h"

class CQuestionBookSaveDlg : public CDialogEx
{
public:
    CQuestionBookSaveDlg(CWnd* pParent = nullptr);

    CStringW GetBookName() const { return m_strBookName; }

    enum { IDD = IDD_QUESTION_BOOK_SAVE_DIALOG };

protected:
    CStringW m_strBookName;

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnBnClickedOk();

    DECLARE_MESSAGE_MAP()
};
