#pragma once

#include "QuestionItem.h"
#include "PdfViewerCtrl.h"

// ============================================================================
// QuizGenViewCtrl.h - AI 문제 생성 전용 View (1단계: UI 구조)
// ============================================================================

class CQuizGenViewCtrl : public CWnd
{
    DECLARE_DYNAMIC(CQuizGenViewCtrl)

public:
    CQuizGenViewCtrl();
    virtual ~CQuizGenViewCtrl();

    BOOL CreateOverPlaceholder(CWnd* pParent, CWnd* pPlaceholder, UINT nHostId);

    void SetPdfFileList(const CStringArray& arrPdfFiles);
    void SelectPdfByIndex(int nPdfIndex);
    int GetSelectedPdfIndex() const { return m_nSelectedPdfIndex; }

    BOOL IsPreviewActive() const;
    BOOL HandlePreviewMouseWheel(short zDelta, const CPoint& ptScreen);
    BOOL HandlePreviewKeyDown(UINT nChar);

    const CQuestionItemArray& GetQuestionItems() const { return m_arrQuestions; }
    const QUIZ_GEN_SETTINGS& GetSettings() const { return m_settings; }

    void Relayout();

protected:
    QUIZ_GEN_SETTINGS   m_settings;
    CQuestionItemArray  m_arrQuestions;
    CStringArray        m_arrPdfFiles;
    int                 m_nSelectedPdfIndex;

    CStatic             m_staticPdfLabel;
    CComboBox           m_comboPdf;
    CButton             m_btnSelectPdf;

    CButton             m_radioAllPages;
    CButton             m_radioPageRange;
    CStatic             m_staticPageStartLabel;
    CEdit               m_editPageStart;
    CStatic             m_staticPageEndLabel;
    CEdit               m_editPageEnd;

    CStatic             m_staticCountLabel;
    CEdit               m_editQuestionCount;
    CButton             m_btnGenerate;

    CStatic             m_staticPreviewLabel;
    CStatic             m_staticQuestionLabel;
    CPdfViewerCtrl      m_pdfPreview;
    CRichEditCtrl       m_richQuestion;

    CButton             m_btnUse;
    CButton             m_btnRegenerate;
    CButton             m_btnAddMore;
    CButton             m_btnSave;

    void CreateChildControls();
    void UpdateLayout();
    void UpdatePageRangeEnable();
    void UpdatePdfCombo();
    void LoadDummyQuestionText();
    void ReadSettingsFromControls();
    CStringW GetDummyQuestionText() const;
    CStringW GetSelectedPdfPath() const;

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnBnClickedSelectPdf();
    afx_msg void OnBnClickedRadioAllPages();
    afx_msg void OnBnClickedRadioPageRange();
    afx_msg void OnCbnSelchangePdfCombo();
    afx_msg void OnBnClickedGenerate();
    afx_msg void OnBnClickedUse();
    afx_msg void OnBnClickedRegenerate();
    afx_msg void OnBnClickedAddMore();
    afx_msg void OnBnClickedSave();

    DECLARE_MESSAGE_MAP()
};
