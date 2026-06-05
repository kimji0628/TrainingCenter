#include "pch.h"
#include "ImageViewDlg.h"
#include "Util.h"

// ============================================================================
// ImageViewDlg.cpp - 이미지 확대 보기 다이얼로그 구현
// ============================================================================

CImageViewDlg::CImageViewDlg(const CStringW& strImagePath, CWnd* pParent)
    : CDialogEx(IDD_IMAGE_VIEW_DIALOG, pParent)
    , m_strImagePath(strImagePath)
{
}

void CImageViewDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CImageViewDlg, CDialogEx)
END_MESSAGE_MAP()

BOOL CImageViewDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetWindowText(L"이미지 보기");
    TrainingUtil::ApplyKoreanFont(this);

    CStringW strFullPath = TrainingUtil::ResolveAppPath(m_strImagePath);
    HRESULT hr = m_image.Load(strFullPath);
    if (FAILED(hr))
    {
        AfxMessageBox(L"이미지 파일을 불러올 수 없습니다.\n" + strFullPath,
            MB_OK | MB_ICONWARNING);
        return TRUE;
    }

    // 이미지 크기에 맞게 다이얼로그 크기 조정 (최대 900x700)
    const int nMaxWidth = 900;
    const int nMaxHeight = 700;
    int nImgWidth = m_image.GetWidth();
    int nImgHeight = m_image.GetHeight();

    int nDisplayWidth = min(nImgWidth, nMaxWidth);
    int nDisplayHeight = min(nImgHeight, nMaxHeight);

    if (nImgWidth > nMaxWidth || nImgHeight > nMaxHeight)
    {
        double dScaleW = static_cast<double>(nMaxWidth) / nImgWidth;
        double dScaleH = static_cast<double>(nMaxHeight) / nImgHeight;
        double dScale = min(dScaleW, dScaleH);
        nDisplayWidth = static_cast<int>(nImgWidth * dScale);
        nDisplayHeight = static_cast<int>(nImgHeight * dScale);
    }

    CRect rcClient, rcWindow;
    GetClientRect(&rcClient);
    GetWindowRect(&rcWindow);

    int nBorderW = rcWindow.Width() - rcClient.Width();
    int nBorderH = rcWindow.Height() - rcClient.Height();

    SetWindowPos(nullptr, 0, 0,
        nDisplayWidth + nBorderW,
        nDisplayHeight + nBorderH,
        SWP_NOMOVE | SWP_NOZORDER);

    CenterWindow();

    CWnd* pStatic = GetDlgItem(IDC_STATIC_IMAGE_VIEW);
    if (pStatic != nullptr)
    {
        pStatic->MoveWindow(0, 0, nDisplayWidth, nDisplayHeight);

        CDC* pDC = pStatic->GetDC();
        if (pDC != nullptr)
        {
            m_image.StretchBlt(pDC->m_hDC, 0, 0, nDisplayWidth, nDisplayHeight,
                0, 0, nImgWidth, nImgHeight, SRCCOPY);
            pStatic->ReleaseDC(pDC);
        }
    }

    return TRUE;
}
