#include "pch.h"
#include "QuizGenViewCtrl.h"
#include "QuestionTestLoader.h"
#include "ScpConfigReader.h"
#include "ScpUiSettings.h"
#include "ScpPaths.h"
#include "OpenAiClient.h"
#include "Resource.h"

#include <fstream>
#include "Util.h"

#include <memory>
#include <thread>

IMPLEMENT_DYNAMIC(CQuizGenViewCtrl, CWnd)

namespace
{
    constexpr int PANEL_GAP = 8;
    constexpr int GENERATED_ACTION_H = 32;
    constexpr int CHATGPT_PROGRESS_H = 22;
    constexpr int ROW_GAP = 8;
    constexpr int ROW_H = 26;
    constexpr int SPLITTER_WIDTH = 6;
    constexpr int PANE_MIN_WIDTH = 160;
    constexpr int TAB_CTRL_H = 24;
    constexpr int BANK_TOOLBAR_H = 28;
    constexpr int BANK_STATUS_H = 28;
    constexpr int BANK_BOTTOM_PANEL_H = BANK_TOOLBAR_H + 8 + BANK_STATUS_H + 4;
    constexpr int GENERATE_BTN_H = 32;
    constexpr int GENERATED_PANE_MIN_H = 150;
    constexpr int GENERATED_SPLITTER_WIDTH = 6;
    constexpr int EMPHASIS_FONT_PT = 110;
    constexpr int GENERATE_FONT_PT = 120;
    constexpr int FORM_LABEL_W = 72;
    constexpr int PDF_COMBO_MAX_CHARS = 24;

    int GetTextWidth(const CWnd* pWnd, const CStringW& strText, int nPadding)
    {
        if (pWnd == nullptr || strText.IsEmpty())
            return nPadding;

        CClientDC dc(const_cast<CWnd*>(pWnd));
        CFont* pFont = pWnd->GetFont();
        if (pFont == nullptr && pWnd->GetParent() != nullptr)
            pFont = pWnd->GetParent()->GetFont();

        CFont* pOld = dc.SelectObject(pFont);
        CSize sz = dc.GetTextExtent(strText);
        dc.SelectObject(pOld);

        return sz.cx + nPadding;
    }

    int GetEditWidthForChars(const CWnd* pWnd, int nChars)
    {
        if (pWnd == nullptr || nChars <= 0)
            return 120;

        CStringW strSample;
        strSample.Preallocate(nChars);
        for (int i = 0; i < nChars; ++i)
            strSample += L'가';
        return GetTextWidth(pWnd, strSample, 28);
    }

    int GetTextWidthWithFont(const CWnd* pWnd, const CStringW& strText, CFont* pFont, int nPadding)
    {
        if (pWnd == nullptr || strText.IsEmpty())
            return nPadding;

        CClientDC dc(const_cast<CWnd*>(pWnd));
        CFont* pUseFont = pFont ? pFont : pWnd->GetFont();
        CFont* pOld = dc.SelectObject(pUseFont);
        CSize sz = dc.GetTextExtent(strText);
        dc.SelectObject(pOld);

        return sz.cx + nPadding;
    }

    void CalcGeneratedPaneHeights(
        int nBodyHeight,
        double dListSplitRatio,
        int& nListHeight,
        int& nDetailHeight)
    {
        nListHeight = 0;
        nDetailHeight = 0;

        if (nBodyHeight <= GENERATED_SPLITTER_WIDTH)
            return;

        const int nSplitAreaH = nBodyHeight - GENERATED_SPLITTER_WIDTH;
        dListSplitRatio = max(0.05, min(0.95, dListSplitRatio));

        nListHeight = static_cast<int>(nSplitAreaH * dListSplitRatio);
        nListHeight = max(
            GENERATED_PANE_MIN_H,
            min(nListHeight, nSplitAreaH - GENERATED_PANE_MIN_H));
        nDetailHeight = nSplitAreaH - nListHeight;
    }
}

BEGIN_MESSAGE_MAP(CQuizGenViewCtrl, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
    ON_NOTIFY(TCN_SELCHANGE, IDC_QUIZGEN_TAB, &CQuizGenViewCtrl::OnTcnSelchangeTab)
    ON_LBN_SELCHANGE(IDC_QUIZGEN_GENERATED_LIST, &CQuizGenViewCtrl::OnLbnSelchangeGeneratedList)
    ON_NOTIFY(EN_SELCHANGE, IDC_QUIZGEN_BANK_LIST, &CQuizGenViewCtrl::OnEnSelchangeBankRich)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_SELECT_PDF, &CQuizGenViewCtrl::OnBnClickedSelectPdf)
    ON_BN_CLICKED(IDC_QUIZGEN_RADIO_ALL_PAGES, &CQuizGenViewCtrl::OnBnClickedRadioAllPages)
    ON_BN_CLICKED(IDC_QUIZGEN_RADIO_PAGE_RANGE, &CQuizGenViewCtrl::OnBnClickedRadioPageRange)
    ON_CBN_SELCHANGE(IDC_QUIZGEN_PDF_COMBO, &CQuizGenViewCtrl::OnCbnSelchangePdfCombo)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_GENERATE, &CQuizGenViewCtrl::OnBnClickedGenerate)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_USE, &CQuizGenViewCtrl::OnBnClickedUse)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_REGENERATE, &CQuizGenViewCtrl::OnBnClickedRegenerate)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_ADD_MORE, &CQuizGenViewCtrl::OnBnClickedAddMore)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_SAVE, &CQuizGenViewCtrl::OnBnClickedSave)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_TEST, &CQuizGenViewCtrl::OnBnClickedTest)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_CHATGPT, &CQuizGenViewCtrl::OnBnClickedChatGpt)
    ON_MESSAGE(WM_QUIZGEN_CHATGPT_DONE, &CQuizGenViewCtrl::OnChatGptTestDone)
    ON_MESSAGE(WM_QUIZGEN_GENERATE_DONE, &CQuizGenViewCtrl::OnQuestionGenerateDone)
    ON_BN_CLICKED(IDC_QUIZGEN_BANK_BTN_DELETE, &CQuizGenViewCtrl::OnBnClickedBankDelete)
    ON_BN_CLICKED(IDC_QUIZGEN_BANK_BTN_MOVE_UP, &CQuizGenViewCtrl::OnBnClickedBankMoveUp)
    ON_BN_CLICKED(IDC_QUIZGEN_BANK_BTN_MOVE_DOWN, &CQuizGenViewCtrl::OnBnClickedBankMoveDown)
END_MESSAGE_MAP()

CQuizGenViewCtrl::CQuizGenViewCtrl()
    : m_nSelectedPdfIndex(-1)
    , m_dSplitRatio(ScpUiSettings::kPdfSplitDefault)
    , m_dGeneratedListSplitRatio(ScpUiSettings::kGeneratedListSplitDefault)
    , m_bDraggingSplit(FALSE)
    , m_bDraggingGeneratedSplit(FALSE)
    , m_nCurrentTestQuestion(-1)
    , m_nSelectedBankIndex(-1)
    , m_bTestQuestionsLoaded(FALSE)
    , m_nActiveTab(TAB_GENERATED)
    , m_bChatGptTestRunning(FALSE)
    , m_bQuestionGenerateRunning(FALSE)
{
}

CQuizGenViewCtrl::~CQuizGenViewCtrl()
{
    SaveUiSettings();

    if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
        m_pdfPreview.CloseDocument();
}

void CQuizGenViewCtrl::LoadUiSettings()
{
    ScpUiSettings::LoadGeneratedListSplitRatio(m_dGeneratedListSplitRatio);
    ScpUiSettings::LoadPdfSplitRatio(m_dSplitRatio);
}

void CQuizGenViewCtrl::SaveUiSettings()
{
    ScpUiSettings::SaveQuizGenLayout(m_dGeneratedListSplitRatio, m_dSplitRatio);
}

BOOL CQuizGenViewCtrl::CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId)
{
    if (pParent == nullptr || pPlaceholder == nullptr ||
        !::IsWindow(pPlaceholder->GetSafeHwnd()))
    {
        return FALSE;
    }

    CRect rcHost;
    pPlaceholder->GetWindowRect(&rcHost);
    pParent->ScreenToClient(&rcHost);

    const DWORD dwStyle = WS_CHILD | WS_CLIPSIBLINGS;
    return Create(nullptr, nullptr, dwStyle, rcHost, pParent, nHostId);
}

BOOL CQuizGenViewCtrl::IsPreviewActive() const
{
    return ::IsWindow(m_pdfPreview.GetSafeHwnd()) &&
           m_pdfPreview.IsWindowVisible() &&
           m_pdfPreview.IsDocumentOpen();
}

BOOL CQuizGenViewCtrl::HandlePreviewMouseWheel(short zDelta, const CPoint& ptScreen)
{
    if (!IsPreviewActive())
        return FALSE;

    return m_pdfPreview.HandleMouseWheel(zDelta, ptScreen);
}

BOOL CQuizGenViewCtrl::HandlePreviewKeyDown(UINT nChar)
{
    if (!IsPreviewActive())
        return FALSE;

    return m_pdfPreview.HandleKeyDown(nChar);
}

int CQuizGenViewCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    LoadUiSettings();
    CreateChildControls();
    LoadDummyQuestionText();
    RefreshBankList();
    ShowActiveTab();
    Relayout();
    return 0;
}

void CQuizGenViewCtrl::CreateChildControls()
{
    const DWORD dwStaticStyle = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE;
    const DWORD dwEditStyle = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    const DWORD dwBtnStyle = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    const DWORD dwComboStyle = WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL;
    const DWORD dwRadioStyle = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON;
    const DWORD dwRichStyle =
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL;
    const DWORD dwGeneratedListStyle =
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT;
    const DWORD dwBankRichStyle =
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
        ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY;

    m_staticPdfLabel.Create(L"PDF 파일 :", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PDF_LABEL);
    m_comboPdf.Create(dwComboStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PDF_COMBO);
    m_btnSelectPdf.Create(L"선택", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_SELECT_PDF);

    m_radioAllPages.Create(L"전체 페이지", dwRadioStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_RADIO_ALL_PAGES);
    m_radioPageRange.Create(L"페이지 범위", dwRadioStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_RADIO_PAGE_RANGE);
    m_staticPageStartLabel.Create(L"시작", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PAGE_START_LABEL);
    m_editPageStart.Create(dwEditStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PAGE_START);
    m_staticPageEndLabel.Create(L"종료", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PAGE_END_LABEL);
    m_editPageEnd.Create(dwEditStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PAGE_END);

    m_staticCountLabel.Create(L"문제 개수 :", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_COUNT_LABEL);
    m_editQuestionCount.Create(dwEditStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_COUNT_EDIT);
    m_btnGenerate.Create(L"문제 생성", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_GENERATE);

    m_staticPreviewLabel.Create(L"PDF 미리보기", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_PREVIEW_LABEL);

    m_tabQuestion.Create(WS_CHILD | WS_VISIBLE | TCS_TABS, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_TAB);

    TCITEMW tie = {};
    tie.mask = TCIF_TEXT;
    tie.pszText = const_cast<LPWSTR>(L"생성된 문제");
    m_tabQuestion.InsertItem(TAB_GENERATED, &tie);
    tie.pszText = const_cast<LPWSTR>(L"임시 문제집");
    m_tabQuestion.InsertItem(TAB_BANK, &tie);
    m_tabQuestion.SetCurSel(TAB_GENERATED);

    CRect rcPreviewHost(0, 0, 100, 100);
    m_pdfPreview.Create(
        nullptr,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        rcPreviewHost,
        this,
        IDC_QUIZGEN_PDF_PREVIEW);
    m_pdfPreview.SetEmbeddedPreviewMode(TRUE);

    m_listGenerated.Create(dwGeneratedListStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_GENERATED_LIST);
    m_richQuestion.Create(dwRichStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_RICHEDIT);
    m_richBank.Create(dwBankRichStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BANK_LIST);
    m_richBank.SetBackgroundColor(FALSE, RGB(252, 252, 252));

    m_btnBankDelete.Create(L"삭제", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BANK_BTN_DELETE);
    m_btnBankMoveUp.Create(L"위로 이동", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BANK_BTN_MOVE_UP);
    m_btnBankMoveDown.Create(L"아래로 이동", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BANK_BTN_MOVE_DOWN);
    m_staticBankCount.Create(
        L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(0, 0, 0, 0),
        this,
        IDC_QUIZGEN_BANK_COUNT);

    m_btnUse.Create(L"문제 채택", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_USE);
    m_btnRegenerate.Create(L"재출제", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_REGENERATE);
    m_btnAddMore.Create(L"추가 생성", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_ADD_MORE);
    m_btnSave.Create(L"최종 저장", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_SAVE);
    m_btnTest.Create(L"기능 시험", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_TEST);
    m_btnChatGpt.Create(L"ChatGPT 연동", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_CHATGPT);
    m_staticChatGptProgress.Create(
        L"",
        WS_CHILD | SS_LEFT | SS_CENTERIMAGE,
        CRect(0, 0, 0, 0),
        this,
        IDC_QUIZGEN_CHATGPT_PROGRESS);
    m_staticChatGptProgress.ShowWindow(SW_HIDE);

    m_radioAllPages.SetCheck(BST_CHECKED);
    m_radioPageRange.SetCheck(BST_UNCHECKED);
    m_editPageStart.SetWindowText(L"1");
    m_editPageEnd.SetWindowText(L"1");
    m_editQuestionCount.SetWindowText(L"5");
    UpdatePageRangeEnable();

    const CWnd* arrFontControls[] = {
        &m_staticPdfLabel, &m_comboPdf, &m_btnSelectPdf,
        &m_radioAllPages, &m_radioPageRange,
        &m_staticPageStartLabel, &m_editPageStart,
        &m_staticPageEndLabel, &m_editPageEnd,
        &m_staticCountLabel, &m_editQuestionCount, &m_btnGenerate,
        &m_staticPreviewLabel, &m_tabQuestion,
        &m_listGenerated, &m_richQuestion, &m_richBank,
        &m_btnBankDelete, &m_btnBankMoveUp, &m_btnBankMoveDown, &m_staticBankCount,
        &m_btnUse, &m_btnRegenerate, &m_btnAddMore, &m_btnSave, &m_btnTest, &m_btnChatGpt,
        &m_staticChatGptProgress
    };

    for (const CWnd* pWnd : arrFontControls)
    {
        if (pWnd != nullptr && ::IsWindow(pWnd->GetSafeHwnd()))
            TrainingUtil::ApplyKoreanFont(const_cast<CWnd*>(pWnd));
    }

    ApplyEmphasisFonts();
    UpdateBankStatusLabel();
}

void CQuizGenViewCtrl::ApplyEmphasisFonts()
{
    if (m_fontEmphasis.GetSafeHandle() == nullptr)
    {
        m_fontEmphasis.CreatePointFont(EMPHASIS_FONT_PT, L"맑은 고딕");
        if (m_fontEmphasis.GetSafeHandle() == nullptr)
            m_fontEmphasis.CreatePointFont(EMPHASIS_FONT_PT, L"Malgun Gothic");
    }

    if (m_fontGenerateBtn.GetSafeHandle() == nullptr)
    {
        m_fontGenerateBtn.CreatePointFont(GENERATE_FONT_PT, L"맑은 고딕");
        if (m_fontGenerateBtn.GetSafeHandle() == nullptr)
            m_fontGenerateBtn.CreatePointFont(GENERATE_FONT_PT, L"Malgun Gothic");
    }

    if (::IsWindow(m_staticBankCount.GetSafeHwnd()))
        m_staticBankCount.SetFont(&m_fontEmphasis);
    if (::IsWindow(m_btnGenerate.GetSafeHwnd()))
        m_btnGenerate.SetFont(&m_fontGenerateBtn);
}

void CQuizGenViewCtrl::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateLayout();
}

void CQuizGenViewCtrl::Relayout()
{
    UpdateLayout();
}

void CQuizGenViewCtrl::UpdateLayout()
{
    if (!::IsWindow(m_comboPdf.GetSafeHwnd()))
        return;

    CRect rcClient;
    GetClientRect(&rcClient);

    const int nLeft = PANEL_GAP;
    const int nWidth = max(0, rcClient.Width() - PANEL_GAP * 2);
    int y = PANEL_GAP;

    auto place = [&](CWnd& wnd, int x, int yy, int w, int h)
    {
        if (::IsWindow(wnd.GetSafeHwnd()))
            wnd.SetWindowPos(nullptr, x, yy, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    const int nSelectBtnW = GetTextWidth(this, L"선택", 24);
    const int nGenerateW = GetTextWidthWithFont(this, L"문제 생성", &m_fontGenerateBtn, 36);
    const int nFieldLeft = nLeft + FORM_LABEL_W + 6;
    const int nComboW = GetEditWidthForChars(this, PDF_COMBO_MAX_CHARS);
    const int nRadioAllW = GetTextWidth(this, L"전체 페이지", 30);
    const int nRadioRangeW = GetTextWidth(this, L"페이지 범위", 30);
    const int nPageStartLabelW = GetTextWidth(this, L"시작", 8);
    const int nPageEndLabelW = GetTextWidth(this, L"종료", 8);
    const int nPageEditW = 48;
    const int nCountEditW = 48;
    const int nCountLabelW = GetTextWidth(this, L"문제 개수 :", 8);
    const int nGroupGap = 16;

    auto placeOptionRow = [&](int nStartX, int nRowY)
    {
        int nX = nStartX;
        place(m_radioAllPages, nX, nRowY, nRadioAllW, ROW_H);
        nX += nRadioAllW + nGroupGap;
        place(m_radioPageRange, nX, nRowY, nRadioRangeW, ROW_H);
        nX += nRadioRangeW + nGroupGap;
        place(m_staticPageStartLabel, nX, nRowY, nPageStartLabelW, ROW_H);
        nX += nPageStartLabelW + 6;
        place(m_editPageStart, nX, nRowY, nPageEditW, ROW_H);
        nX += nPageEditW + nGroupGap;
        place(m_staticPageEndLabel, nX, nRowY, nPageEndLabelW, ROW_H);
        nX += nPageEndLabelW + 6;
        place(m_editPageEnd, nX, nRowY, nPageEditW, ROW_H);
        nX += nPageEditW + nGroupGap;
        place(m_staticCountLabel, nX, nRowY, nCountLabelW, ROW_H);
        nX += nCountLabelW + 6;
        place(m_editQuestionCount, nX, nRowY, nCountEditW, ROW_H);
        place(m_btnGenerate, nLeft + nWidth - nGenerateW, nRowY, nGenerateW, GENERATE_BTN_H);
    };

    const int nOptionBlockW =
        nRadioAllW + nGroupGap +
        nRadioRangeW + nGroupGap +
        nPageStartLabelW + 6 + nPageEditW + nGroupGap +
        nPageEndLabelW + 6 + nPageEditW + nGroupGap +
        nCountLabelW + 6 + nCountEditW;

    const int nRow1TailW = nComboW + nGroupGap + nSelectBtnW;
    const BOOL bSingleRow = (nFieldLeft + nRow1TailW + nGroupGap + nOptionBlockW <= nLeft + nWidth);

    int nRowX = nLeft;
    place(m_staticPdfLabel, nRowX, y, FORM_LABEL_W, ROW_H);
    nRowX = nFieldLeft;
    place(m_comboPdf, nRowX, y, nComboW, ROW_H);
    nRowX += nComboW + nGroupGap;
    place(m_btnSelectPdf, nRowX, y, nSelectBtnW, ROW_H);

    if (bSingleRow)
    {
        nRowX += nSelectBtnW + nGroupGap;
        placeOptionRow(nRowX, y);
    }
    y += ROW_H + ROW_GAP;

    if (!bSingleRow)
    {
        placeOptionRow(nFieldLeft, y);
        y += ROW_H + ROW_GAP;
    }

    const int nCenterTop = y;
    const int nCenterBottom = rcClient.bottom - PANEL_GAP;
    const int nCenterHeight = max(0, nCenterBottom - nCenterTop);

    const int nSplitAreaW = max(0, nWidth - SPLITTER_WIDTH);
    const int nMinLeft = PANE_MIN_WIDTH;
    const int nMinRight = PANE_MIN_WIDTH;
    int nLeftPaneW = static_cast<int>(nSplitAreaW * m_dSplitRatio);
    nLeftPaneW = max(nMinLeft, min(nLeftPaneW, nSplitAreaW - nMinRight));
    m_dSplitRatio = (nSplitAreaW > 0)
        ? static_cast<double>(nLeftPaneW) / static_cast<double>(nSplitAreaW)
        : 0.5;

    const int nRightPaneW = max(0, nSplitAreaW - nLeftPaneW);
    const int nSplitX = nLeft + nLeftPaneW;
    const int nRightX = nSplitX + SPLITTER_WIDTH;

    place(m_staticPreviewLabel, nLeft, nCenterTop, nLeftPaneW, TAB_CTRL_H);

    const int nLeftPaneTop = nCenterTop + TAB_CTRL_H + 4;
    const int nLeftPaneHeight = max(0, nCenterBottom - nLeftPaneTop);

    if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
    {
        m_pdfPreview.SetWindowPos(
            nullptr,
            nLeft,
            nLeftPaneTop,
            nLeftPaneW,
            nLeftPaneHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    m_rcSplitter.SetRect(nSplitX, nCenterTop, nSplitX + SPLITTER_WIDTH, nCenterTop + nCenterHeight);
    InvalidateRect(&m_rcSplitter, FALSE);

    place(m_tabQuestion, nRightX, nCenterTop, nRightPaneW, TAB_CTRL_H);

    const int nContentTop = nCenterTop + TAB_CTRL_H + 4;
    const int nContentBottom = nCenterTop + nCenterHeight;
    const int nGenActionY = nContentBottom - GENERATED_ACTION_H - 4;
    const int nProgressY = nGenActionY - CHATGPT_PROGRESS_H - 4;
    const int nGeneratedBodyHeight = max(0, nProgressY - nContentTop - 4);
    m_rcGeneratedBody.SetRect(nRightX, nContentTop, nRightX + nRightPaneW, nContentTop + nGeneratedBodyHeight);

    int nGeneratedListH = 0;
    int nGeneratedDetailH = 0;
    CalcGeneratedPaneHeights(
        nGeneratedBodyHeight,
        m_dGeneratedListSplitRatio,
        nGeneratedListH,
        nGeneratedDetailH);

    const int nSplitAreaH = max(0, nGeneratedBodyHeight - GENERATED_SPLITTER_WIDTH);
    if (nSplitAreaH > 0)
    {
        m_dGeneratedListSplitRatio =
            static_cast<double>(nGeneratedListH) / static_cast<double>(nSplitAreaH);
    }

    const int nGeneratedSplitterY = nContentTop + nGeneratedListH;
    m_rcGeneratedSplitter.SetRect(
        nRightX,
        nGeneratedSplitterY,
        nRightX + nRightPaneW,
        nGeneratedSplitterY + GENERATED_SPLITTER_WIDTH);

    const int nGeneratedRichTop = nGeneratedSplitterY + GENERATED_SPLITTER_WIDTH;
    const int nBankToolbarY = nContentBottom - BANK_BOTTOM_PANEL_H + 4;
    const int nBankStatusY = nContentBottom - BANK_STATUS_H - 4;
    const int nBankListHeight = max(0, nBankToolbarY - nContentTop - 4);

    place(m_listGenerated, nRightX, nContentTop, nRightPaneW, nGeneratedListH);
    place(m_richQuestion, nRightX, nGeneratedRichTop, nRightPaneW, nGeneratedDetailH);
    InvalidateRect(&m_rcGeneratedSplitter, FALSE);
    place(m_richBank, nRightX, nContentTop, nRightPaneW, nBankListHeight);

    const int nBankBtnGap = 8;
    const int nBankDeleteW = GetTextWidth(this, L"삭제", 28);
    const int nBankUpW = GetTextWidth(this, L"위로 이동", 28);
    const int nBankDownW = GetTextWidth(this, L"아래로 이동", 28);
    int nBankBtnX = nRightX;
    place(m_btnBankDelete, nBankBtnX, nBankToolbarY, nBankDeleteW, BANK_TOOLBAR_H);
    nBankBtnX += nBankDeleteW + nBankBtnGap;
    place(m_btnBankMoveUp, nBankBtnX, nBankToolbarY, nBankUpW, BANK_TOOLBAR_H);
    nBankBtnX += nBankUpW + nBankBtnGap;
    place(m_btnBankMoveDown, nBankBtnX, nBankToolbarY, nBankDownW, BANK_TOOLBAR_H);
    place(m_staticBankCount, nRightX, nBankStatusY, nRightPaneW, BANK_STATUS_H);

    const int nBtnGap = 8;
    const int nBtnUseW = GetTextWidth(this, L"문제 채택", 28);
    const int nBtnRegenW = GetTextWidth(this, L"재출제", 28);
    const int nBtnAddW = GetTextWidth(this, L"추가 생성", 28);
    const int nBtnSaveW = GetTextWidth(this, L"최종 저장", 28);
    const int nBtnTestW = GetTextWidth(this, L"기능 시험", 28);
    const int nBtnChatGptW = GetTextWidth(this, L"ChatGPT 연동", 28);
    place(m_staticChatGptProgress, nRightX, nProgressY, nRightPaneW, CHATGPT_PROGRESS_H);
    int nBtnX = nRightX;
    place(m_btnUse, nBtnX, nGenActionY, nBtnUseW, GENERATED_ACTION_H);
    nBtnX += nBtnUseW + nBtnGap;
    place(m_btnRegenerate, nBtnX, nGenActionY, nBtnRegenW, GENERATED_ACTION_H);
    nBtnX += nBtnRegenW + nBtnGap;
    place(m_btnAddMore, nBtnX, nGenActionY, nBtnAddW, GENERATED_ACTION_H);
    nBtnX += nBtnAddW + nBtnGap;
    place(m_btnSave, nBtnX, nGenActionY, nBtnSaveW, GENERATED_ACTION_H);
    nBtnX += nBtnSaveW + nBtnGap;
    place(m_btnTest, nBtnX, nGenActionY, nBtnTestW, GENERATED_ACTION_H);
    nBtnX += nBtnTestW + nBtnGap;
    place(m_btnChatGpt, nBtnX, nGenActionY, nBtnChatGptW, GENERATED_ACTION_H);

    ShowActiveTab();
}

void CQuizGenViewCtrl::ShowActiveTab()
{
    if (!::IsWindow(m_tabQuestion.GetSafeHwnd()))
        return;

    m_nActiveTab = m_tabQuestion.GetCurSel();
    if (m_nActiveTab < TAB_GENERATED)
        m_nActiveTab = TAB_GENERATED;

    const BOOL bGenerated = (m_nActiveTab == TAB_GENERATED);

    if (::IsWindow(m_listGenerated.GetSafeHwnd()))
        m_listGenerated.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_richQuestion.GetSafeHwnd()))
        m_richQuestion.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_richBank.GetSafeHwnd()))
        m_richBank.ShowWindow(bGenerated ? SW_HIDE : SW_SHOW);
    if (::IsWindow(m_btnBankDelete.GetSafeHwnd()))
        m_btnBankDelete.ShowWindow(bGenerated ? SW_HIDE : SW_SHOW);
    if (::IsWindow(m_btnBankMoveUp.GetSafeHwnd()))
        m_btnBankMoveUp.ShowWindow(bGenerated ? SW_HIDE : SW_SHOW);
    if (::IsWindow(m_btnBankMoveDown.GetSafeHwnd()))
        m_btnBankMoveDown.ShowWindow(bGenerated ? SW_HIDE : SW_SHOW);
    if (::IsWindow(m_staticBankCount.GetSafeHwnd()))
        m_staticBankCount.ShowWindow(bGenerated ? SW_HIDE : SW_SHOW);
    if (::IsWindow(m_btnUse.GetSafeHwnd()))
        m_btnUse.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_btnRegenerate.GetSafeHwnd()))
        m_btnRegenerate.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_btnAddMore.GetSafeHwnd()))
        m_btnAddMore.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_btnSave.GetSafeHwnd()))
        m_btnSave.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_btnTest.GetSafeHwnd()))
        m_btnTest.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_btnChatGpt.GetSafeHwnd()))
        m_btnChatGpt.ShowWindow(bGenerated ? SW_SHOW : SW_HIDE);
    if (::IsWindow(m_staticChatGptProgress.GetSafeHwnd()))
    {
        m_staticChatGptProgress.ShowWindow(
            (bGenerated && (m_bChatGptTestRunning || m_bQuestionGenerateRunning))
                ? SW_SHOW
                : SW_HIDE);
    }
}

void CQuizGenViewCtrl::UpdateBankStatusLabel()
{
    CStringW strBank;
    strBank.Format(L"현재 저장된 문제 : %d 문제", static_cast<int>(m_SelectedQuestionList.size()));
    if (::IsWindow(m_staticBankCount.GetSafeHwnd()))
        m_staticBankCount.SetWindowText(strBank);
}

void CQuizGenViewCtrl::OnTcnSelchangeTab(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    ShowActiveTab();
    Invalidate(FALSE);
    if (pResult != nullptr)
        *pResult = 0;
}

void CQuizGenViewCtrl::LogQuestionSelect(
    const CStringW& strListType,
    int nDisplayIndex,
    int nQuestionIndex,
    const QUESTION_ITEM& item,
    int nPdfPageBefore,
    BOOL bMoveResult,
    int nPdfPageAfter) const
{
    CStringW strEntry;
    strEntry.Format(
        L"[QUESTION_SELECT]\r\n"
        L"ListType=%s\r\n"
        L"DisplayIndex=%d\r\n"
        L"QuestionIndex=%d\r\n"
        L"QuestionText=%s\r\n"
        L"SourcePage=%d\r\n"
        L"CurrentPdfPageBefore=%d\r\n"
        L"MovePdfToPageResult=%s\r\n"
        L"CurrentPdfPageAfter=%d\r\n",
        strListType.GetString(),
        nDisplayIndex,
        nQuestionIndex,
        item.strQuestion.GetString(),
        item.nSourcePage,
        nPdfPageBefore,
        bMoveResult ? L"TRUE" : L"FALSE",
        nPdfPageAfter);

    TRACE(L"%s\n", strEntry.GetString());

    const CStringW strPath = ScpPaths::GetLogFolder() + L"QuestionSelect.txt";
    try
    {
        std::ofstream ofs(strPath, std::ios::binary | std::ios::app);
        if (!ofs.is_open())
            return;

        const CStringW strFull =
            L"[" + OpenAiClient::GetCurrentTimestamp() + L"]\r\n" + strEntry + L"\r\n";
        const std::string strUtf8 = TrainingUtil::CStringWToUtf8(strFull);
        ofs.write(strUtf8.data(), static_cast<std::streamsize>(strUtf8.size()));
    }
    catch (...)
    {
    }
}

BOOL CQuizGenViewCtrl::NavigatePdfToSourcePage(int nSourcePage)
{
    if (nSourcePage < 1 || !::IsWindow(m_pdfPreview.GetSafeHwnd()))
        return FALSE;

    if (!m_pdfPreview.IsDocumentOpen())
    {
        ReadSettingsFromControls();
        const CStringW strPdf = GetSelectedPdfPath();
        if (!strPdf.IsEmpty())
            m_pdfPreview.OpenDocument(strPdf);
    }

    if (!m_pdfPreview.IsDocumentOpen())
        return FALSE;

    return m_pdfPreview.GoToPage(nSourcePage);
}

int CQuizGenViewCtrl::ResolveGeneratedQuestionIndex(int nListIndex) const
{
    if (nListIndex < 0 || !::IsWindow(m_listGenerated.GetSafeHwnd()))
        return -1;

    const DWORD_PTR dwItemData = m_listGenerated.GetItemData(nListIndex);
    if (dwItemData == static_cast<DWORD_PTR>(LB_ERR))
        return nListIndex;

    const int nQuestionIndex =
        CGeneratedQuestionListBox::GetQuestionIndexFromItemData(dwItemData);
    if (nQuestionIndex >= 0 &&
        nQuestionIndex < static_cast<int>(m_TestQuestionList.size()))
    {
        return nQuestionIndex;
    }

    return nListIndex;
}

void CQuizGenViewCtrl::RefreshGeneratedList()
{
    if (!::IsWindow(m_listGenerated.GetSafeHwnd()))
        return;

    m_listGenerated.ResetContent();

    QUESTION_LIST_LABEL_OPTIONS labelOptions;
    labelOptions.bShowIndex = TRUE;
    labelOptions.bShowSourcePage = TRUE;
    labelOptions.nMaxTextLength = 96;

    for (int i = 0; i < static_cast<int>(m_TestQuestionList.size()); ++i)
    {
        const QUESTION_ITEM& item = m_TestQuestionList[i];
        QUESTION_LIST_LABEL_OPTIONS itemOptions = labelOptions;

        if (IsQuestionInBank(item, m_SelectedQuestionList))
            itemOptions.strAdoptedMark = L"✓ ";

        const int nIndex = m_listGenerated.AddString(
            FormatQuestionListLabel(item, i + 1, &itemOptions));
        if (nIndex != LB_ERR)
        {
            m_listGenerated.SetItemData(
                nIndex,
                CGeneratedQuestionListBox::MakeItemData(
                    i,
                    IsQuestionInBank(item, m_SelectedQuestionList)));
        }
    }

    if (m_TestQuestionList.empty())
        return;

    if (m_nCurrentTestQuestion < 0 ||
        m_nCurrentTestQuestion >= static_cast<int>(m_TestQuestionList.size()))
    {
        m_nCurrentTestQuestion = 0;
    }

    m_listGenerated.SetCurSel(m_nCurrentTestQuestion);
    m_listGenerated.Invalidate(FALSE);
}

void CQuizGenViewCtrl::SelectGeneratedQuestion(int nIndex, BOOL bNavigatePdf)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(m_TestQuestionList.size()))
        return;

    m_nCurrentTestQuestion = nIndex;
    const QUESTION_ITEM& item = m_TestQuestionList[nIndex];

    if (::IsWindow(m_listGenerated.GetSafeHwnd()) &&
        m_listGenerated.IsWindowVisible() &&
        m_listGenerated.GetCurSel() != nIndex)
    {
        m_listGenerated.SetCurSel(nIndex);
    }

    m_richQuestion.SetWindowText(FormatQuestionDisplayText(item));

    if (bNavigatePdf)
    {
        const int nPdfBefore = m_pdfPreview.GetCurrentPageOneBased();
        const BOOL bMoved = NavigatePdfToSourcePage(item.nSourcePage);
        const int nPdfAfter = m_pdfPreview.GetCurrentPageOneBased();
        LogQuestionSelect(
            L"Generated",
            nIndex + 1,
            nIndex,
            item,
            nPdfBefore,
            bMoved,
            nPdfAfter);
    }
}

void CQuizGenViewCtrl::OnLbnSelchangeGeneratedList()
{
    const int nListIndex = m_listGenerated.GetCurSel();
    if (nListIndex < 0)
        return;

    const int nQuestionIndex = ResolveGeneratedQuestionIndex(nListIndex);
    SelectGeneratedQuestion(nQuestionIndex, TRUE);
}

int CQuizGenViewCtrl::FindBankIndexByCharPos(long nCharPos) const
{
    if (m_arrBankBlockStarts.empty() || nCharPos < 0)
        return -1;

    for (int i = static_cast<int>(m_arrBankBlockStarts.size()) - 1; i >= 0; --i)
    {
        if (nCharPos >= m_arrBankBlockStarts[static_cast<size_t>(i)])
            return i;
    }

    return -1;
}

void CQuizGenViewCtrl::SelectBankQuestion(int nIndex, BOOL bNavigatePdf)
{
    if (nIndex < 0 || nIndex >= static_cast<int>(m_SelectedQuestionList.size()))
        return;

    m_nSelectedBankIndex = nIndex;

    if (::IsWindow(m_richBank.GetSafeHwnd()) && !m_arrBankBlockStarts.empty())
    {
        const long nStart = m_arrBankBlockStarts[static_cast<size_t>(nIndex)];
        long nEnd = 0;
        if (nIndex + 1 < static_cast<int>(m_arrBankBlockStarts.size()))
            nEnd = m_arrBankBlockStarts[static_cast<size_t>(nIndex + 1)];
        else
            nEnd = m_richBank.GetWindowTextLength();

        m_richBank.SetSel(nStart, nEnd);
    }

    if (bNavigatePdf)
    {
        const QUESTION_ITEM& item = m_SelectedQuestionList[nIndex];
        const int nPdfBefore = m_pdfPreview.GetCurrentPageOneBased();
        const BOOL bMoved = NavigatePdfToSourcePage(item.nSourcePage);
        const int nPdfAfter = m_pdfPreview.GetCurrentPageOneBased();
        LogQuestionSelect(
            L"Temp",
            nIndex + 1,
            nIndex,
            item,
            nPdfBefore,
            bMoved,
            nPdfAfter);
    }
}

void CQuizGenViewCtrl::OnEnSelchangeBankRich(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    if (!::IsWindow(m_richBank.GetSafeHwnd()))
    {
        if (pResult != nullptr)
            *pResult = 0;
        return;
    }

    long nStart = 0;
    long nEnd = 0;
    m_richBank.GetSel(nStart, nEnd);

    const int nIndex = FindBankIndexByCharPos(nStart);
    if (nIndex >= 0)
        SelectBankQuestion(nIndex, TRUE);

    if (pResult != nullptr)
        *pResult = 0;
}

BOOL CQuizGenViewCtrl::HasSelectedBankItem() const
{
    return m_nSelectedBankIndex >= 0 &&
           m_nSelectedBankIndex < static_cast<int>(m_SelectedQuestionList.size());
}

int CQuizGenViewCtrl::GetSelectedBankIndex() const
{
    if (!HasSelectedBankItem())
        return -1;
    return m_nSelectedBankIndex;
}

void CQuizGenViewCtrl::RefreshBankList()
{
    if (!::IsWindow(m_richBank.GetSafeHwnd()))
        return;

    m_arrBankBlockStarts.clear();

    QUESTION_BANK_ENTRY_OPTIONS bankOptions;
    bankOptions.bShowChoices = TRUE;
    bankOptions.bShowSourcePage = TRUE;
    bankOptions.bShowAnswer = FALSE;
    bankOptions.bShowExplain = FALSE;

    CStringW strDocument;
    const int nCount = static_cast<int>(m_SelectedQuestionList.size());
    for (int i = 0; i < nCount; ++i)
    {
        m_arrBankBlockStarts.push_back(static_cast<long>(strDocument.GetLength()));
        strDocument += FormatQuestionBankEntry(m_SelectedQuestionList[i], i + 1, &bankOptions);
        if (i + 1 < nCount)
            strDocument += L"\r\n\r\n";
    }

    m_richBank.SetWindowText(strDocument);
    UpdateBankStatusLabel();

    if (m_SelectedQuestionList.empty())
    {
        m_nSelectedBankIndex = -1;
        return;
    }

    if (m_nSelectedBankIndex < 0 ||
        m_nSelectedBankIndex >= static_cast<int>(m_SelectedQuestionList.size()))
    {
        m_nSelectedBankIndex = 0;
    }

    SelectBankQuestion(m_nSelectedBankIndex, TRUE);
}

BOOL CQuizGenViewCtrl::LoadTestQuestionFile()
{
    CStringW strError;
    const CStringW strPath = QuestionTestLoader::GetTestQuestionFilePath();
    CQuestionItemArray loaded;

    if (!QuestionTestLoader::LoadFromFile(strPath, loaded, strError))
    {
        if (strError.IsEmpty())
            strError = L"QuestionListForTest.txt 파일을 찾을 수 없습니다.";
        AfxMessageBox(strError, MB_OK | MB_ICONWARNING);
        return FALSE;
    }

    m_TestQuestionList = std::move(loaded);
    m_nCurrentTestQuestion = 0;
    m_bTestQuestionsLoaded = TRUE;
    return TRUE;
}

void CQuizGenViewCtrl::DisplayCurrentTestQuestion(BOOL bNavigatePdf)
{
    if (!m_bTestQuestionsLoaded ||
        m_nCurrentTestQuestion < 0 ||
        m_nCurrentTestQuestion >= static_cast<int>(m_TestQuestionList.size()))
    {
        return;
    }

    const QUESTION_ITEM& item = m_TestQuestionList[m_nCurrentTestQuestion];

    if (::IsWindow(m_listGenerated.GetSafeHwnd()) &&
        m_listGenerated.IsWindowVisible() &&
        m_listGenerated.GetCurSel() != m_nCurrentTestQuestion)
    {
        m_listGenerated.SetCurSel(m_nCurrentTestQuestion);
    }

    m_richQuestion.SetWindowText(FormatQuestionDisplayText(item));

    if (bNavigatePdf)
    {
        const int nPdfBefore = m_pdfPreview.GetCurrentPageOneBased();
        const BOOL bMoved = NavigatePdfToSourcePage(item.nSourcePage);
        const int nPdfAfter = m_pdfPreview.GetCurrentPageOneBased();
        LogQuestionSelect(
            L"Generated",
            m_nCurrentTestQuestion + 1,
            m_nCurrentTestQuestion,
            item,
            nPdfBefore,
            bMoved,
            nPdfAfter);
    }
}

void CQuizGenViewCtrl::AdvanceToNextTestQuestion()
{
    if (!m_bTestQuestionsLoaded || m_TestQuestionList.empty())
    {
        AfxMessageBox(
            L"기능 시험 문제가 로드되지 않았습니다.\r\n\r\n[기능 시험] 버튼을 먼저 실행하세요.",
            MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_nCurrentTestQuestion =
        (m_nCurrentTestQuestion + 1) % static_cast<int>(m_TestQuestionList.size());
    DisplayCurrentTestQuestion();
}

BOOL CQuizGenViewCtrl::AdoptCurrentTestQuestion(CStringW& strMessage)
{
    strMessage.Empty();

    if (!m_bTestQuestionsLoaded ||
        m_nCurrentTestQuestion < 0 ||
        m_nCurrentTestQuestion >= static_cast<int>(m_TestQuestionList.size()))
    {
        strMessage =
            L"기능 시험 문제가 로드되지 않았습니다.\r\n\r\n[기능 시험] 버튼을 먼저 실행하세요.";
        return FALSE;
    }

    const QUESTION_ITEM& item = m_TestQuestionList[m_nCurrentTestQuestion];
    for (const QUESTION_ITEM& selected : m_SelectedQuestionList)
    {
        if (selected.strId.CompareNoCase(item.strId) == 0)
        {
            strMessage = L"이미 채택된 문제입니다.\r\n\r\n(ID: " + item.strId + L")";
            return FALSE;
        }
    }

    QUESTION_ITEM adoptedItem = item;
    adoptedItem.bUseFlag = TRUE;
    m_SelectedQuestionList.push_back(adoptedItem);
    RefreshBankList();
    RefreshGeneratedList();
    strMessage.Format(
        L"문제가 임시 문제집에 추가되었습니다.\r\n\r\n현재 저장된 문제 : %d 문제",
        static_cast<int>(m_SelectedQuestionList.size()));
    return TRUE;
}

BOOL CQuizGenViewCtrl::HitTestSplitter(const CPoint& pt) const
{
    if (m_rcSplitter.IsRectEmpty())
        return FALSE;

    CRect rcHit = m_rcSplitter;
    rcHit.InflateRect(4, 0);
    return rcHit.PtInRect(pt);
}

BOOL CQuizGenViewCtrl::HitTestGeneratedSplitter(const CPoint& pt) const
{
    if (!::IsWindow(m_tabQuestion.GetSafeHwnd()) ||
        m_tabQuestion.GetCurSel() != TAB_GENERATED ||
        m_rcGeneratedSplitter.IsRectEmpty())
    {
        return FALSE;
    }

    CRect rcHit = m_rcGeneratedSplitter;
    rcHit.InflateRect(0, 4);
    return rcHit.PtInRect(pt);
}

void CQuizGenViewCtrl::OnPaint()
{
    CPaintDC dc(this);

    if (!m_rcSplitter.IsRectEmpty())
    {
        CRect rcDraw = m_rcSplitter;
        dc.DrawEdge(rcDraw, EDGE_ETCHED, BF_RECT);
    }

    if (m_nActiveTab == TAB_GENERATED && !m_rcGeneratedSplitter.IsRectEmpty())
    {
        CRect rcDraw = m_rcGeneratedSplitter;
        dc.DrawEdge(rcDraw, EDGE_ETCHED, BF_RECT);
    }
}

void CQuizGenViewCtrl::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (HitTestGeneratedSplitter(point))
    {
        m_bDraggingGeneratedSplit = TRUE;
        SetCapture();
        return;
    }

    if (HitTestSplitter(point))
    {
        m_bDraggingSplit = TRUE;
        SetCapture();
        return;
    }

    if (::IsWindow(m_tabQuestion.GetSafeHwnd()) &&
        m_tabQuestion.GetCurSel() == TAB_BANK &&
        ::IsWindow(m_richBank.GetSafeHwnd()) &&
        m_richBank.IsWindowVisible())
    {
        CRect rcBank;
        m_richBank.GetWindowRect(&rcBank);
        ScreenToClient(&rcBank);
        if (rcBank.PtInRect(point))
        {
            CPoint ptBank = point;
            ClientToScreen(&ptBank);
            m_richBank.ScreenToClient(&ptBank);

            const int nCharIndex = static_cast<int>(LOWORD(m_richBank.CharFromPos(ptBank)));
            const int nIndex = FindBankIndexByCharPos(nCharIndex);
            if (nIndex >= 0)
                SelectBankQuestion(nIndex, TRUE);
        }
    }

    CWnd::OnLButtonDown(nFlags, point);
}

void CQuizGenViewCtrl::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDraggingGeneratedSplit)
    {
        m_bDraggingGeneratedSplit = FALSE;
        if (GetCapture() == this)
            ReleaseCapture();
        SaveUiSettings();
        return;
    }

    if (m_bDraggingSplit)
    {
        m_bDraggingSplit = FALSE;
        if (GetCapture() == this)
            ReleaseCapture();
        SaveUiSettings();
        return;
    }

    CWnd::OnLButtonUp(nFlags, point);
}

void CQuizGenViewCtrl::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDraggingGeneratedSplit)
    {
        const int nBodyTop = m_rcGeneratedBody.top;
        const int nBodyHeight = m_rcGeneratedBody.Height();
        const int nSplitAreaH = max(0, nBodyHeight - GENERATED_SPLITTER_WIDTH);

        if (nSplitAreaH > 0)
        {
            int nListH = point.y - nBodyTop;
            nListH = max(
                GENERATED_PANE_MIN_H,
                min(nListH, nSplitAreaH - GENERATED_PANE_MIN_H));
            m_dGeneratedListSplitRatio =
                static_cast<double>(nListH) / static_cast<double>(nSplitAreaH);
        }

        UpdateLayout();
        return;
    }

    if (m_bDraggingSplit)
    {
        CRect rcClient;
        GetClientRect(&rcClient);

        const int nLeft = PANEL_GAP;
        const int nWidth = max(0, rcClient.Width() - PANEL_GAP * 2);
        const int nSplitAreaW = max(0, nWidth - SPLITTER_WIDTH);
        const int nMinLeft = PANE_MIN_WIDTH;
        const int nMinRight = PANE_MIN_WIDTH;

        int nLeftPaneW = point.x - nLeft;
        nLeftPaneW = max(nMinLeft, min(nLeftPaneW, nSplitAreaW - nMinRight));
        m_dSplitRatio = (nSplitAreaW > 0)
            ? static_cast<double>(nLeftPaneW) / static_cast<double>(nSplitAreaW)
            : ScpUiSettings::kPdfSplitDefault;

        UpdateLayout();
        return;
    }

    CWnd::OnMouseMove(nFlags, point);
}

BOOL CQuizGenViewCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest == HTCLIENT)
    {
        CPoint pt;
        GetCursorPos(&pt);
        ScreenToClient(&pt);

        if (HitTestGeneratedSplitter(pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_SIZENS));
            return TRUE;
        }

        if (HitTestSplitter(pt))
        {
            ::SetCursor(::LoadCursor(nullptr, IDC_SIZEWE));
            return TRUE;
        }
    }

    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CQuizGenViewCtrl::SetPdfFileList(const CStringArray& arrPdfFiles)
{
    m_arrPdfFiles.RemoveAll();
    for (int i = 0; i < arrPdfFiles.GetSize(); ++i)
        m_arrPdfFiles.Add(arrPdfFiles[i]);

    UpdatePdfCombo();
}

void CQuizGenViewCtrl::UpdatePdfCombo()
{
    const int nPrev = m_nSelectedPdfIndex;
    m_comboPdf.ResetContent();

    for (int i = 0; i < m_arrPdfFiles.GetSize(); ++i)
    {
        const CStringW& strFullPath = m_arrPdfFiles[i];
        int nSlash = strFullPath.ReverseFind(L'\\');
        CStringW strName = (nSlash >= 0) ? strFullPath.Mid(nSlash + 1) : strFullPath;
        m_comboPdf.AddString(strName);
    }

    if (m_arrPdfFiles.GetSize() > 0)
    {
        int nSelect = (nPrev >= 0 && nPrev < m_arrPdfFiles.GetSize()) ? nPrev : 0;
        m_comboPdf.SetCurSel(nSelect);
        SelectPdfByIndex(nSelect);
    }
    else
    {
        m_nSelectedPdfIndex = -1;
        if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
            m_pdfPreview.CloseDocument();
    }
}

void CQuizGenViewCtrl::SelectPdfByIndex(int nPdfIndex)
{
    if (nPdfIndex < 0 || nPdfIndex >= m_arrPdfFiles.GetSize())
        return;

    m_nSelectedPdfIndex = nPdfIndex;
    m_settings.strPdfPath = m_arrPdfFiles[nPdfIndex];

    if (m_comboPdf.GetCurSel() != nPdfIndex)
        m_comboPdf.SetCurSel(nPdfIndex);

    if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
    {
        if (!m_pdfPreview.OpenDocument(m_settings.strPdfPath))
        {
            AfxMessageBox(
                L"PDF 미리보기를 열 수 없습니다.\n" + m_settings.strPdfPath,
                MB_OK | MB_ICONWARNING);
        }
    }
}

CStringW CQuizGenViewCtrl::GetSelectedPdfPath() const
{
    if (m_nSelectedPdfIndex >= 0 && m_nSelectedPdfIndex < m_arrPdfFiles.GetSize())
        return m_arrPdfFiles[m_nSelectedPdfIndex];
    return CStringW();
}

void CQuizGenViewCtrl::UpdatePageRangeEnable()
{
    const BOOL bRange = (m_radioPageRange.GetCheck() == BST_CHECKED);
    m_editPageStart.EnableWindow(bRange);
    m_editPageEnd.EnableWindow(bRange);
    m_staticPageStartLabel.EnableWindow(bRange);
    m_staticPageEndLabel.EnableWindow(bRange);
}

void CQuizGenViewCtrl::ReadSettingsFromControls()
{
    m_settings.strPdfPath = GetSelectedPdfPath();
    m_settings.bAllPages = (m_radioAllPages.GetCheck() == BST_CHECKED);

    CStringW strStart, strEnd, strCount;
    m_editPageStart.GetWindowText(strStart);
    m_editPageEnd.GetWindowText(strEnd);
    m_editQuestionCount.GetWindowText(strCount);

    m_settings.nPageStart = _wtoi(strStart);
    m_settings.nPageEnd = _wtoi(strEnd);
    m_settings.nQuestionCount = _wtoi(strCount);

    if (m_settings.nPageStart < 1)
        m_settings.nPageStart = 1;
    if (m_settings.nPageEnd < m_settings.nPageStart)
        m_settings.nPageEnd = m_settings.nPageStart;
    if (m_settings.nQuestionCount < 1)
        m_settings.nQuestionCount = 1;
}

CStringW CQuizGenViewCtrl::GetDummyQuestionText() const
{
    return
        L"문제 1.\r\n\r\n"
        L"다음 중 유압펌프의 역할은?\r\n\r\n"
        L"①\r\n"
        L"②\r\n"
        L"③\r\n"
        L"④\r\n\r\n"
        L"정답 : ②";
}

void CQuizGenViewCtrl::LoadDummyQuestionText()
{
    m_richQuestion.SetWindowText(GetDummyQuestionText());
}

void CQuizGenViewCtrl::OnBnClickedSelectPdf()
{
    AfxMessageBox(
        L"[UI 단계] PDF 선택\r\n\r\n"
        L"좌측 PDF 목록 또는 상단 콤보박스에서 PDF를 선택하세요.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedRadioAllPages()
{
    m_radioAllPages.SetCheck(BST_CHECKED);
    m_radioPageRange.SetCheck(BST_UNCHECKED);
    UpdatePageRangeEnable();
}

void CQuizGenViewCtrl::OnBnClickedRadioPageRange()
{
    m_radioAllPages.SetCheck(BST_UNCHECKED);
    m_radioPageRange.SetCheck(BST_CHECKED);
    UpdatePageRangeEnable();
}

void CQuizGenViewCtrl::OnCbnSelchangePdfCombo()
{
    const int nSel = m_comboPdf.GetCurSel();
    if (nSel >= 0)
        SelectPdfByIndex(nSel);
}

void CQuizGenViewCtrl::OnBnClickedGenerate()
{
    if (m_bQuestionGenerateRunning || m_bChatGptTestRunning)
        return;

    ReadSettingsFromControls();

    if (m_settings.strPdfPath.IsEmpty())
    {
        AfxMessageBox(L"PDF 파일을 선택하세요.", MB_OK | MB_ICONWARNING);
        return;
    }

    if (m_settings.nQuestionCount < 1)
    {
        AfxMessageBox(L"문제 개수는 1 이상이어야 합니다.", MB_OK | MB_ICONWARNING);
        return;
    }

    SCP_OPENAI_CONFIG config;
    CStringW strConfigError;
    if (!ScpConfigReader::LoadOpenAiConfig(config, strConfigError))
    {
        AfxMessageBox(strConfigError, MB_OK | MB_ICONWARNING);
        return;
    }

    OPENAI_GENERATE_REQUEST request;
    request.config = config;
    request.strPdfPath = m_settings.strPdfPath;
    request.bAllPages = m_settings.bAllPages;
    request.nPageStart = m_settings.nPageStart;
    request.nPageEnd = m_settings.nPageEnd;
    request.nQuestionCount = m_settings.nQuestionCount;

    StartQuestionGeneration(request);
}

void CQuizGenViewCtrl::OnBnClickedUse()
{
    CStringW strMessage;
    if (!AdoptCurrentTestQuestion(strMessage))
    {
        AfxMessageBox(strMessage, MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_tabQuestion.SetCurSel(TAB_BANK);
    ShowActiveTab();
    AfxMessageBox(strMessage, MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedRegenerate()
{
    AdvanceToNextTestQuestion();
}

void CQuizGenViewCtrl::OnBnClickedAddMore()
{
    AdvanceToNextTestQuestion();
}

void CQuizGenViewCtrl::OnBnClickedSave()
{
    AfxMessageBox(
        L"[기능 시험] 최종 저장\r\n\r\n"
        L"TXT 저장 기능은 다음 단계에서 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::SetGeneratedActionButtonsEnabled(BOOL bEnabled)
{
    CWnd* arrButtons[] = {
        &m_btnUse, &m_btnRegenerate, &m_btnAddMore, &m_btnSave, &m_btnTest, &m_btnChatGpt
    };

    for (CWnd* pWnd : arrButtons)
    {
        if (pWnd != nullptr && ::IsWindow(pWnd->GetSafeHwnd()))
            pWnd->EnableWindow(bEnabled);
    }
}

void CQuizGenViewCtrl::SetChatGptTestBusy(BOOL bBusy)
{
    m_bChatGptTestRunning = bBusy;
    const BOOL bAnyBusy = m_bChatGptTestRunning || m_bQuestionGenerateRunning;
    SetGeneratedActionButtonsEnabled(bAnyBusy ? FALSE : TRUE);

    if (::IsWindow(m_btnGenerate.GetSafeHwnd()))
        m_btnGenerate.EnableWindow(bAnyBusy ? FALSE : TRUE);

    if (::IsWindow(m_staticChatGptProgress.GetSafeHwnd()))
    {
        if (bBusy)
        {
            m_staticChatGptProgress.SetWindowText(L"ChatGPT 연동 시험 중입니다...");
            m_staticChatGptProgress.ShowWindow(SW_SHOW);
        }
        else if (!m_bQuestionGenerateRunning)
        {
            m_staticChatGptProgress.SetWindowText(L"");
            m_staticChatGptProgress.ShowWindow(SW_HIDE);
        }
    }
}

void CQuizGenViewCtrl::SetQuestionGenerateBusy(BOOL bBusy)
{
    m_bQuestionGenerateRunning = bBusy;
    const BOOL bAnyBusy = m_bChatGptTestRunning || m_bQuestionGenerateRunning;
    SetGeneratedActionButtonsEnabled(bAnyBusy ? FALSE : TRUE);

    if (::IsWindow(m_btnGenerate.GetSafeHwnd()))
        m_btnGenerate.EnableWindow(bAnyBusy ? FALSE : TRUE);

    if (::IsWindow(m_staticChatGptProgress.GetSafeHwnd()))
    {
        if (bBusy)
        {
            m_staticChatGptProgress.SetWindowText(L"ChatGPT로 문제를 생성하는 중입니다...");
            m_staticChatGptProgress.ShowWindow(SW_SHOW);
        }
        else if (!m_bChatGptTestRunning)
        {
            m_staticChatGptProgress.SetWindowText(L"");
            m_staticChatGptProgress.ShowWindow(SW_HIDE);
        }
    }
}

void CQuizGenViewCtrl::StartQuestionGeneration(const OPENAI_GENERATE_REQUEST& request)
{
    if (!::IsWindow(GetSafeHwnd()))
        return;

    SetQuestionGenerateBusy(TRUE);

    const HWND hwnd = m_hWnd;
    std::thread([hwnd, request]()
    {
        OPENAI_GENERATE_RESULT* pResult = new OPENAI_GENERATE_RESULT();
        *pResult = OpenAiQuestionGenerator::Run(request);

        if (::IsWindow(hwnd))
            ::PostMessage(hwnd, WM_QUIZGEN_GENERATE_DONE, 0, reinterpret_cast<LPARAM>(pResult));
        else
            delete pResult;
    }).detach();
}

void CQuizGenViewCtrl::StartChatGptConnectionTest(const SCP_OPENAI_CONFIG& config)
{
    if (!::IsWindow(GetSafeHwnd()))
        return;

    SetChatGptTestBusy(TRUE);

    const HWND hwnd = m_hWnd;
    std::thread([hwnd, config]()
    {
        OPENAI_TEST_RESULT* pResult = new OPENAI_TEST_RESULT();
        *pResult = OpenAiConnectionTest::RunConnectionTest(config);

        if (::IsWindow(hwnd))
            ::PostMessage(hwnd, WM_QUIZGEN_CHATGPT_DONE, 0, reinterpret_cast<LPARAM>(pResult));
        else
            delete pResult;
    }).detach();
}

void CQuizGenViewCtrl::OnBnClickedChatGpt()
{
    if (m_bChatGptTestRunning)
        return;

    SCP_OPENAI_CONFIG config;
    CStringW strError;
    if (!ScpConfigReader::LoadOpenAiConfig(config, strError))
    {
        AfxMessageBox(strError, MB_OK | MB_ICONWARNING);
        return;
    }

    StartChatGptConnectionTest(config);
}

LRESULT CQuizGenViewCtrl::OnQuestionGenerateDone(WPARAM /*wParam*/, LPARAM lParam)
{
    std::unique_ptr<OPENAI_GENERATE_RESULT> pResult(reinterpret_cast<OPENAI_GENERATE_RESULT*>(lParam));
    SetQuestionGenerateBusy(FALSE);

    if (pResult == nullptr)
        return 0;

    if (pResult->bSuccess)
    {
        m_TestQuestionList = std::move(pResult->questions);
        m_nCurrentTestQuestion = 0;
        m_bTestQuestionsLoaded = TRUE;
        RefreshGeneratedList();
        DisplayCurrentTestQuestion();
        m_tabQuestion.SetCurSel(TAB_GENERATED);
        ShowActiveTab();

        CStringW strMsg;
        strMsg.Format(
            L"문제 생성 완료\r\n\r\n"
            L"요청: %d개\r\n"
            L"생성: %d개\r\n\r\n"
            L"첫 번째 문제를 표시합니다.",
            pResult->nRequestedCount,
            static_cast<int>(m_TestQuestionList.size()));
        AfxMessageBox(strMsg, MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CStringW strMsg;
        strMsg.Format(
            L"문제 생성 실패\r\n\r\n"
            L"단계: %s\r\n"
            L"HTTP 상태 코드: %d\r\n"
            L"오류: %s",
            OpenAiQuestionGenerator::StageToUserMessage(pResult->stage).GetString(),
            pResult->nHttpStatus,
            pResult->strErrorMessage.IsEmpty()
                ? L"(없음)"
                : pResult->strErrorMessage.GetString());

        if (!pResult->strPossibleCause.IsEmpty())
        {
            strMsg += L"\r\n\r\n원인:\r\n";
            strMsg += pResult->strPossibleCause;
        }

        strMsg += L"\r\n\r\n자세한 내용은 Log\\ErrorLog.txt 를 확인하세요.";
        AfxMessageBox(strMsg, MB_OK | MB_ICONWARNING);
    }

    return 0;
}

LRESULT CQuizGenViewCtrl::OnChatGptTestDone(WPARAM /*wParam*/, LPARAM lParam)
{
    std::unique_ptr<OPENAI_TEST_RESULT> pResult(reinterpret_cast<OPENAI_TEST_RESULT*>(lParam));
    SetChatGptTestBusy(FALSE);

    if (pResult == nullptr)
        return 0;

    if (pResult->bSuccess)
    {
        CStringW strMsg;
        strMsg.Format(
            L"ChatGPT 연동 성공\r\n\r\n응답:\r\n%s",
            pResult->strAssistantMessage.GetString());
        AfxMessageBox(strMsg, MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CStringW strMsg;
        strMsg.Format(
            L"ChatGPT 연동 실패\r\n\r\nHTTP 상태 코드: %d\r\n오류: %s\r\n\r\n원인:\r\n%s",
            pResult->nHttpStatus,
            pResult->strErrorMessage.IsEmpty()
                ? L"(없음)"
                : pResult->strErrorMessage.GetString(),
            pResult->strPossibleCause.IsEmpty()
                ? L"원인을 확인할 수 없습니다."
                : pResult->strPossibleCause.GetString());
        AfxMessageBox(strMsg, MB_OK | MB_ICONWARNING);
    }

    return 0;
}

void CQuizGenViewCtrl::OnBnClickedTest()
{
    if (!LoadTestQuestionFile())
        return;

    RefreshGeneratedList();
    DisplayCurrentTestQuestion();
    m_tabQuestion.SetCurSel(TAB_GENERATED);
    ShowActiveTab();

    CStringW strMsg;
    strMsg.Format(
        L"기능 시험 문제 %d개를 로드했습니다.\r\n\r\n"
        L"첫 번째 문제를 표시합니다.",
        static_cast<int>(m_TestQuestionList.size()));
    AfxMessageBox(strMsg, MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedBankDelete()
{
    const int nIndex = GetSelectedBankIndex();
    if (nIndex < 0)
    {
        AfxMessageBox(L"삭제할 문제를 선택하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    m_SelectedQuestionList.erase(m_SelectedQuestionList.begin() + nIndex);

    if (m_SelectedQuestionList.empty())
        m_nSelectedBankIndex = -1;
    else if (nIndex >= static_cast<int>(m_SelectedQuestionList.size()))
        m_nSelectedBankIndex = static_cast<int>(m_SelectedQuestionList.size()) - 1;
    else
        m_nSelectedBankIndex = nIndex;

    RefreshBankList();
}

void CQuizGenViewCtrl::OnBnClickedBankMoveUp()
{
    const int nIndex = GetSelectedBankIndex();
    if (nIndex < 0)
    {
        AfxMessageBox(L"이동할 문제를 선택하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (nIndex <= 0)
        return;

    std::swap(m_SelectedQuestionList[nIndex], m_SelectedQuestionList[nIndex - 1]);
    m_nSelectedBankIndex = nIndex - 1;
    RefreshBankList();
}

void CQuizGenViewCtrl::OnBnClickedBankMoveDown()
{
    const int nIndex = GetSelectedBankIndex();
    if (nIndex < 0)
    {
        AfxMessageBox(L"이동할 문제를 선택하세요.", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (nIndex >= static_cast<int>(m_SelectedQuestionList.size()) - 1)
        return;

    std::swap(m_SelectedQuestionList[nIndex], m_SelectedQuestionList[nIndex + 1]);
    m_nSelectedBankIndex = nIndex + 1;
    RefreshBankList();
}
