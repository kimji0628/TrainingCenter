#pragma once
#include "Resource.h"

// ============================================================================
// ImageViewDlg.h - Image zoom view dialog (CStringW / Unicode)
// ============================================================================

class CImageViewDlg : public CDialogEx
{
public:
    CImageViewDlg(const CStringW& strImagePath, CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_IMAGE_VIEW_DIALOG };
#endif

protected:
    CStringW m_strImagePath;
    CImage   m_image;

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()
};
