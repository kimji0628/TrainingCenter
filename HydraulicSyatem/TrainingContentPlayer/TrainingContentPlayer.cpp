#include "pch.h"
#include "framework.h"
#include "TrainingContentPlayer.h"
#include "TrainingDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ============================================================================
// TrainingContentPlayer.cpp - MFC 애플리케이션 진입점
// ============================================================================

CTrainingContentPlayerApp theApp;

CTrainingContentPlayerApp::CTrainingContentPlayerApp()
{
}

BEGIN_MESSAGE_MAP(CTrainingContentPlayerApp, CWinApp)
END_MESSAGE_MAP()

BOOL CTrainingContentPlayerApp::InitInstance()
{
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();

    AfxInitRichEdit2();

    if (!AfxOleInit())
    {
        AfxMessageBox(L"COM 초기화에 실패했습니다.", MB_OK | MB_ICONERROR);
        return FALSE;
    }

    CTrainingDlg dlg;
    m_pMainWnd = &dlg;
    INT_PTR nResponse = dlg.DoModal();
    m_pMainWnd = nullptr;

    if (nResponse == IDOK)
    {
        // TODO: OK 버튼 처리
    }
    else if (nResponse == IDCANCEL)
    {
        // TODO: Cancel 버튼 처리
    }

    return FALSE;
}
