#pragma once
#include "Resource.h"
#include "TrainingManager.h"
#include "VideoViewDlg.h"

// ============================================================================
// TrainingDlg.h - Main training content player dialog (CStringW / Unicode)
// ============================================================================

class CTrainingDlg : public CDialogEx
{
public:
    CTrainingDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TRAININGCONTENTPLAYER_DIALOG };
#endif

protected:
    CTrainingManager m_Manager;
    CTreeCtrl        m_treeCourse;
    CStatic          m_staticTitle;
    CEdit            m_editDescription;
    CStatic          m_staticThumbnail;

    int m_nCurrentCourseIndex;
    int m_nCurrentLessonIndex;
    BOOL m_bControlsReady;
    BOOL m_bPdfListMode;
    BOOL m_bSuppressTreeSelChange;

    CStringW m_strDataFolder;
    CStringW m_strProgressFile;
    CStringArray m_arrPdfFiles;

    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    void SetupKoreanUI();
    BOOL LoadCourseData();
    void ReloadCourses();
    void BuildCourseTree();
    void BuildPdfTree();
    void ShowCourseTree();
    void ShowPdfTree();
    void EnsureCourseTree();
    void OpenPdfByIndex(int nPdfIndex);
    void RefreshPdfFileList();
    void UpdateContentButtons();
    void UpdateTreeItemState(int nCourseIndex, int nLessonIndex);

    void DisplayLesson(int nCourseIndex, int nLessonIndex);
    void ClearLessonDisplay();

    void LoadThumbnail(const CStringW& strImagePath);

    BOOL SelectLesson(int nCourseIndex, int nLessonIndex);
    void MoveToAdjacentLesson(int nDirection);

    void LaunchUrl(const CStringW& strUrl);
    void LaunchFile(const CStringW& strRelativePath);

    void UpdateNavigationButtons();
    int GetDescriptionMinHeight() const;
    void LayoutControls(int cx, int cy);

    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnSelchangedTreeCourse(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedBtnYoutube();
    afx_msg void OnBnClickedBtnPdf();
    afx_msg void OnBnClickedBtnImage();
    afx_msg void OnBnClickedBtnNext();
    afx_msg void OnBnClickedBtnPrev();
    afx_msg void OnBnClickedBtnComplete();
    afx_msg void OnBnClickedBtnReload();
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg LRESULT OnEnsureVideoPlayer(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()
};
