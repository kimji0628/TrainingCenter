#pragma once
#include "Resource.h"

// ============================================================================
// VideoViewDlg.h - Single in-app WebView2 video player (one video at a time)
// ============================================================================

class CVideoViewDlg : public CDialogEx
{
public:
    static BOOL EnsureCreated(CWnd* pParent, CWnd* pHostArea = nullptr);
    static BOOL PlayVideo(CWnd* pParent, const CStringW& strUrl, CWnd* pHostArea = nullptr);
    static void HideActive();
    static void SyncHostArea(CWnd* pHostArea);
    static void Shutdown();

    explicit CVideoViewDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_VIDEO_VIEW_DIALOG };
#endif

protected:
    CStringW m_strUrl;
    CStringW m_strPendingUrl;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_spWebViewController;
    Microsoft::WRL::ComPtr<ICoreWebView2>         m_spWebView;
    EventRegistrationToken m_navigationCompletedToken;
    BOOL m_bWebViewReady;
    BOOL m_bInitFailed;
    BOOL m_bInitStarted;
    HRESULT m_hrInitError;
    BOOL m_bPendingVideoPlay;
    BOOL m_bWaitingForBlank;
    BOOL m_bHasActiveVideo;
    BOOL m_bWebViewCleanedUp;

    static CVideoViewDlg* s_pActiveDlg;

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    virtual void OnCancel();

    void PrepareHiddenForInit(CWnd* pHostArea);
    void PositionInHostArea(CWnd* pHostArea);
    void InitWebView();
    void ShowInitError();
    void ConfigureWebView();
    void SetupVirtualHost();
    void SetupMediaVirtualHost();
    BOOL WaitForWebViewReady(DWORD dwTimeoutMs);
    void PumpPendingMessages();
    void ClearIframeMedia();
    void NavigateToUrl(const CStringW& strUrl);
    void LoadPlayerPage(const CStringW& strUrl);
    void StopAndBlank();
    void HidePlayer();
    void ResizeWebView();
    void CleanupWebView();
    void OnNavigationCompleted(ICoreWebView2NavigationCompletedEventArgs* pArgs);

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnDestroy();
    afx_msg void OnClose();
    afx_msg LRESULT OnDeferredInitWebView(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
};

#define WM_APP_INIT_WEBVIEW2 (WM_APP + 200)
