#include "pch.h"
#include "QuizGenViewCtrl.h"
#include "Resource.h"
#include "Util.h"

IMPLEMENT_DYNAMIC(CQuizGenViewCtrl, CWnd)

namespace
{
    constexpr int BOTTOM_PANEL_HEIGHT = 40;
    constexpr int PANEL_GAP = 8;
    constexpr int ROW_GAP = 8;
    constexpr int ROW_H = 26;
    constexpr int SPLIT_GAP = 8;
    constexpr int FORM_LABEL_W = 72;
    constexpr int FORM_INDENT = 16;
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
}

BEGIN_MESSAGE_MAP(CQuizGenViewCtrl, CWnd)
    ON_WM_CREATE()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_SELECT_PDF, &CQuizGenViewCtrl::OnBnClickedSelectPdf)
    ON_BN_CLICKED(IDC_QUIZGEN_RADIO_ALL_PAGES, &CQuizGenViewCtrl::OnBnClickedRadioAllPages)
    ON_BN_CLICKED(IDC_QUIZGEN_RADIO_PAGE_RANGE, &CQuizGenViewCtrl::OnBnClickedRadioPageRange)
    ON_CBN_SELCHANGE(IDC_QUIZGEN_PDF_COMBO, &CQuizGenViewCtrl::OnCbnSelchangePdfCombo)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_GENERATE, &CQuizGenViewCtrl::OnBnClickedGenerate)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_USE, &CQuizGenViewCtrl::OnBnClickedUse)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_REGENERATE, &CQuizGenViewCtrl::OnBnClickedRegenerate)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_ADD_MORE, &CQuizGenViewCtrl::OnBnClickedAddMore)
    ON_BN_CLICKED(IDC_QUIZGEN_BTN_SAVE, &CQuizGenViewCtrl::OnBnClickedSave)
END_MESSAGE_MAP()

CQuizGenViewCtrl::CQuizGenViewCtrl()
    : m_nSelectedPdfIndex(-1)
{
}

CQuizGenViewCtrl::~CQuizGenViewCtrl()
{
    if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
        m_pdfPreview.CloseDocument();
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

    CreateChildControls();
    LoadDummyQuestionText();
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
    m_staticQuestionLabel.Create(L"생성된 문제 보기", dwStaticStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_QUESTION_LABEL);

    CRect rcPreviewHost(0, 0, 100, 100);
    m_pdfPreview.Create(
        nullptr,
        nullptr,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        rcPreviewHost,
        this,
        IDC_QUIZGEN_PDF_PREVIEW);
    m_pdfPreview.SetEmbeddedPreviewMode(TRUE);

    m_richQuestion.Create(
        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        CRect(0, 0, 0, 0),
        this,
        IDC_QUIZGEN_RICHEDIT);

    m_btnUse.Create(L"사용", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_USE);
    m_btnRegenerate.Create(L"재출제", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_REGENERATE);
    m_btnAddMore.Create(L"추가 생성", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_ADD_MORE);
    m_btnSave.Create(L"최종 저장", dwBtnStyle, CRect(0, 0, 0, 0), this, IDC_QUIZGEN_BTN_SAVE);

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
        &m_staticPreviewLabel, &m_staticQuestionLabel,
        &m_richQuestion,
        &m_btnUse, &m_btnRegenerate, &m_btnAddMore, &m_btnSave
    };

    for (const CWnd* pWnd : arrFontControls)
    {
        if (pWnd != nullptr && ::IsWindow(pWnd->GetSafeHwnd()))
            TrainingUtil::ApplyKoreanFont(const_cast<CWnd*>(pWnd));
    }
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
    const int nGenerateW = 100;
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
        place(m_btnGenerate, nLeft + nWidth - nGenerateW, nRowY, nGenerateW, ROW_H);
    };

    const int nOptionBlockW =
        nRadioAllW + nGroupGap +
        nRadioRangeW + nGroupGap +
        nPageStartLabelW + 6 + nPageEditW + nGroupGap +
        nPageEndLabelW + 6 + nPageEditW + nGroupGap +
        nCountLabelW + 6 + nCountEditW;

    const int nRow1TailW = nComboW + nGroupGap + nSelectBtnW;
    const BOOL bSingleRow = (nFieldLeft + nRow1TailW + nGroupGap + nOptionBlockW <= nLeft + nWidth);

    // 1행: PDF + 선택 (+ 넓으면 옵션 전체 포함)
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

    const int nBottomY = rcClient.bottom - PANEL_GAP - BOTTOM_PANEL_HEIGHT;
    const int nCenterTop = y;
    const int nCenterHeight = max(0, nBottomY - nCenterTop - PANEL_GAP);
    const int nHalfWidth = max(120, (nWidth - SPLIT_GAP) / 2);

    place(m_staticPreviewLabel, nLeft, nCenterTop, nHalfWidth, 20);
    place(m_staticQuestionLabel, nLeft + nHalfWidth + SPLIT_GAP, nCenterTop, nHalfWidth, 20);

    const int nPaneTop = nCenterTop + 22;
    const int nPaneHeight = max(0, nCenterHeight - 22);

    if (::IsWindow(m_pdfPreview.GetSafeHwnd()))
    {
        m_pdfPreview.SetWindowPos(
            nullptr,
            nLeft,
            nPaneTop,
            nHalfWidth,
            nPaneHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    place(m_richQuestion, nLeft + nHalfWidth + SPLIT_GAP, nPaneTop, nHalfWidth, nPaneHeight);

    const int nBtnWidth = 96;
    const int nBtnGap = 8;
    int nBtnX = nLeft;
    place(m_btnUse, nBtnX, nBottomY, nBtnWidth, 28);
    nBtnX += nBtnWidth + nBtnGap;
    place(m_btnRegenerate, nBtnX, nBottomY, nBtnWidth, 28);
    nBtnX += nBtnWidth + nBtnGap;
    place(m_btnAddMore, nBtnX, nBottomY, nBtnWidth, 28);
    nBtnX += nBtnWidth + nBtnGap;
    place(m_btnSave, nBtnX, nBottomY, nBtnWidth, 28);
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
    ReadSettingsFromControls();
    AfxMessageBox(
        L"[UI 단계] 문제 생성\r\n\r\n"
        L"2단계 이후 ChatGPT API 연동 및 실제 문제 생성이 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedUse()
{
    AfxMessageBox(
        L"[UI 단계] 사용\r\n\r\n"
        L"선택한 문제를 임시 문제집에 누적하는 기능은 이후 단계에서 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedRegenerate()
{
    AfxMessageBox(
        L"[UI 단계] 재출제\r\n\r\n"
        L"현재 문제를 다시 생성하는 기능은 이후 단계에서 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedAddMore()
{
    AfxMessageBox(
        L"[UI 단계] 추가 생성\r\n\r\n"
        L"추가 문제 생성 반복 기능은 이후 단계에서 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}

void CQuizGenViewCtrl::OnBnClickedSave()
{
    AfxMessageBox(
        L"[UI 단계] 최종 저장\r\n\r\n"
        L"번호 자동 재정렬 후 TXT 저장 기능은 이후 단계에서 구현됩니다.",
        MB_OK | MB_ICONINFORMATION);
}
