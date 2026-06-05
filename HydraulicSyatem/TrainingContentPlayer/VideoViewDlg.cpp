#include "pch.h"
#include "VideoViewDlg.h"
#include "Util.h"
#include <wrl/event.h>

#pragma comment(lib, "WebView2LoaderStatic.lib")

// ============================================================================
// VideoViewDlg.cpp - Single in-app WebView2 video player implementation
// ============================================================================

CVideoViewDlg* CVideoViewDlg::s_pActiveDlg = nullptr;

CVideoViewDlg::CVideoViewDlg(CWnd* pParent)
    : CDialogEx(IDD_VIDEO_VIEW_DIALOG, pParent)
    , m_bWebViewReady(FALSE)
    , m_bInitFailed(FALSE)
    , m_bInitStarted(FALSE)
    , m_hrInitError(S_OK)
    , m_bPendingVideoPlay(FALSE)
    , m_bWaitingForBlank(FALSE)
    , m_bHasActiveVideo(FALSE)
    , m_bWebViewCleanedUp(FALSE)
    , m_navigationCompletedToken{}
{
}

void CVideoViewDlg::PositionInHostArea(CWnd* pHostArea)
{
    if (pHostArea == nullptr || !::IsWindow(pHostArea->GetSafeHwnd()))
        return;

    CWnd* pParentWnd = GetParent();
    if (pParentWnd == nullptr)
        return;

    CRect rcHost;
    pHostArea->GetWindowRect(&rcHost);
    pParentWnd->ScreenToClient(&rcHost);

    SetWindowPos(
        &CWnd::wndTop,
        rcHost.left,
        rcHost.top,
        rcHost.Width(),
        rcHost.Height(),
        SWP_SHOWWINDOW);
    ResizeWebView();
}

void CVideoViewDlg::PrepareHiddenForInit(CWnd* pHostArea)
{
    if (pHostArea != nullptr && ::IsWindow(pHostArea->GetSafeHwnd()))
    {
        CWnd* pParentWnd = GetParent();
        if (pParentWnd != nullptr)
        {
            CRect rcHost;
            pHostArea->GetWindowRect(&rcHost);
            pParentWnd->ScreenToClient(&rcHost);
            SetWindowPos(
                nullptr,
                rcHost.left,
                rcHost.top,
                rcHost.Width(),
                rcHost.Height(),
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_HIDEWINDOW);
            return;
        }
    }

    ShowWindow(SW_HIDE);
}

BOOL CVideoViewDlg::EnsureCreated(CWnd* pParent, CWnd* pHostArea)
{
    if (s_pActiveDlg != nullptr && ::IsWindow(s_pActiveDlg->GetSafeHwnd()))
        return TRUE;

    CVideoViewDlg* pDlg = new CVideoViewDlg(pParent);
    if (!pDlg->Create(IDD_VIDEO_VIEW_DIALOG, pParent))
    {
        delete pDlg;
        return FALSE;
    }

    s_pActiveDlg = pDlg;
    pDlg->PrepareHiddenForInit(pHostArea);
    return TRUE;
}

void CVideoViewDlg::PumpPendingMessages()
{
    MSG msg;
    while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
}

BOOL CVideoViewDlg::WaitForWebViewReady(DWORD dwTimeoutMs)
{
    const DWORD dwStart = GetTickCount();

    while (!m_bWebViewReady && !m_bInitFailed && (GetTickCount() - dwStart) < dwTimeoutMs)
    {
        PumpPendingMessages();
        Sleep(30);
    }

    return m_bWebViewReady;
}

BOOL CVideoViewDlg::PlayVideo(CWnd* pParent, const CStringW& strUrl, CWnd* pHostArea)
{
    if (strUrl.IsEmpty())
        return FALSE;

    if (!EnsureCreated(pParent, pHostArea))
        return FALSE;

    if (!s_pActiveDlg->WaitForWebViewReady(30000))
    {
        s_pActiveDlg->ShowInitError();
        return FALSE;
    }

    // Always play inside the video dialog only (no external browser)
    s_pActiveDlg->NavigateToUrl(strUrl);

    if (pHostArea != nullptr)
        s_pActiveDlg->PositionInHostArea(pHostArea);
    else
    {
        s_pActiveDlg->CenterWindow(pParent);
        s_pActiveDlg->ShowWindow(SW_SHOW);
        s_pActiveDlg->ResizeWebView();
    }

    if (s_pActiveDlg->m_spWebViewController)
        s_pActiveDlg->m_spWebViewController->put_IsVisible(TRUE);

    // Wait for blank-then-load sequence to finish when switching videos
    if (s_pActiveDlg->m_bWaitingForBlank)
    {
        const DWORD dwStart = GetTickCount();
        while (s_pActiveDlg->m_bWaitingForBlank && (GetTickCount() - dwStart) < 5000)
        {
            s_pActiveDlg->PumpPendingMessages();
            Sleep(20);
        }
    }

    return TRUE;
}

void CVideoViewDlg::HideActive()
{
    if (s_pActiveDlg != nullptr && ::IsWindow(s_pActiveDlg->GetSafeHwnd()))
        s_pActiveDlg->HidePlayer();
}

void CVideoViewDlg::SyncHostArea(CWnd* pHostArea)
{
    if (s_pActiveDlg == nullptr || !::IsWindow(s_pActiveDlg->GetSafeHwnd()))
        return;

    if (!s_pActiveDlg->IsWindowVisible())
        return;

    s_pActiveDlg->PositionInHostArea(pHostArea);
}

void CVideoViewDlg::Shutdown()
{
    if (s_pActiveDlg == nullptr)
        return;

    CVideoViewDlg* pDlg = s_pActiveDlg;
    s_pActiveDlg = nullptr;

    pDlg->CleanupWebView();

    if (::IsWindow(pDlg->GetSafeHwnd()))
        pDlg->DestroyWindow();

    delete pDlg;
}

void CVideoViewDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CVideoViewDlg, CDialogEx)
    ON_WM_SIZE()
    ON_WM_DESTROY()
    ON_WM_CLOSE()
    ON_MESSAGE(WM_APP_INIT_WEBVIEW2, &CVideoViewDlg::OnDeferredInitWebView)
END_MESSAGE_MAP()

BOOL CVideoViewDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    TrainingUtil::ApplyKoreanFont(this);

    // Defer WebView2 init until the main dialog message loop is running
    PostMessage(WM_APP_INIT_WEBVIEW2, 0, 0);
    return TRUE;
}

LRESULT CVideoViewDlg::OnDeferredInitWebView(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    InitWebView();
    return 0;
}

void CVideoViewDlg::ShowInitError()
{
    CStringW strBrowserFolder = TrainingUtil::GetWebView2BrowserExecutableFolder();
    CStringW strMessage;

    if (strBrowserFolder.IsEmpty())
    {
        strMessage =
            L"WebView2 동영상 플레이어를 초기화하지 못했습니다.\n"
            L"Microsoft Edge WebView2 Runtime이 설치되어 있지 않거나 찾을 수 없습니다.\n\n"
            L"https://developer.microsoft.com/microsoft-edge/webview2/ 에서\n"
            L"Evergreen WebView2 Runtime을 설치한 후 다시 실행하세요.";
    }
    else if (FAILED(m_hrInitError))
    {
        strMessage.Format(
            L"WebView2 동영상 플레이어를 초기화하지 못했습니다.\n"
            L"오류 코드: 0x%08X\n"
            L"런타임 경로: %s",
            static_cast<unsigned>(m_hrInitError),
            strBrowserFolder.GetString());
    }
    else
    {
        strMessage =
            L"WebView2 동영상 플레이어 초기화 시간이 초과되었습니다.\n"
            L"잠시 후 다시 시도하거나 프로그램을 재시작하세요.";
    }

    AfxMessageBox(strMessage, MB_OK | MB_ICONERROR);
}

void CVideoViewDlg::InitWebView()
{
    if (m_bInitStarted || m_bWebViewReady)
        return;

    m_bInitStarted = TRUE;
    m_bInitFailed = FALSE;
    m_hrInitError = S_OK;

    WCHAR szUserData[MAX_PATH] = { 0 };
    ::GetTempPathW(MAX_PATH, szUserData);
    ::wcscat_s(szUserData, L"TrainingContentPlayer_WebView2\\");
    ::CreateDirectoryW(szUserData, nullptr);

    Microsoft::WRL::ComPtr<ICoreWebView2EnvironmentOptions> spOptions =
        Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    spOptions->put_AdditionalBrowserArguments(
        L"--autoplay-policy=no-user-gesture-required --disable-features=ElasticOverscroll");

    CStringW strBrowserFolder = TrainingUtil::GetWebView2BrowserExecutableFolder();
    LPCWSTR pszBrowserFolder = strBrowserFolder.IsEmpty() ? nullptr : strBrowserFolder.GetString();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        pszBrowserFolder,
        szUserData,
        spOptions.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* pEnvironment) -> HRESULT
            {
                if (FAILED(result) || pEnvironment == nullptr)
                {
                    m_hrInitError = FAILED(result) ? result : E_FAIL;
                    m_bInitFailed = TRUE;
                    return result;
                }

                return pEnvironment->CreateCoreWebView2Controller(
                    m_hWnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result, ICoreWebView2Controller* pController) -> HRESULT
                        {
                            if (FAILED(result) || pController == nullptr)
                            {
                                m_hrInitError = FAILED(result) ? result : E_FAIL;
                                m_bInitFailed = TRUE;
                                return result;
                            }

                            m_spWebViewController = pController;
                            m_spWebViewController->get_CoreWebView2(&m_spWebView);

                            ConfigureWebView();
                            m_bWebViewReady = TRUE;
                            ResizeWebView();
                            StopAndBlank();

                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr))
    {
        m_hrInitError = hr;
        m_bInitFailed = TRUE;
    }
}

void CVideoViewDlg::SetupVirtualHost()
{
    Microsoft::WRL::ComPtr<ICoreWebView2_3> spWebView3;
    if (FAILED(m_spWebView.As(&spWebView3)) || !spWebView3)
        return;

    CStringW strFolder = TrainingUtil::GetYoutubePlayerFolder();
    spWebView3->SetVirtualHostNameToFolderMapping(
        L"trainingcontentplayer.local",
        strFolder,
        COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
}

void CVideoViewDlg::OnNavigationCompleted(ICoreWebView2NavigationCompletedEventArgs* pArgs)
{
    if (m_bWaitingForBlank)
    {
        m_bWaitingForBlank = FALSE;
        if (!m_strPendingUrl.IsEmpty())
        {
            CStringW strNext = m_strPendingUrl;
            m_strPendingUrl.Empty();
            LoadPlayerPage(strNext);
        }
        return;
    }

    if (m_bPendingVideoPlay)
        m_bPendingVideoPlay = FALSE;
}

void CVideoViewDlg::ConfigureWebView()
{
    if (!m_spWebView)
        return;

    SetupVirtualHost();

    Microsoft::WRL::ComPtr<ICoreWebView2Settings> spSettings;
    if (SUCCEEDED(m_spWebView->get_Settings(&spSettings)) && spSettings)
    {
        spSettings->put_IsStatusBarEnabled(FALSE);
        spSettings->put_AreDefaultScriptDialogsEnabled(TRUE);
        spSettings->put_IsZoomControlEnabled(FALSE);
    }

    m_spWebView->add_PermissionRequested(
        Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
            [](ICoreWebView2* /*pSender*/, ICoreWebView2PermissionRequestedEventArgs* pArgs) -> HRESULT
            {
                pArgs->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
                return S_OK;
            }).Get(),
        nullptr);

    m_spWebView->add_NavigationCompleted(
        Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* /*pSender*/, ICoreWebView2NavigationCompletedEventArgs* pArgs) -> HRESULT
            {
                OnNavigationCompleted(pArgs);
                return S_OK;
            }).Get(),
        &m_navigationCompletedToken);
}

void CVideoViewDlg::ClearIframeMedia()
{
    if (!m_spWebView)
        return;

    m_spWebView->ExecuteScript(
        L"(function(){"
        L"var f=document.querySelector('iframe');"
        L"if(f){f.src='about:blank';f.remove();}"
        L"})();",
        nullptr);
}

void CVideoViewDlg::StopAndBlank()
{
    if (m_bWebViewCleanedUp || !m_bWebViewReady || !m_spWebView)
        return;

    m_bPendingVideoPlay = FALSE;
    m_bWaitingForBlank = FALSE;
    m_bHasActiveVideo = FALSE;
    m_strPendingUrl.Empty();
    m_strUrl.Empty();

    ClearIframeMedia();
    m_spWebView->Stop();
    m_spWebView->Navigate(L"about:blank");
}

void CVideoViewDlg::LoadPlayerPage(const CStringW& strUrl)
{
    if (!m_bWebViewReady || !m_spWebView)
        return;

    m_strUrl = strUrl;
    m_bPendingVideoPlay = TRUE;
    m_bHasActiveVideo = TRUE;

    CStringW strPlayerUrl;
    if (TrainingUtil::PrepareYoutubePlayerPage(strUrl, strPlayerUrl))
        m_spWebView->Navigate(strPlayerUrl);
    else
        m_spWebView->Navigate(strUrl);
}

void CVideoViewDlg::NavigateToUrl(const CStringW& strUrl)
{
    m_strUrl = strUrl;

    if (!m_bWebViewReady || !m_spWebView)
        return;

    if (!m_bHasActiveVideo)
    {
        LoadPlayerPage(strUrl);
        return;
    }

    // Step 1: stop previous video completely, then load new one in NavigationCompleted
    m_strPendingUrl = strUrl;
    m_bWaitingForBlank = TRUE;
    m_bPendingVideoPlay = FALSE;

    ClearIframeMedia();
    m_spWebView->Stop();
    m_spWebView->Navigate(L"about:blank");
}

void CVideoViewDlg::HidePlayer()
{
    StopAndBlank();
    ShowWindow(SW_HIDE);
}

void CVideoViewDlg::ResizeWebView()
{
    if (m_bWebViewCleanedUp || !m_spWebViewController)
        return;

    CRect rcClient;
    GetClientRect(&rcClient);

    if (rcClient.Width() <= 0 || rcClient.Height() <= 0)
        return;

    RECT rcBounds = { 0, 0, rcClient.Width(), rcClient.Height() };
    m_spWebViewController->put_Bounds(rcBounds);
}

void CVideoViewDlg::CleanupWebView()
{
    if (m_bWebViewCleanedUp)
        return;

    m_bWebViewCleanedUp = TRUE;
    m_bWebViewReady = FALSE;
    m_bInitFailed = FALSE;
    m_bInitStarted = FALSE;
    m_bHasActiveVideo = FALSE;
    m_bWaitingForBlank = FALSE;
    m_bPendingVideoPlay = FALSE;
    m_strPendingUrl.Empty();
    m_strUrl.Empty();

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> spController;
    Microsoft::WRL::ComPtr<ICoreWebView2> spWebView;
    spController.Attach(m_spWebViewController.Detach());
    spWebView.Attach(m_spWebView.Detach());

    if (spController)
    {
        spController->put_IsVisible(FALSE);
        spController->Close();
    }

    spWebView = nullptr;
    spController = nullptr;
}

void CVideoViewDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    ResizeWebView();
}

void CVideoViewDlg::OnDestroy()
{
    CleanupWebView();
    CDialogEx::OnDestroy();
}

void CVideoViewDlg::OnCancel()
{
    HidePlayer();
}

void CVideoViewDlg::OnClose()
{
    HidePlayer();
}
