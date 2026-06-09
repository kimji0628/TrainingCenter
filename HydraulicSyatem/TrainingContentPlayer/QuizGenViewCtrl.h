#pragma once

#include "QuestionItem.h"
#include "OpenAiConnectionTest.h"
#include "OpenAiQuestionGenerator.h"
#include "ScpConfigReader.h"
#include "GeneratedQuestionListBox.h"
#include "PdfViewerCtrl.h"

#define WM_QUIZGEN_CHATGPT_DONE (WM_USER + 211)
#define WM_QUIZGEN_GENERATE_DONE (WM_USER + 212)
#define WM_QUIZGEN_PDF_INDEX_CHANGED (WM_USER + 213)
#define WM_QUIZGEN_GENERATED_LIST_SEL (WM_USER + 214)

// ============================================================================
// QuizGenViewCtrl.h - AI 문제 생성 / 기능 시험 View
// ============================================================================

class CQuizGenViewCtrl : public CWnd
{
    DECLARE_DYNAMIC(CQuizGenViewCtrl)

public:
    CQuizGenViewCtrl();
    virtual ~CQuizGenViewCtrl();

    BOOL CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId);

    void SetPdfFileList(const CStringArray& arrPdfFiles);
    void SelectPdfByIndex(int nPdfIndex, BOOL bNotifyParent = TRUE);
    int GetSelectedPdfIndex() const { return m_nSelectedPdfIndex; }

    BOOL IsPreviewActive() const;
    BOOL HandlePreviewMouseWheel(short zDelta, const CPoint& ptScreen);
    BOOL HandlePreviewKeyDown(UINT nChar);

    const CQuestionItemArray& GetQuestionItems() const { return m_TestQuestionList; }
    const CQuestionItemArray& GetSelectedQuestionItems() const { return m_SelectedQuestionList; }
    const QUIZ_GEN_SETTINGS& GetSettings() const { return m_settings; }

    void Relayout();

protected:
    enum { TAB_GENERATED = 0, TAB_BANK = 1 };

    QUIZ_GEN_SETTINGS      m_settings;
    CStringArray           m_arrPdfFiles;
    int                    m_nSelectedPdfIndex;

    CQuestionItemArray     m_TestQuestionList;
    int                    m_nCurrentTestQuestion;
    CQuestionItemArray     m_SelectedQuestionList;
    int                    m_nSelectedBankIndex;
    BOOL                   m_bTestQuestionsLoaded;

    CStatic                m_staticPdfLabel;
    CComboBox              m_comboPdf;
    CButton                m_btnSelectPdf;

    CButton                m_radioAllPages;
    CButton                m_radioPageRange;
    CStatic                m_staticPageStartLabel;
    CEdit                  m_editPageStart;
    CStatic                m_staticPageEndLabel;
    CEdit                  m_editPageEnd;

    CStatic                m_staticCountLabel;
    CEdit                  m_editQuestionCount;
    CButton                m_btnGenerate;

    CStatic                m_staticPreviewLabel;
    CTabCtrl               m_tabQuestion;
    CPdfViewerCtrl         m_pdfPreview;
    CGeneratedQuestionListBox m_listGenerated;
    CRichEditCtrl          m_richQuestion;
    CRichEditCtrl          m_richBank;
    std::vector<long>      m_arrBankBlockStarts;
    CButton                m_btnBankDelete;
    CButton                m_btnBankMoveUp;
    CButton                m_btnBankMoveDown;
    CStatic                m_staticBankCount;

    CButton                m_btnUse;
    CButton                m_btnRegenerate;
    CButton                m_btnAddMore;
    CButton                m_btnSave;
    CButton                m_btnTest;
    CButton                m_btnChatGpt;
    CStatic                m_staticChatGptProgress;

    double                 m_dSplitRatio;
    double                 m_dGeneratedListSplitRatio;
    BOOL                   m_bDraggingSplit;
    BOOL                   m_bDraggingGeneratedSplit;
    CRect                  m_rcSplitter;
    CRect                  m_rcGeneratedSplitter;
    CRect                  m_rcGeneratedBody;
    int                    m_nActiveTab;
    BOOL                   m_bChatGptTestRunning;
    BOOL                   m_bQuestionGenerateRunning;

    CFont                  m_fontEmphasis;
    CFont                  m_fontGenerateBtn;

    void CreateChildControls();
    void ApplyEmphasisFonts();
    void UpdateLayout();
    void UpdateBankStatusLabel();
    void ShowActiveTab();
    BOOL HitTestSplitter(const CPoint& pt) const;
    BOOL HitTestGeneratedSplitter(const CPoint& pt) const;
    void LoadUiSettings();
    void SaveUiSettings();
    void UpdatePageRangeEnable();
    void UpdatePdfCombo();
    void LoadDummyQuestionText();
    void ReadSettingsFromControls();
    BOOL NavigatePdfToQuestionSource(const QUESTION_ITEM& item);
    int FindGeneratedListIndexForQuestion(int nQuestionIndex) const;
    void HandleGeneratedListSelection(int nListIndex);
    void StampQuestionSourcePdfPaths(CQuestionItemArray& questions, const CStringW& strPdfPath);
    void LogQuestionSelect(
        const CStringW& strListType,
        int nDisplayIndex,
        int nQuestionIndex,
        const QUESTION_ITEM& item,
        int nPdfPageBefore,
        BOOL bMoveResult,
        int nPdfPageAfter) const;
    int ResolveGeneratedQuestionIndex(int nListIndex) const;
    void RefreshGeneratedList();
    void SelectGeneratedQuestion(int nIndex, BOOL bNavigatePdf = TRUE);
    CStringW GetDummyQuestionText() const;
    CStringW GetSelectedPdfPath() const;

    BOOL LoadTestQuestionFile();
    void DisplayCurrentTestQuestion(BOOL bNavigatePdf = TRUE);
    void AdvanceToNextTestQuestion();
    BOOL AdoptCurrentTestQuestion(CStringW& strMessage);
    void RefreshBankList();
    void SelectBankQuestion(int nIndex, BOOL bNavigatePdf = TRUE);
    int FindBankIndexByCharPos(long nCharPos) const;
    BOOL HasSelectedBankItem() const;
    int GetSelectedBankIndex() const;
    void SetGeneratedActionButtonsEnabled(BOOL bEnabled);
    void SetChatGptTestBusy(BOOL bBusy);
    void SetQuestionGenerateBusy(BOOL bBusy);
    void StartChatGptConnectionTest(const SCP_OPENAI_CONFIG& config);
    void StartQuestionGeneration(const OPENAI_GENERATE_REQUEST& request);

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnLbnSelchangeGeneratedList();
    afx_msg void OnEnSelchangeBankRich(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedSelectPdf();
    afx_msg void OnBnClickedRadioAllPages();
    afx_msg void OnBnClickedRadioPageRange();
    afx_msg void OnCbnSelchangePdfCombo();
    afx_msg void OnBnClickedGenerate();
    afx_msg void OnBnClickedUse();
    afx_msg void OnBnClickedRegenerate();
    afx_msg void OnBnClickedAddMore();
    afx_msg void OnBnClickedSave();
    afx_msg void OnBnClickedTest();
    afx_msg void OnBnClickedChatGpt();
    afx_msg LRESULT OnChatGptTestDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnQuestionGenerateDone(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnGeneratedListSelectionMsg(WPARAM wParam, LPARAM lParam);
    afx_msg void OnBnClickedBankDelete();
    afx_msg void OnBnClickedBankMoveUp();
    afx_msg void OnBnClickedBankMoveDown();

    DECLARE_MESSAGE_MAP()
};
