#pragma once

#include "Resource.h"

#include "TrainingManager.h"

#include "VideoViewDlg.h"

#include "ImageViewCtrl.h"
#include "PdfCoverViewCtrl.h"
#include "PdfViewerCtrl.h"



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

    CImageViewCtrl   m_imageView;
    CPdfCoverViewCtrl m_pdfCoverView;
    CPdfViewerCtrl m_pdfViewer;



    int m_nCurrentCourseIndex;

    int m_nCurrentLessonIndex;

    BOOL m_bControlsReady;

    BOOL m_bPdfListMode;
    BOOL m_bPdfViewerActive;

    BOOL m_bImageListMode;

    BOOL m_bSuppressTreeSelChange;

    BOOL m_bExitCleanupDone;
    BOOL m_bExitConfirmShowing;



    CStringW m_strDataFolder;

    CStringW m_strProgressFile;

    CStringArray m_arrPdfFiles;

    CStringArray m_arrImageFiles;

    int m_nSelectedPdfIndex;



    virtual void DoDataExchange(CDataExchange* pDX);

    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg);



    void SetupKoreanUI();

    BOOL LoadCourseData();

    void ReloadCourses();

    void BuildCourseTree();

    void BuildPdfTree();

    void BuildImageTree();

    void ShowCourseTree();

    void ShowPdfTree();

    void ShowImageTree();

    void EnsureCourseTree();

    void CollectPdfIndicesInFolder(HTREEITEM hFolderItem, CArray<int>& arrPdfIndices);

    void DisplayPdfCoversInFolder(HTREEITEM hFolderItem, int nSelectPdfIndex = -1);

    void OnPdfTreeItemSelected(HTREEITEM hItem);

    void BringPdfCoverToFront();
    void BringPdfViewerToFront();
    void OpenPdfViewer(int nPdfIndex);
    void ClosePdfViewer();

    void DisplayImageByIndex(int nImageIndex);

    void DisplayImagesInFolder(HTREEITEM hFolderItem);

    void OnImageTreeItemSelected(HTREEITEM hItem);

    void SelectImageTreeItemByIndex(int nImageIndex);

    void RefreshPdfFileList();

    void RefreshImageFileList();

    void UpdateContentButtons();

    void UpdateTreeItemState(int nCourseIndex, int nLessonIndex);



    void DisplayLesson(int nCourseIndex, int nLessonIndex);

    void ClearLessonDisplay();



    void LoadThumbnail(const CStringW& strImagePath);

    void LoadImageGrid(const CArray<int>& arrImageIndices);



    BOOL SelectLesson(int nCourseIndex, int nLessonIndex);

    void MoveToAdjacentLesson(int nDirection);



    void LaunchUrl(const CStringW& strUrl);

    void LaunchFile(const CStringW& strRelativePath);



    void UpdateNavigationButtons();

    int GetDescriptionMinHeight() const;

    void LayoutControls(int cx, int cy);
    void BringImageViewToFront();

    void DiscardPendingCloseMessages();
    void CleanupResourcesBeforeExit();



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

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

    afx_msg void OnDestroy();

    afx_msg LRESULT OnEnsureVideoPlayer(WPARAM wParam, LPARAM lParam);

    afx_msg LRESULT OnImageViewItemSelected(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnPdfCoverItemSelected(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnPdfViewerBackToList(WPARAM wParam, LPARAM lParam);



    DECLARE_MESSAGE_MAP()

};

