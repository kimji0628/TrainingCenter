#include "pch.h"
#include "TrainingDlg.h"
#include "StartDlg.h"
#include "PdfRenderEngine.h"
#include "Util.h"

// Tree 아이템 데이터: 상위 16비트 = 코스 인덱스, 하위 16비트 = 레슨 인덱스 (0xFFFF = 코스 노드)
#define MAKE_TREE_DATA(course, lesson)  ((static_cast<DWORD_PTR>(course) << 16) | (lesson))
#define GET_COURSE_INDEX(data)          (static_cast<int>((data) >> 16))
#define GET_LESSON_INDEX(data)          (static_cast<int>((data) & 0xFFFF))
#define COURSE_NODE_LESSON_INDEX        0xFFFF
#define PDF_TREE_COURSE_MARKER          0xFFFE
#define PDF_TREE_FOLDER_MARKER          0xFFFD

#define MAKE_PDF_TREE_DATA(index)       ((static_cast<DWORD_PTR>(PDF_TREE_COURSE_MARKER) << 16) | static_cast<DWORD_PTR>(index))
#define MAKE_PDF_FOLDER_DATA()          ((static_cast<DWORD_PTR>(PDF_TREE_FOLDER_MARKER) << 16) | static_cast<DWORD_PTR>(0xFFFF))
#define IS_PDF_TREE_ITEM(data)          (GET_COURSE_INDEX(data) == PDF_TREE_COURSE_MARKER)
#define IS_PDF_FOLDER_ITEM(data)        (GET_COURSE_INDEX(data) == PDF_TREE_FOLDER_MARKER)
#define IMAGE_TREE_FILE_MARKER          0xFFFC
#define IMAGE_TREE_FOLDER_MARKER        0xFFFB

#define MAKE_IMAGE_TREE_DATA(index)     ((static_cast<DWORD_PTR>(IMAGE_TREE_FILE_MARKER) << 16) | static_cast<DWORD_PTR>(index))
#define MAKE_IMAGE_FOLDER_DATA()        ((static_cast<DWORD_PTR>(IMAGE_TREE_FOLDER_MARKER) << 16) | static_cast<DWORD_PTR>(0xFFFF))
#define IS_IMAGE_TREE_ITEM(data)        (GET_COURSE_INDEX(data) == IMAGE_TREE_FILE_MARKER)
#define IS_IMAGE_FOLDER_ITEM(data)      (GET_COURSE_INDEX(data) == IMAGE_TREE_FOLDER_MARKER)

namespace
{
    CStringW NormalizeMediaRootPath(const CStringW& strFolder)
    {
        CStringW strPath = strFolder;
        if (!strPath.IsEmpty() && strPath[strPath.GetLength() - 1] != L'\\')
            strPath += L'\\';
        return strPath;
    }

    CStringW GetMediaRelativePath(const CStringW& strRootFolder, const CStringW& strFullPath)
    {
        CStringW strRoot = NormalizeMediaRootPath(strRootFolder);
        if (strFullPath.GetLength() >= strRoot.GetLength() &&
            _wcsnicmp(strFullPath, strRoot, strRoot.GetLength()) == 0)
        {
            return strFullPath.Mid(strRoot.GetLength());
        }

        int nPos = strFullPath.ReverseFind(L'\\');
        return (nPos >= 0) ? strFullPath.Mid(nPos + 1) : strFullPath;
    }

    HTREEITEM FindOrCreateMediaFolderNode(
        CTreeCtrl& tree,
        CStringArray& arrFolderKeys,
        CArray<HTREEITEM>& arrFolderItems,
        const CStringW& strFolderKey,
        const CStringW& strFolderName,
        HTREEITEM hParent,
        DWORD_PTR dwFolderData)
    {
        for (int i = 0; i < arrFolderKeys.GetSize(); ++i)
        {
            if (arrFolderKeys[i].CompareNoCase(strFolderKey) == 0)
                return arrFolderItems[i];
        }

        HTREEITEM hFolder = tree.InsertItem(strFolderName, hParent, TVI_LAST);
        tree.SetItemData(hFolder, dwFolderData);
        arrFolderKeys.Add(strFolderKey);
        arrFolderItems.Add(hFolder);
        tree.Expand(hFolder, TVE_EXPAND);
        return hFolder;
    }

    HTREEITEM FindImageTreeItemRecursive(CTreeCtrl& tree, HTREEITEM hItem, int nImageIndex)
    {
        if (hItem == nullptr)
            return nullptr;

        DWORD_PTR dwData = tree.GetItemData(hItem);
        if (IS_IMAGE_TREE_ITEM(dwData) &&
            static_cast<int>(GET_LESSON_INDEX(dwData)) == nImageIndex)
        {
            return hItem;
        }

        for (HTREEITEM hChild = tree.GetChildItem(hItem);
            hChild != nullptr;
            hChild = tree.GetNextSiblingItem(hChild))
        {
            HTREEITEM hFound = FindImageTreeItemRecursive(tree, hChild, nImageIndex);
            if (hFound != nullptr)
                return hFound;
        }

        return nullptr;
    }

    HTREEITEM FindPdfTreeItemRecursive(CTreeCtrl& tree, HTREEITEM hItem, int nPdfIndex)
    {
        if (hItem == nullptr)
            return nullptr;

        const DWORD_PTR dwData = tree.GetItemData(hItem);
        if (IS_PDF_TREE_ITEM(dwData) &&
            static_cast<int>(GET_LESSON_INDEX(dwData)) == nPdfIndex)
        {
            return hItem;
        }

        for (HTREEITEM hChild = tree.GetChildItem(hItem);
            hChild != nullptr;
            hChild = tree.GetNextSiblingItem(hChild))
        {
            HTREEITEM hFound = FindPdfTreeItemRecursive(tree, hChild, nPdfIndex);
            if (hFound != nullptr)
                return hFound;
        }

        return nullptr;
    }

    CStringW FormatLessonTreeItemText(const CTrainingLesson& lesson)
    {
        CStringW strItemText = lesson.m_strTitle;

        if (!lesson.m_strVideo.IsEmpty())
        {
            const CStringW strPrefix = TrainingUtil::IsUrlVideo(lesson.m_strVideo)
                ? L"\u25B6 "      // ▶ YouTube / URL
                : L"\U0001F3AC "; // 🎬 로컬 동영상
            strItemText = strPrefix + strItemText;
        }

        if (lesson.m_bCompleted)
            strItemText = L"[완료] " + strItemText;

        return strItemText;
    }

    int MeasureToolbarButtonWidth(CWnd* pParent, CWnd* pButton)
    {
        if (pParent == nullptr || pButton == nullptr ||
            !::IsWindow(pButton->GetSafeHwnd()))
        {
            return 90;
        }

        CStringW strText;
        pButton->GetWindowText(strText);
        if (strText.IsEmpty())
            return 90;

        CClientDC dc(pParent);
        CFont* pFont = pButton->GetFont();
        if (pFont == nullptr)
            pFont = pParent->GetFont();

        CFont* pOld = dc.SelectObject(pFont);
        CSize sz = dc.GetTextExtent(strText);
        dc.SelectObject(pOld);

        return static_cast<int>(sz.cx * 1.5) + 28;
    }

    int MeasureToolbarGap(int nButtonWidth)
    {
        return max(12, nButtonWidth / 4);
    }

    void LayoutToolbarButtons(
        CWnd* pParent,
        const UINT* pButtonIds,
        int nButtonCount,
        int nContentLeft,
        int nContentWidth,
        int nBtnY,
        int nBtnHeight)
    {
        if (pParent == nullptr || pButtonIds == nullptr || nButtonCount <= 0)
            return;

        CArray<int> arrWidths;
        arrWidths.SetSize(nButtonCount);

        int nTotalWidth = 0;
        int nMaxWidth = 0;
        for (int i = 0; i < nButtonCount; ++i)
        {
            CWnd* pButton = pParent->GetDlgItem(pButtonIds[i]);
            const int nWidth = MeasureToolbarButtonWidth(pParent, pButton);
            arrWidths[i] = nWidth;
            nTotalWidth += nWidth;
            nMaxWidth = max(nMaxWidth, nWidth);
        }

        const int nGap = MeasureToolbarGap(nMaxWidth);
        nTotalWidth += nGap * (nButtonCount - 1);

        int nRowCount = 1;
        if (nTotalWidth > nContentWidth)
            nRowCount = 2;

        int nBtnIndex = 0;
        for (int nRow = 0; nRow < nRowCount && nBtnIndex < nButtonCount; ++nRow)
        {
            const int nRowY = nBtnY + nRow * (nBtnHeight + 8);
            int nBtnX = nContentLeft;
            int nRowUsed = 0;

            while (nBtnIndex < nButtonCount)
            {
                const int nWidth = arrWidths[nBtnIndex];
                const int nNeed = (nRowUsed == 0) ? nWidth : (nGap + nWidth);

                if (nRowCount > 1 && nRowUsed > 0 &&
                    (nBtnX - nContentLeft) + nNeed > nContentWidth)
                {
                    break;
                }

                if (nRowCount == 1 && nBtnX + nWidth > nContentLeft + nContentWidth)
                    break;

                CWnd* pButton = pParent->GetDlgItem(pButtonIds[nBtnIndex]);
                if (pButton != nullptr && pButton->GetSafeHwnd() != nullptr)
                {
                    if (nRowUsed > 0)
                        nBtnX += nGap;
                    pButton->SetWindowPos(
                        nullptr,
                        nBtnX,
                        nRowY,
                        nWidth,
                        nBtnHeight,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                    nBtnX += nWidth;
                    nRowUsed += nNeed;
                }

                ++nBtnIndex;
            }
        }
    }

    int GetToolbarBottomY(
        const UINT* pButtonIds,
        int nButtonCount,
        CWnd* pParent,
        int nContentWidth,
        int nBtnY,
        int nBtnHeight)
    {
        if (pButtonIds == nullptr || nButtonCount <= 0)
            return nBtnY + nBtnHeight;

        int nMaxWidth = 0;
        int nTotalWidth = 0;
        for (int i = 0; i < nButtonCount; ++i)
        {
            CWnd* pButton = pParent->GetDlgItem(pButtonIds[i]);
            const int nWidth = MeasureToolbarButtonWidth(pParent, pButton);
            nTotalWidth += nWidth;
            nMaxWidth = max(nMaxWidth, nWidth);
        }

        const int nGap = MeasureToolbarGap(nMaxWidth);
        nTotalWidth += nGap * (nButtonCount - 1);

        if (nTotalWidth > nContentWidth)
            return nBtnY + (nBtnHeight + 8) + nBtnHeight;

        return nBtnY + nBtnHeight;
    }
}

// ============================================================================
// TrainingDlg.cpp - 메인 교육 콘텐츠 플레이어 다이얼로그 구현
// ============================================================================

CTrainingDlg::CTrainingDlg(CWnd* pParent)
    : CDialogEx(IDD_TRAININGCONTENTPLAYER_DIALOG, pParent)
    , m_nCurrentCourseIndex(-1)
    , m_nCurrentLessonIndex(-1)
    , m_bControlsReady(FALSE)
    , m_bPdfListMode(FALSE)
    , m_bPdfViewerActive(FALSE)
    , m_bImageListMode(FALSE)
    , m_bQuizGenMode(FALSE)
    , m_bSuppressTreeSelChange(FALSE)
    , m_bExitCleanupDone(FALSE)
    , m_bExitConfirmShowing(FALSE)
    , m_nSelectedPdfIndex(-1)
{
}

void CTrainingDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TREE_COURSE, m_treeCourse);
    DDX_Control(pDX, IDC_STATIC_TITLE, m_staticTitle);
    DDX_Control(pDX, IDC_EDIT_DESCRIPTION, m_editDescription);
    DDX_Control(pDX, IDC_STATIC_THUMBNAIL, m_staticThumbnail);
}

BEGIN_MESSAGE_MAP(CTrainingDlg, CDialogEx)
    ON_NOTIFY(TVN_SELCHANGED, IDC_TREE_COURSE, &CTrainingDlg::OnSelchangedTreeCourse)
    ON_BN_CLICKED(IDC_BTN_YOUTUBE, &CTrainingDlg::OnBnClickedBtnYoutube)
    ON_BN_CLICKED(IDC_BTN_PDF, &CTrainingDlg::OnBnClickedBtnPdf)
    ON_BN_CLICKED(IDC_BTN_IMAGE, &CTrainingDlg::OnBnClickedBtnImage)
    ON_BN_CLICKED(IDC_BTN_QUIZ_GEN, &CTrainingDlg::OnBnClickedBtnQuizGen)
    ON_BN_CLICKED(IDC_BTN_NEXT, &CTrainingDlg::OnBnClickedBtnNext)
    ON_BN_CLICKED(IDC_BTN_PREV, &CTrainingDlg::OnBnClickedBtnPrev)
    ON_BN_CLICKED(IDC_BTN_COMPLETE, &CTrainingDlg::OnBnClickedBtnComplete)
    ON_BN_CLICKED(IDC_BTN_RELOAD, &CTrainingDlg::OnBnClickedBtnReload)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_CLOSE()
    ON_WM_SYSCOMMAND()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_USER + 100, &CTrainingDlg::OnEnsureVideoPlayer)
    ON_MESSAGE(WM_IMAGE_VIEW_ITEM_SELECTED, &CTrainingDlg::OnImageViewItemSelected)
    ON_MESSAGE(WM_PDF_COVER_ITEM_SELECTED, &CTrainingDlg::OnPdfCoverItemSelected)
    ON_MESSAGE(WM_PDF_VIEWER_BACK_TO_LIST, &CTrainingDlg::OnPdfViewerBackToList)
END_MESSAGE_MAP()

void CTrainingDlg::SetupKoreanUI()
{
    // Unicode CStringW literals for Korean UI text
    GetDlgItem(IDC_BTN_YOUTUBE)->SetWindowText(L"영상보기");
    GetDlgItem(IDC_BTN_PDF)->SetWindowText(L"PDF보기");
    GetDlgItem(IDC_BTN_IMAGE)->SetWindowText(L"이미지보기");
    GetDlgItem(IDC_BTN_QUIZ_GEN)->SetWindowText(L"문제생성");
    GetDlgItem(IDC_BTN_NEXT)->SetWindowText(L"다음");
    GetDlgItem(IDC_BTN_PREV)->SetWindowText(L"이전");
    GetDlgItem(IDC_BTN_COMPLETE)->SetWindowText(L"학습완료");
    GetDlgItem(IDC_BTN_RELOAD)->SetWindowText(L"새로고침");

    TrainingUtil::ApplyKoreanFont(this);
    TrainingUtil::ApplyKoreanFont(&m_treeCourse);
    TrainingUtil::ApplyKoreanFont(&m_staticTitle);
    TrainingUtil::ApplyKoreanFont(&m_editDescription);
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_YOUTUBE));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_PDF));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_IMAGE));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_QUIZ_GEN));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_NEXT));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_PREV));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_COMPLETE));
    TrainingUtil::ApplyKoreanFont(GetDlgItem(IDC_BTN_RELOAD));
}

BOOL CTrainingDlg::LoadCourseData()
{
    m_strDataFolder = TrainingUtil::GetDataFolder();
    m_strProgressFile = TrainingUtil::GetProgressFilePath(m_strDataFolder);

    CreateDirectoryW(m_strDataFolder, nullptr);

    if (!m_Manager.LoadAllCourses(m_strDataFolder))
        return FALSE;

    m_Manager.LoadProgress(m_strProgressFile);
    return TRUE;
}

void CTrainingDlg::ReloadCourses()
{
    CStringW strSavedTitle;
    if (const CTrainingLesson* pLesson = m_Manager.GetLesson(
        m_nCurrentCourseIndex, m_nCurrentLessonIndex))
    {
        strSavedTitle = pLesson->m_strTitle;
    }

    if (!LoadCourseData())
    {
        AfxMessageBox(
            L"JSON 코스 데이터를 다시 불러오지 못했습니다.\n"
            L"Data 폴더와 JSON 파일을 확인하세요.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    BuildCourseTree();

    if (!strSavedTitle.IsEmpty())
    {
        for (int c = 0; c < m_Manager.m_Courses.GetSize(); ++c)
        {
            for (int l = 0; l < m_Manager.m_Courses[c].m_Lessons.GetSize(); ++l)
            {
                if (m_Manager.m_Courses[c].m_Lessons[l].m_strTitle == strSavedTitle)
                {
                    SelectLesson(c, l);
                    return;
                }
            }
        }
    }

    if (m_Manager.m_Courses.GetSize() > 0 &&
        m_Manager.m_Courses[0].m_Lessons.GetSize() > 0)
    {
        SelectLesson(0, 0);
    }
    else
    {
        ClearLessonDisplay();
        m_nCurrentCourseIndex = -1;
        m_nCurrentLessonIndex = -1;
        UpdateNavigationButtons();
    }
}

BOOL CTrainingDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), TRUE);
    SetIcon(AfxGetApp()->LoadIcon(IDR_MAINFRAME), FALSE);

    SetWindowText(L"스마트 강의 플레이어");
    SetupKoreanUI();

    // Pre-create WebView2 player after the modal message loop starts
    PostMessage(WM_USER + 100, 0, 0);

    if (!m_imageView.CreateOverPlaceholder(this, &m_staticThumbnail, IDC_IMAGE_VIEW_HOST))
    {
        AfxMessageBox(
            L"이미지 보기 영역 초기화에 실패했습니다.",
            MB_OK | MB_ICONERROR);
        EndDialog(IDCANCEL);
        return FALSE;
    }

    if (!m_pdfCoverView.CreateOverPlaceholder(this, &m_staticThumbnail, IDC_PDF_COVER_VIEW))
    {
        AfxMessageBox(
            L"PDF 표지 보기 영역 초기화에 실패했습니다.",
            MB_OK | MB_ICONERROR);
        EndDialog(IDCANCEL);
        return FALSE;
    }
    m_pdfCoverView.ShowWindow(SW_HIDE);

    if (!m_pdfViewer.CreateOverPlaceholder(this, &m_staticThumbnail, IDC_PDF_VIEWER_HOST))
    {
        AfxMessageBox(
            L"PDF 뷰어 영역 초기화에 실패했습니다.",
            MB_OK | MB_ICONERROR);
        EndDialog(IDCANCEL);
        return FALSE;
    }
    m_pdfViewer.ShowWindow(SW_HIDE);

    if (!m_quizGenView.CreateOverPlaceholder(this, &m_staticThumbnail, IDC_QUIZGEN_HOST))
    {
        AfxMessageBox(
            L"문제생성 화면 초기화에 실패했습니다.",
            MB_OK | MB_ICONERROR);
        EndDialog(IDCANCEL);
        return FALSE;
    }
    m_quizGenView.ShowWindow(SW_HIDE);

    RefreshPdfFileList();
    RefreshImageFileList();

    if (!LoadCourseData())
    {
        AfxMessageBox(
            L"Data 폴더에서 JSON 코스 파일을 찾을 수 없습니다.\n"
            L"Data 폴더에 JSON 파일을 추가한 후 다시 실행하세요.",
            MB_OK | MB_ICONWARNING);
    }
    else
    {
        BuildCourseTree();

        if (m_Manager.m_Courses.GetSize() > 0 &&
            m_Manager.m_Courses[0].m_Lessons.GetSize() > 0)
        {
            SelectLesson(0, 0);
        }
    }

    UpdateNavigationButtons();
    UpdateContentButtons();

    m_bControlsReady = TRUE;

    CRect rcClient;
    GetClientRect(&rcClient);
    LayoutControls(rcClient.Width(), rcClient.Height());

    CStartDlg startDlg(this);
    startDlg.DoModal();

    return TRUE;
}

BOOL CTrainingDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_bPdfViewerActive && ::IsWindow(m_pdfViewer.GetSafeHwnd()))
    {
        if (pMsg->message == WM_KEYDOWN)
        {
            if (m_pdfViewer.HandleKeyDown(static_cast<UINT>(pMsg->wParam)))
                return TRUE;
        }

        if (pMsg->message == WM_MOUSEWHEEL)
        {
            CPoint pt(GET_X_LPARAM(pMsg->lParam), GET_Y_LPARAM(pMsg->lParam));
            if (m_pdfViewer.HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(pMsg->wParam), pt))
                return TRUE;
        }
    }

    if (m_bQuizGenMode && ::IsWindow(m_quizGenView.GetSafeHwnd()))
    {
        if (pMsg->message == WM_KEYDOWN)
        {
            if (m_quizGenView.HandlePreviewKeyDown(static_cast<UINT>(pMsg->wParam)))
                return TRUE;
        }

        if (pMsg->message == WM_MOUSEWHEEL)
        {
            CPoint pt(GET_X_LPARAM(pMsg->lParam), GET_Y_LPARAM(pMsg->lParam));
            if (m_quizGenView.HandlePreviewMouseWheel(GET_WHEEL_DELTA_WPARAM(pMsg->wParam), pt))
                return TRUE;
        }
    }

    if (pMsg->message == WM_MOUSEWHEEL)
    {
        CPoint pt(GET_X_LPARAM(pMsg->lParam), GET_Y_LPARAM(pMsg->lParam));

        if (::IsWindow(m_imageView.GetSafeHwnd()) &&
            m_imageView.IsSingleImageMode() &&
            m_imageView.HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(pMsg->wParam), pt))
        {
            return TRUE;
        }
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}

int CTrainingDlg::GetDescriptionMinHeight() const
{
    CClientDC dc(const_cast<CTrainingDlg*>(this));

    CFont* pFont = m_editDescription.GetFont();
    if (pFont == nullptr)
        pFont = GetFont();

    CFont* pOldFont = dc.SelectObject(pFont);

    TEXTMETRIC tm = {};
    dc.GetTextMetrics(&tm);
    dc.SelectObject(pOldFont);

    const int nLineHeight = tm.tmHeight + tm.tmExternalLeading;
    return nLineHeight * 2 + 10;
}

void CTrainingDlg::LayoutControls(int cx, int cy)
{
    if (!m_bControlsReady || cx <= 0 || cy <= 0)
        return;

    if (!m_treeCourse.GetSafeHwnd() ||
        !m_staticTitle.GetSafeHwnd() ||
        !m_editDescription.GetSafeHwnd() ||
        !m_imageView.GetSafeHwnd())
    {
        return;
    }

    const int nMargin = 10;
    const int nTreeWidth = 325;
    const int nGap = 10;
    const int nContentLeft = nMargin + nTreeWidth + nGap;
    const int nRightMargin = 10;
    const int nContentWidth = max(100, cx - nContentLeft - nRightMargin);
    const int nTitleTop = 10;
    const int nTitleHeight = 24;
    const int nDescTop = nTitleTop + nTitleHeight + 6;
    const int nDescHeight = GetDescriptionMinHeight();
    const int nBtnHeight = 28;
    const int nBtnY = nDescTop + nDescHeight + 8;
    const int nCompleteWidth = 110;
    const int nCompleteHeight = 28;

    const UINT arrButtonIds[] = {
        IDC_BTN_YOUTUBE, IDC_BTN_PDF, IDC_BTN_IMAGE, IDC_BTN_QUIZ_GEN,
        IDC_BTN_RELOAD, IDC_BTN_NEXT, IDC_BTN_PREV
    };
    const int nBtnCount = static_cast<int>(sizeof(arrButtonIds) / sizeof(arrButtonIds[0]));
    const int nToolbarBottom = GetToolbarBottomY(
        arrButtonIds, nBtnCount, this, nContentWidth, nBtnY, nBtnHeight);
    const int nThumbTop = nToolbarBottom + 10;
    const int nThumbBottom = max(nThumbTop + 100, cy - nMargin - nCompleteHeight - 10);

    m_treeCourse.SetWindowPos(
        nullptr, nMargin, nMargin, nTreeWidth, cy - (nMargin * 2),
        SWP_NOZORDER | SWP_NOACTIVATE);

    m_staticTitle.SetWindowPos(
        nullptr, nContentLeft, nTitleTop, nContentWidth, nTitleHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    m_editDescription.SetWindowPos(
        nullptr, nContentLeft, nDescTop, nContentWidth, nDescHeight,
        SWP_NOZORDER | SWP_NOACTIVATE);

    LayoutToolbarButtons(
        this,
        arrButtonIds,
        nBtnCount,
        nContentLeft,
        nContentWidth,
        nBtnY,
        nBtnHeight);

    CWnd* pComplete = GetDlgItem(IDC_BTN_COMPLETE);
    if (pComplete != nullptr && pComplete->GetSafeHwnd() != nullptr)
    {
        pComplete->SetWindowPos(
            nullptr,
            cx - nRightMargin - nCompleteWidth,
            cy - nMargin - nCompleteHeight,
            nCompleteWidth,
            nCompleteHeight,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    m_imageView.SetWindowPos(
        nullptr,
        nContentLeft,
        nThumbTop,
        nContentWidth,
        nThumbBottom - nThumbTop,
        SWP_NOZORDER | SWP_NOACTIVATE);

    m_pdfCoverView.SetWindowPos(
        nullptr,
        nContentLeft,
        nThumbTop,
        nContentWidth,
        nThumbBottom - nThumbTop,
        SWP_NOZORDER | SWP_NOACTIVATE);

    m_pdfViewer.SetWindowPos(
        nullptr,
        nContentLeft,
        nThumbTop,
        nContentWidth,
        nThumbBottom - nThumbTop,
        SWP_NOZORDER | SWP_NOACTIVATE);

    m_quizGenView.SetWindowPos(
        nullptr,
        nContentLeft,
        nThumbTop,
        nContentWidth,
        nThumbBottom - nThumbTop,
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.Relayout();

    CVideoViewDlg::SyncHostArea(&m_imageView);
}

void CTrainingDlg::BringQuizGenToFront()
{
    CVideoViewDlg::HideActive();

    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ShowWindow(SW_HIDE);
    if (::IsWindow(m_pdfViewer.GetSafeHwnd()))
        m_pdfViewer.ShowWindow(SW_HIDE);
    if (::IsWindow(m_imageView.GetSafeHwnd()))
        m_imageView.ShowWindow(SW_HIDE);

    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
    {
        m_quizGenView.SetWindowPos(
            &CWnd::wndTop,
            0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        m_quizGenView.Relayout();
    }
}

void CTrainingDlg::BringImageViewToFront()
{
    CVideoViewDlg::HideActive();

    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ShowWindow(SW_HIDE);
    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.ShowWindow(SW_HIDE);

    if (!::IsWindow(m_imageView.GetSafeHwnd()))
        return;

    m_imageView.SetWindowPos(
        &CWnd::wndTop,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void CTrainingDlg::BringPdfCoverToFront()
{
    CVideoViewDlg::HideActive();

    if (::IsWindow(m_imageView.GetSafeHwnd()))
        m_imageView.ShowWindow(SW_HIDE);
    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.ShowWindow(SW_HIDE);

    if (::IsWindow(m_pdfViewer.GetSafeHwnd()))
        m_pdfViewer.ShowWindow(SW_HIDE);

    if (!::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        return;

    m_pdfCoverView.SetWindowPos(
        &CWnd::wndTop,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void CTrainingDlg::BringPdfViewerToFront()
{
    CVideoViewDlg::HideActive();

    if (::IsWindow(m_imageView.GetSafeHwnd()))
        m_imageView.ShowWindow(SW_HIDE);
    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.ShowWindow(SW_HIDE);

    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ShowWindow(SW_HIDE);

    if (!::IsWindow(m_pdfViewer.GetSafeHwnd()))
        return;

    m_pdfViewer.SetWindowPos(
        &CWnd::wndTop,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void CTrainingDlg::OpenPdfViewer(int nPdfIndex)
{
    if (nPdfIndex < 0 || nPdfIndex >= m_arrPdfFiles.GetSize())
        return;

    const CStringW& strFullPath = m_arrPdfFiles[nPdfIndex];

    BringPdfViewerToFront();

    if (!m_pdfViewer.OpenDocument(strFullPath))
    {
        AfxMessageBox(
            L"PDF 파일을 열 수 없습니다.\n" + strFullPath,
            MB_OK | MB_ICONERROR);
        ClosePdfViewer();
        return;
    }

    m_bPdfViewerActive = TRUE;
    m_nSelectedPdfIndex = nPdfIndex;

    int nSlash = strFullPath.ReverseFind(L'\\');
    CStringW strFileName = (nSlash >= 0) ? strFullPath.Mid(nSlash + 1) : strFullPath;
    m_staticTitle.SetWindowText(strFileName);
    m_editDescription.SetWindowText(
        L"마우스 휠: 이전/다음 페이지 | Ctrl+휠: 확대/축소\n\n"
        L"키보드: ↑↓/PageUp·PageDown(페이지), Home/End(처음·끝), Ctrl+±/0(확대·축소·맞춤)\n"
        L"목록으로 돌아가기를 누르면 PDF 표지 목록으로 복귀합니다.");
}

void CTrainingDlg::ClosePdfViewer()
{
    m_pdfViewer.CloseDocument();
    m_bPdfViewerActive = FALSE;
    BringPdfCoverToFront();
}

void CTrainingDlg::UpdatePdfTreeSelectionHighlightRecursive(
    HTREEITEM hItem,
    int nSelectedPdfIndex)
{
    if (hItem == nullptr)
        return;

    for (HTREEITEM hChild = m_treeCourse.GetChildItem(hItem);
        hChild != nullptr;
        hChild = m_treeCourse.GetNextSiblingItem(hChild))
    {
        UpdatePdfTreeSelectionHighlightRecursive(hChild, nSelectedPdfIndex);
    }

    const DWORD_PTR dwData = m_treeCourse.GetItemData(hItem);
    if (!IS_PDF_TREE_ITEM(dwData))
        return;

    const int nPdfIndex = static_cast<int>(GET_LESSON_INDEX(dwData));
    const UINT nState = (nSelectedPdfIndex >= 0 && nPdfIndex == nSelectedPdfIndex)
        ? TVIS_BOLD
        : 0;
    m_treeCourse.SetItemState(hItem, nState, TVIS_BOLD);
}

void CTrainingDlg::UpdatePdfTreeSelectionHighlight(int nSelectedPdfIndex)
{
    if (!m_bPdfListMode && !m_bQuizGenMode)
        return;

    HTREEITEM hRoot = m_treeCourse.GetRootItem();
    UpdatePdfTreeSelectionHighlightRecursive(hRoot, nSelectedPdfIndex);
}

void CTrainingDlg::SelectPdfTreeItemByIndex(int nPdfIndex)
{
    if (!m_bPdfListMode || nPdfIndex < 0)
        return;

    HTREEITEM hRoot = m_treeCourse.GetRootItem();
    HTREEITEM hItem = FindPdfTreeItemRecursive(m_treeCourse, hRoot, nPdfIndex);
    if (hItem == nullptr)
        return;

    m_bSuppressTreeSelChange = TRUE;
    m_treeCourse.SelectItem(hItem);
    m_treeCourse.EnsureVisible(hItem);
    m_bSuppressTreeSelChange = FALSE;

    UpdatePdfTreeSelectionHighlight(nPdfIndex);
}

void CTrainingDlg::SelectPdfFile(int nPdfIndex)
{
    if (!m_bPdfListMode || nPdfIndex < 0 || nPdfIndex >= m_arrPdfFiles.GetSize())
        return;

    if (m_nSelectedPdfIndex == nPdfIndex && m_bPdfViewerActive)
        return;

    HTREEITEM hRoot = m_treeCourse.GetRootItem();
    HTREEITEM hPdfItem = FindPdfTreeItemRecursive(m_treeCourse, hRoot, nPdfIndex);
    if (hPdfItem != nullptr)
    {
        HTREEITEM hFolder = m_treeCourse.GetParentItem(hPdfItem);
        if (hFolder != nullptr)
            DisplayPdfCoversInFolder(hFolder, nPdfIndex);
    }

    SelectPdfTreeItemByIndex(nPdfIndex);
    OpenPdfViewer(nPdfIndex);
    m_pdfCoverView.SetSelectedPdfIndex(nPdfIndex);
}

void CTrainingDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (m_bControlsReady && nType != SIZE_MINIMIZED)
        LayoutControls(cx, cy);
}

void CTrainingDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CDialogEx::OnGetMinMaxInfo(lpMMI);

    if (lpMMI != nullptr)
    {
        lpMMI->ptMinTrackSize.x = 700;
        lpMMI->ptMinTrackSize.y = 500;
    }
}

void CTrainingDlg::BuildCourseTree()
{
    m_treeCourse.DeleteAllItems();

    for (int c = 0; c < m_Manager.m_Courses.GetSize(); ++c)
    {
        const CTrainingCourse& course = m_Manager.m_Courses[c];

        HTREEITEM hCourse = m_treeCourse.InsertItem(
            course.m_strCourseName, TVI_ROOT, TVI_LAST);
        m_treeCourse.SetItemData(hCourse, MAKE_TREE_DATA(c, COURSE_NODE_LESSON_INDEX));

        for (int l = 0; l < course.m_Lessons.GetSize(); ++l)
        {
            const CTrainingLesson& lesson = course.m_Lessons[l];
            const CStringW strItemText = FormatLessonTreeItemText(lesson);

            HTREEITEM hLesson = m_treeCourse.InsertItem(strItemText, hCourse, TVI_LAST);
            m_treeCourse.SetItemData(hLesson, MAKE_TREE_DATA(c, l));
        }

        m_treeCourse.Expand(hCourse, TVE_EXPAND);
    }
}

void CTrainingDlg::RefreshPdfFileList()
{
    m_arrPdfFiles.RemoveAll();
    TrainingUtil::EnsurePdfFolder();
    TrainingUtil::FindPdfFiles(TrainingUtil::GetPdfFolder(), m_arrPdfFiles);
}

void CTrainingDlg::BuildPdfTree()
{
    m_treeCourse.DeleteAllItems();

    HTREEITEM hRoot = m_treeCourse.InsertItem(L"PDF", TVI_ROOT, TVI_LAST);
    m_treeCourse.SetItemData(hRoot, MAKE_TREE_DATA(0, COURSE_NODE_LESSON_INDEX));

    CStringW strPdfRoot = NormalizeMediaRootPath(TrainingUtil::GetPdfFolder());
    CStringArray arrFolderKeys;
    CArray<HTREEITEM> arrFolderItems;

    CStringW strCurrentPdf;
    if (m_nCurrentCourseIndex >= 0 && m_nCurrentLessonIndex >= 0)
    {
        const CTrainingLesson* pLesson = m_Manager.GetLesson(
            m_nCurrentCourseIndex, m_nCurrentLessonIndex);
        if (pLesson != nullptr && !pLesson->m_strPdfFile.IsEmpty())
            strCurrentPdf = TrainingUtil::ResolveAppPath(pLesson->m_strPdfFile);
    }

    HTREEITEM hSelect = hRoot;
    for (int i = 0; i < m_arrPdfFiles.GetSize(); ++i)
    {
        const CStringW& strPath = m_arrPdfFiles[i];
        CStringW strRelative = GetMediaRelativePath(strPdfRoot, strPath);

        int nSlash = strRelative.ReverseFind(L'\\');
        CStringW strFileName = (nSlash >= 0) ? strRelative.Mid(nSlash + 1) : strRelative;
        HTREEITEM hParent = hRoot;

        if (nSlash >= 0)
        {
            CStringW strFolderPart = strRelative.Left(nSlash);
            CStringW strFolderKey;
            CStringW strFolderRemain = strFolderPart;
            HTREEITEM hFolderParent = hRoot;

            while (!strFolderRemain.IsEmpty())
            {
                int nNextSlash = strFolderRemain.Find(L'\\');
                CStringW strSegment = (nNextSlash >= 0)
                    ? strFolderRemain.Left(nNextSlash)
                    : strFolderRemain;

                if (!strFolderKey.IsEmpty())
                    strFolderKey += L'\\';
                strFolderKey += strSegment;

                hFolderParent = FindOrCreateMediaFolderNode(
                    m_treeCourse,
                    arrFolderKeys,
                    arrFolderItems,
                    strFolderKey,
                    strSegment,
                    hFolderParent,
                    MAKE_PDF_FOLDER_DATA());

                if (nNextSlash < 0)
                    break;

                strFolderRemain = strFolderRemain.Mid(nNextSlash + 1);
            }

            hParent = hFolderParent;
        }

        HTREEITEM hItem = m_treeCourse.InsertItem(strFileName, hParent, TVI_LAST);
        m_treeCourse.SetItemData(hItem, MAKE_PDF_TREE_DATA(i));

        if (!strCurrentPdf.IsEmpty() && strPath.CompareNoCase(strCurrentPdf) == 0)
            hSelect = hItem;
    }

    m_treeCourse.Expand(hRoot, TVE_EXPAND);

    m_bSuppressTreeSelChange = TRUE;
    m_treeCourse.SelectItem(hSelect);
    m_treeCourse.EnsureVisible(hSelect);
    m_bSuppressTreeSelChange = FALSE;
}

void CTrainingDlg::RefreshImageFileList()
{
    m_arrImageFiles.RemoveAll();
    TrainingUtil::FindImageFiles(TrainingUtil::GetImageFolder(), m_arrImageFiles);
}

void CTrainingDlg::BuildImageTree()
{
    m_treeCourse.DeleteAllItems();

    HTREEITEM hRoot = m_treeCourse.InsertItem(L"이미지 파일 목록", TVI_ROOT, TVI_LAST);
    m_treeCourse.SetItemData(hRoot, MAKE_TREE_DATA(0, COURSE_NODE_LESSON_INDEX));

    CStringW strImageRoot = NormalizeMediaRootPath(TrainingUtil::GetImageFolder());
    CStringArray arrFolderKeys;
    CArray<HTREEITEM> arrFolderItems;

    for (int i = 0; i < m_arrImageFiles.GetSize(); ++i)
    {
        const CStringW& strPath = m_arrImageFiles[i];
        CStringW strRelative = GetMediaRelativePath(strImageRoot, strPath);

        int nSlash = strRelative.ReverseFind(L'\\');
        CStringW strFileName = (nSlash >= 0) ? strRelative.Mid(nSlash + 1) : strRelative;
        HTREEITEM hParent = hRoot;

        if (nSlash >= 0)
        {
            CStringW strFolderPart = strRelative.Left(nSlash);
            CStringW strFolderKey;
            CStringW strFolderRemain = strFolderPart;
            HTREEITEM hFolderParent = hRoot;

            while (!strFolderRemain.IsEmpty())
            {
                int nNextSlash = strFolderRemain.Find(L'\\');
                CStringW strSegment = (nNextSlash >= 0)
                    ? strFolderRemain.Left(nNextSlash)
                    : strFolderRemain;

                if (!strFolderKey.IsEmpty())
                    strFolderKey += L'\\';
                strFolderKey += strSegment;

                hFolderParent = FindOrCreateMediaFolderNode(
                    m_treeCourse,
                    arrFolderKeys,
                    arrFolderItems,
                    strFolderKey,
                    strSegment,
                    hFolderParent,
                    MAKE_IMAGE_FOLDER_DATA());

                if (nNextSlash < 0)
                    break;

                strFolderRemain = strFolderRemain.Mid(nNextSlash + 1);
            }

            hParent = hFolderParent;
        }

        HTREEITEM hItem = m_treeCourse.InsertItem(strFileName, hParent, TVI_LAST);
        m_treeCourse.SetItemData(hItem, MAKE_IMAGE_TREE_DATA(i));
    }

    m_treeCourse.Expand(hRoot, TVE_EXPAND);

    m_bSuppressTreeSelChange = TRUE;
    m_treeCourse.SelectItem(hRoot);
    m_treeCourse.EnsureVisible(hRoot);
    m_bSuppressTreeSelChange = FALSE;

    CArray<int> arrAllImages;
    for (int i = 0; i < m_arrImageFiles.GetSize(); ++i)
        arrAllImages.Add(i);

    BringImageViewToFront();
    LoadImageGrid(arrAllImages);
}

void CTrainingDlg::ShowCourseTree()
{
    if (!m_bPdfListMode && !m_bImageListMode && !m_bQuizGenMode)
        return;

    m_bPdfListMode = FALSE;
    m_bImageListMode = FALSE;
    m_bQuizGenMode = FALSE;
    m_bPdfViewerActive = FALSE;
    m_nSelectedPdfIndex = -1;
    m_pdfViewer.CloseDocument();
    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ShowWindow(SW_HIDE);
    if (::IsWindow(m_pdfViewer.GetSafeHwnd()))
        m_pdfViewer.ShowWindow(SW_HIDE);
    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.ShowWindow(SW_HIDE);
    BuildCourseTree();

    if (m_nCurrentCourseIndex >= 0 && m_nCurrentLessonIndex >= 0)
    {
        HTREEITEM hCourse = m_treeCourse.GetRootItem();
        for (int c = 0; c < m_nCurrentCourseIndex && hCourse != nullptr; ++c)
            hCourse = m_treeCourse.GetNextSiblingItem(hCourse);

        if (hCourse != nullptr)
        {
            HTREEITEM hLesson = m_treeCourse.GetChildItem(hCourse);
            for (int l = 0; l < m_nCurrentLessonIndex && hLesson != nullptr; ++l)
                hLesson = m_treeCourse.GetNextSiblingItem(hLesson);

            if (hLesson != nullptr)
            {
                m_bSuppressTreeSelChange = TRUE;
                m_treeCourse.SelectItem(hLesson);
                m_treeCourse.EnsureVisible(hLesson);
                m_bSuppressTreeSelChange = FALSE;
            }
        }
    }
}

void CTrainingDlg::EnsureCourseTree()
{
    if (m_bPdfListMode || m_bImageListMode || m_bQuizGenMode)
        ShowCourseTree();
}

void CTrainingDlg::ShowImageTree()
{
    CVideoViewDlg::HideActive();
    RefreshImageFileList();

    if (m_arrImageFiles.GetSize() == 0)
    {
        AfxMessageBox(
            L"Images 폴더에 이미지 파일이 없습니다.\n"
            L"Bin\\Images 폴더에 PNG/JPG 파일을 추가하세요.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    m_bPdfListMode = FALSE;
    m_bImageListMode = TRUE;
    m_bQuizGenMode = FALSE;
    m_nSelectedPdfIndex = -1;
    BuildImageTree();
    BringImageViewToFront();

    m_staticTitle.SetWindowText(L"이미지 보기");
    m_editDescription.SetWindowText(
        L"좌측 목록에서 폴더 또는 이미지를 선택하세요.\n\n"
        L"폴더를 선택하면 해당 폴더의 이미지가 격자로 표시되고,\n"
        L"이미지를 선택하거나 격자에서 클릭하면 크게 볼 수 있습니다.");
}

void CTrainingDlg::ShowPdfTree()
{
    CVideoViewDlg::HideActive();
    RefreshPdfFileList();

    if (m_arrPdfFiles.GetSize() == 0)
    {
        AfxMessageBox(
            L"PDF 폴더에 PDF 파일이 없습니다.\n"
            L"실행 파일 기준 .\\PDF 폴더에 PDF 파일을 추가하세요.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    m_bPdfListMode = TRUE;
    m_bImageListMode = FALSE;
    m_bQuizGenMode = FALSE;
    m_bPdfViewerActive = FALSE;
    m_pdfViewer.CloseDocument();
    BuildPdfTree();

    BringPdfCoverToFront();
    m_pdfCoverView.ClearView();
    m_nSelectedPdfIndex = -1;

    m_staticTitle.SetWindowText(L"PDF 보기");
    m_editDescription.SetWindowText(
        L"좌측 목록에서 PDF 파일명을 클릭하거나, 폴더를 선택한 뒤 표지를 클릭하세요.\n\n"
        L"PDF 파일명을 클릭하면 즉시 내부 뷰어에서 열리고,\n"
        L"폴더를 선택하면 해당 폴더의 PDF 표지(1페이지)가 카드 형태로 표시됩니다.\n"
        L".\\PDF 폴더와 하위 폴더의 모든 PDF 파일(.pdf, .PDF)이 표시됩니다.");

    HTREEITEM hSelected = m_treeCourse.GetSelectedItem();
    OnPdfTreeItemSelected(hSelected);
}

void CTrainingDlg::ShowQuizGenView()
{
    CVideoViewDlg::HideActive();
    RefreshPdfFileList();

    if (m_arrPdfFiles.GetSize() == 0)
    {
        AfxMessageBox(
            L"PDF 폴더에 PDF 파일이 없습니다.\n"
            L"문제 생성을 위해 .\\PDF 폴더에 PDF 파일을 추가하세요.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    m_bPdfListMode = FALSE;
    m_bImageListMode = FALSE;
    m_bQuizGenMode = TRUE;
    m_bPdfViewerActive = FALSE;
    m_nSelectedPdfIndex = -1;
    m_pdfViewer.CloseDocument();

    BuildPdfTree();
    m_quizGenView.SetPdfFileList(m_arrPdfFiles);
    BringQuizGenToFront();

    m_staticTitle.SetWindowText(L"문제 생성");
    m_editDescription.SetWindowText(
        L"PDF를 선택하고 출제 페이지 범위·문제 개수를 지정한 뒤 [문제 생성]을 누르세요.\n\n"
        L"좌측 PDF 목록 또는 상단 콤보박스에서 PDF를 선택할 수 있습니다.\n"
        L"현재 단계는 UI 구조 설계이며, 실제 AI 문제 생성은 이후 단계에서 구현됩니다.");

    HTREEITEM hSelected = m_treeCourse.GetSelectedItem();
    OnQuizGenTreeItemSelected(hSelected);
}

void CTrainingDlg::OnQuizGenTreeItemSelected(HTREEITEM hItem)
{
    if (!m_bQuizGenMode || hItem == nullptr)
        return;

    const DWORD_PTR dwData = m_treeCourse.GetItemData(hItem);
    if (IS_PDF_TREE_ITEM(dwData))
        SelectQuizGenPdfIndex(static_cast<int>(GET_LESSON_INDEX(dwData)));
}

void CTrainingDlg::SelectQuizGenPdfIndex(int nPdfIndex)
{
    if (!m_bQuizGenMode || nPdfIndex < 0 || nPdfIndex >= m_arrPdfFiles.GetSize())
        return;

    m_nSelectedPdfIndex = nPdfIndex;
    m_quizGenView.SelectPdfByIndex(nPdfIndex);

    HTREEITEM hRoot = m_treeCourse.GetRootItem();
    HTREEITEM hPdfItem = FindPdfTreeItemRecursive(m_treeCourse, hRoot, nPdfIndex);
    if (hPdfItem != nullptr)
    {
        m_bSuppressTreeSelChange = TRUE;
        m_treeCourse.SelectItem(hPdfItem);
        m_treeCourse.EnsureVisible(hPdfItem);
        m_bSuppressTreeSelChange = FALSE;
    }

    UpdatePdfTreeSelectionHighlight(nPdfIndex);
}

void CTrainingDlg::CollectPdfIndicesInFolder(
    HTREEITEM hFolderItem,
    CArray<int>& arrPdfIndices)
{
    arrPdfIndices.RemoveAll();

    if (hFolderItem == nullptr)
        return;

    for (HTREEITEM hChild = m_treeCourse.GetChildItem(hFolderItem);
        hChild != nullptr;
        hChild = m_treeCourse.GetNextSiblingItem(hChild))
    {
        const DWORD_PTR dwData = m_treeCourse.GetItemData(hChild);
        if (IS_PDF_TREE_ITEM(dwData))
            arrPdfIndices.Add(static_cast<int>(GET_LESSON_INDEX(dwData)));
    }
}

void CTrainingDlg::DisplayPdfCoversInFolder(
    HTREEITEM hFolderItem,
    int nSelectPdfIndex)
{
    if (hFolderItem == nullptr)
        return;

    CArray<int> arrPdfIndices;
    CollectPdfIndicesInFolder(hFolderItem, arrPdfIndices);

    BringPdfCoverToFront();
    m_pdfCoverView.ShowPdfCovers(m_arrPdfFiles, arrPdfIndices);

    if (nSelectPdfIndex >= 0)
    {
        m_nSelectedPdfIndex = nSelectPdfIndex;
        m_pdfCoverView.SetSelectedPdfIndex(nSelectPdfIndex);

        if (nSelectPdfIndex < m_arrPdfFiles.GetSize())
        {
            const CStringW& strFullPath = m_arrPdfFiles[nSelectPdfIndex];
            int nSlash = strFullPath.ReverseFind(L'\\');
            CStringW strFileName = (nSlash >= 0) ? strFullPath.Mid(nSlash + 1) : strFullPath;
            m_staticTitle.SetWindowText(strFileName);
        }
    }
    else
    {
        m_nSelectedPdfIndex = -1;
        m_staticTitle.SetWindowText(L"PDF 보기");
    }
}

void CTrainingDlg::OnPdfTreeItemSelected(HTREEITEM hItem)
{
    if (hItem == nullptr)
        return;

    const DWORD_PTR dwData = m_treeCourse.GetItemData(hItem);
    if (IS_PDF_TREE_ITEM(dwData))
    {
        SelectPdfFile(static_cast<int>(GET_LESSON_INDEX(dwData)));
        return;
    }

    if (m_bPdfViewerActive)
        ClosePdfViewer();

    DisplayPdfCoversInFolder(hItem, -1);
    UpdatePdfTreeSelectionHighlight(-1);
}

void CTrainingDlg::PlayLessonVideo(int nCourseIndex, int nLessonIndex)
{
    if (m_bPdfViewerActive)
        ClosePdfViewer();
    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ShowWindow(SW_HIDE);

    const CTrainingLesson* pLesson = m_Manager.GetLesson(nCourseIndex, nLessonIndex);
    if (pLesson == nullptr || pLesson->m_strVideo.IsEmpty())
    {
        AfxMessageBox(
            L"이 강의에는 등록된 동영상이 없습니다.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const CStringW& strVideo = pLesson->m_strVideo;
    if (TrainingUtil::IsUrlVideo(strVideo))
    {
        LaunchUrl(strVideo);
    }
    else
    {
        if (!TrainingUtil::IsSupportedLocalVideo(strVideo))
        {
            AfxMessageBox(
                L"지원하지 않는 동영상 형식입니다.",
                MB_OK | MB_ICONWARNING);
            return;
        }

        CStringW strFullPath = TrainingUtil::ResolveAppPath(strVideo);
        if (GetFileAttributesW(strFullPath) == INVALID_FILE_ATTRIBUTES)
        {
            AfxMessageBox(
                L"동영상 파일을 찾을 수 없습니다.\n\n" + strVideo,
                MB_OK | MB_ICONWARNING);
            return;
        }

        CVideoViewDlg::PlayVideo(this, strVideo, &m_imageView);
    }

    m_staticTitle.SetWindowText(pLesson->m_strTitle);
    m_editDescription.SetWindowText(pLesson->m_strDescription);
}

void CTrainingDlg::UpdateContentButtons()
{
    const CTrainingLesson* pLesson = m_Manager.GetLesson(
        m_nCurrentCourseIndex, m_nCurrentLessonIndex);

    BOOL bHasLesson = (pLesson != nullptr);
    GetDlgItem(IDC_BTN_YOUTUBE)->EnableWindow(
        bHasLesson && !pLesson->m_strVideo.IsEmpty());
    GetDlgItem(IDC_BTN_PDF)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_IMAGE)->EnableWindow(m_arrImageFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_QUIZ_GEN)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
}

void CTrainingDlg::DisplayImageByIndex(int nImageIndex)
{
    if (nImageIndex < 0 || nImageIndex >= m_arrImageFiles.GetSize())
        return;

    BringImageViewToFront();
    m_imageView.ShowSingleImage(m_arrImageFiles[nImageIndex]);
}

void CTrainingDlg::DisplayImagesInFolder(HTREEITEM hFolderItem)
{
    CArray<int> arrIndices;
    if (hFolderItem != nullptr)
    {
        for (HTREEITEM hChild = m_treeCourse.GetChildItem(hFolderItem);
            hChild != nullptr;
            hChild = m_treeCourse.GetNextSiblingItem(hChild))
        {
            DWORD_PTR dwData = m_treeCourse.GetItemData(hChild);
            if (IS_IMAGE_TREE_ITEM(dwData))
                arrIndices.Add(static_cast<int>(GET_LESSON_INDEX(dwData)));
        }
    }

    LoadImageGrid(arrIndices);
}

void CTrainingDlg::OnImageTreeItemSelected(HTREEITEM hItem)
{
    if (hItem == nullptr)
        return;

    DWORD_PTR dwData = m_treeCourse.GetItemData(hItem);
    if (IS_IMAGE_TREE_ITEM(dwData))
        DisplayImageByIndex(static_cast<int>(GET_LESSON_INDEX(dwData)));
    else
        DisplayImagesInFolder(hItem);
}

void CTrainingDlg::SelectImageTreeItemByIndex(int nImageIndex)
{
    HTREEITEM hRoot = m_treeCourse.GetRootItem();
    HTREEITEM hItem = FindImageTreeItemRecursive(m_treeCourse, hRoot, nImageIndex);
    if (hItem != nullptr)
    {
        m_bSuppressTreeSelChange = TRUE;
        m_treeCourse.SelectItem(hItem);
        m_treeCourse.EnsureVisible(hItem);
        m_bSuppressTreeSelChange = FALSE;
    }
}

void CTrainingDlg::LoadImageGrid(const CArray<int>& arrImageIndices)
{
    const int nCount = static_cast<int>(arrImageIndices.GetSize());
    if (nCount <= 0)
    {
        m_imageView.ClearView();
        return;
    }

    if (nCount == 1)
    {
        DisplayImageByIndex(arrImageIndices[0]);
        return;
    }

    BringImageViewToFront();
    m_imageView.ShowImageGrid(m_arrImageFiles, arrImageIndices);
}

void CTrainingDlg::UpdateTreeItemState(int nCourseIndex, int nLessonIndex)
{
    HTREEITEM hCourse = m_treeCourse.GetRootItem();
    for (int c = 0; c < nCourseIndex && hCourse != nullptr; ++c)
    {
        hCourse = m_treeCourse.GetNextSiblingItem(hCourse);
    }

    if (hCourse == nullptr)
        return;

    HTREEITEM hLesson = m_treeCourse.GetChildItem(hCourse);
    for (int l = 0; l < nLessonIndex && hLesson != nullptr; ++l)
    {
        hLesson = m_treeCourse.GetNextSiblingItem(hLesson);
    }

    if (hLesson == nullptr)
        return;

    const CTrainingLesson* pLesson = m_Manager.GetLesson(nCourseIndex, nLessonIndex);
    if (pLesson != nullptr)
        m_treeCourse.SetItemText(hLesson, FormatLessonTreeItemText(*pLesson));
}

void CTrainingDlg::DisplayLesson(int nCourseIndex, int nLessonIndex)
{
    CVideoViewDlg::HideActive();

    if (::IsWindow(m_quizGenView.GetSafeHwnd()))
        m_quizGenView.ShowWindow(SW_HIDE);

    const CTrainingLesson* pLesson = m_Manager.GetLesson(nCourseIndex, nLessonIndex);
    if (pLesson == nullptr)
    {
        ClearLessonDisplay();
        return;
    }

    m_nCurrentCourseIndex = nCourseIndex;
    m_nCurrentLessonIndex = nLessonIndex;

    // 제목 표시
    m_staticTitle.SetWindowText(pLesson->m_strTitle);

    // 설명 표시
    m_editDescription.SetWindowText(pLesson->m_strDescription);

    // 썸네일 표시
    LoadThumbnail(pLesson->m_strImageFile);

    UpdateContentButtons();
    UpdateNavigationButtons();
}

void CTrainingDlg::ClearLessonDisplay()
{
    CVideoViewDlg::HideActive();

    m_staticTitle.SetWindowText(L"");
    m_editDescription.SetWindowText(L"");
    m_imageView.ClearView();

    GetDlgItem(IDC_BTN_YOUTUBE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_PDF)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_IMAGE)->EnableWindow(m_arrImageFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_QUIZ_GEN)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
}

void CTrainingDlg::LoadThumbnail(const CStringW& strImagePath)
{
    BringImageViewToFront();

    if (strImagePath.IsEmpty())
    {
        m_imageView.ClearView();
        return;
    }

    CStringW strFullPath = TrainingUtil::ResolveAppPath(strImagePath);
    if (GetFileAttributesW(strFullPath) == INVALID_FILE_ATTRIBUTES)
    {
        m_imageView.ClearView();
        return;
    }

    m_imageView.ShowSingleImage(strFullPath);
}

BOOL CTrainingDlg::SelectLesson(int nCourseIndex, int nLessonIndex)
{
    const CTrainingLesson* pLesson = m_Manager.GetLesson(nCourseIndex, nLessonIndex);
    if (pLesson == nullptr)
        return FALSE;

    EnsureCourseTree();
    DisplayLesson(nCourseIndex, nLessonIndex);

    // Tree에서 해당 항목 선택
    HTREEITEM hCourse = m_treeCourse.GetRootItem();
    for (int c = 0; c < nCourseIndex && hCourse != nullptr; ++c)
    {
        hCourse = m_treeCourse.GetNextSiblingItem(hCourse);
    }

    if (hCourse == nullptr)
        return FALSE;

    HTREEITEM hLesson = m_treeCourse.GetChildItem(hCourse);
    for (int l = 0; l < nLessonIndex && hLesson != nullptr; ++l)
    {
        hLesson = m_treeCourse.GetNextSiblingItem(hLesson);
    }

    if (hLesson != nullptr)
    {
        m_treeCourse.SelectItem(hLesson);
        m_treeCourse.EnsureVisible(hLesson);
    }

    return TRUE;
}

void CTrainingDlg::MoveToAdjacentLesson(int nDirection)
{
    if (m_nCurrentCourseIndex < 0 || m_nCurrentLessonIndex < 0)
        return;

    int nCourse = m_nCurrentCourseIndex;
    int nLesson = m_nCurrentLessonIndex + nDirection;

    if (nLesson < 0)
    {
        // 이전 코스의 마지막 Lesson으로 이동
        nCourse--;
        if (nCourse < 0)
            return;
        nLesson = static_cast<int>(m_Manager.m_Courses[nCourse].m_Lessons.GetSize()) - 1;
    }
    else if (nLesson >= m_Manager.m_Courses[nCourse].m_Lessons.GetSize())
    {
        // 다음 코스의 첫 Lesson으로 이동
        nCourse++;
        if (nCourse >= m_Manager.m_Courses.GetSize())
            return;
        nLesson = 0;
    }

    SelectLesson(nCourse, nLesson);
}

void CTrainingDlg::UpdateNavigationButtons()
{
    BOOL bHasLesson = (m_nCurrentCourseIndex >= 0 && m_nCurrentLessonIndex >= 0);

    BOOL bHasPrev = FALSE;
    BOOL bHasNext = FALSE;

    if (bHasLesson)
    {
        bHasPrev = !(m_nCurrentCourseIndex == 0 && m_nCurrentLessonIndex == 0);

        int nLastCourse = static_cast<int>(m_Manager.m_Courses.GetSize()) - 1;
        int nLastLesson = static_cast<int>(m_Manager.m_Courses[nLastCourse].m_Lessons.GetSize()) - 1;
        bHasNext = !(m_nCurrentCourseIndex == nLastCourse && m_nCurrentLessonIndex == nLastLesson);
    }

    GetDlgItem(IDC_BTN_PREV)->EnableWindow(bHasPrev);
    GetDlgItem(IDC_BTN_NEXT)->EnableWindow(bHasNext);
    GetDlgItem(IDC_BTN_COMPLETE)->EnableWindow(bHasLesson);
}

void CTrainingDlg::LaunchUrl(const CStringW& strUrl)
{
    if (strUrl.IsEmpty())
        return;

    CVideoViewDlg::PlayVideo(this, strUrl, &m_imageView);
}

LRESULT CTrainingDlg::OnEnsureVideoPlayer(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    CVideoViewDlg::EnsureCreated(this, &m_imageView);
    return 0;
}

LRESULT CTrainingDlg::OnImageViewItemSelected(WPARAM wParam, LPARAM /*lParam*/)
{
    const int nImageIndex = static_cast<int>(wParam);
    DisplayImageByIndex(nImageIndex);
    SelectImageTreeItemByIndex(nImageIndex);
    return 0;
}

LRESULT CTrainingDlg::OnPdfCoverItemSelected(WPARAM wParam, LPARAM /*lParam*/)
{
    const int nPdfIndex = static_cast<int>(wParam);
    SelectPdfFile(nPdfIndex);
    return 0;
}

LRESULT CTrainingDlg::OnPdfViewerBackToList(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (!m_bPdfListMode)
        return 0;

    const int nPrevSelected = m_nSelectedPdfIndex;
    ClosePdfViewer();

    if (nPrevSelected >= 0 && nPrevSelected < m_arrPdfFiles.GetSize())
    {
        HTREEITEM hRoot = m_treeCourse.GetRootItem();
        HTREEITEM hPdfItem = FindPdfTreeItemRecursive(m_treeCourse, hRoot, nPrevSelected);
        if (hPdfItem != nullptr)
        {
            HTREEITEM hFolder = m_treeCourse.GetParentItem(hPdfItem);
            if (hFolder != nullptr)
                DisplayPdfCoversInFolder(hFolder, nPrevSelected);
        }
        SelectPdfTreeItemByIndex(nPrevSelected);
        m_pdfCoverView.SetSelectedPdfIndex(nPrevSelected);
    }
    else
    {
        m_staticTitle.SetWindowText(L"PDF 보기");
    }

    m_editDescription.SetWindowText(
        L"좌측 목록에서 PDF 파일명을 클릭하거나, 폴더를 선택한 뒤 표지를 클릭하세요.\n\n"
        L"PDF 파일명을 클릭하면 즉시 내부 뷰어에서 열리고,\n"
        L"폴더를 선택하면 해당 폴더의 PDF 표지(1페이지)가 카드 형태로 표시됩니다.");

    return 0;
}

void CTrainingDlg::LaunchFile(const CStringW& strRelativePath)
{
    if (strRelativePath.IsEmpty())
        return;

    CStringW strFullPath = TrainingUtil::ResolveAppPath(strRelativePath);

    if (GetFileAttributesW(strFullPath) == INVALID_FILE_ATTRIBUTES)
    {
        AfxMessageBox(L"파일을 찾을 수 없습니다.\n" + strFullPath,
            MB_OK | MB_ICONWARNING);
        return;
    }

    HINSTANCE hResult = ShellExecuteW(
        m_hWnd, L"open", strFullPath, nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) <= 32)
    {
        AfxMessageBox(L"파일을 실행할 수 없습니다.", MB_OK | MB_ICONERROR);
    }
}

void CTrainingDlg::OnSelchangedTreeCourse(NMHDR* pNMHDR, LRESULT* pResult)
{
    if (m_bSuppressTreeSelChange)
    {
        *pResult = 0;
        return;
    }

    LPNMTREEVIEW pNMTreeView = reinterpret_cast<LPNMTREEVIEW>(pNMHDR);
    HTREEITEM hItem = pNMTreeView->itemNew.hItem;

    if (hItem != nullptr)
    {
        DWORD_PTR dwData = m_treeCourse.GetItemData(hItem);

        if (m_bImageListMode)
            OnImageTreeItemSelected(hItem);
        else if (m_bPdfListMode)
            OnPdfTreeItemSelected(hItem);
        else if (m_bQuizGenMode)
            OnQuizGenTreeItemSelected(hItem);
        else
        {
            int nCourseIndex = GET_COURSE_INDEX(dwData);
            int nLessonIndex = GET_LESSON_INDEX(dwData);

            if (nLessonIndex != COURSE_NODE_LESSON_INDEX)
                DisplayLesson(nCourseIndex, nLessonIndex);
        }
    }

    *pResult = 0;
}

void CTrainingDlg::OnBnClickedBtnYoutube()
{
    EnsureCourseTree();
    PlayLessonVideo(m_nCurrentCourseIndex, m_nCurrentLessonIndex);
}

void CTrainingDlg::OnBnClickedBtnPdf()
{
    ShowPdfTree();
}

void CTrainingDlg::OnBnClickedBtnImage()
{
    ShowImageTree();
}

void CTrainingDlg::OnBnClickedBtnQuizGen()
{
    ShowQuizGenView();
}

void CTrainingDlg::OnBnClickedBtnNext()
{
    EnsureCourseTree();
    MoveToAdjacentLesson(1);
}

void CTrainingDlg::OnBnClickedBtnPrev()
{
    EnsureCourseTree();
    MoveToAdjacentLesson(-1);
}

void CTrainingDlg::OnBnClickedBtnComplete()
{
    if (m_nCurrentCourseIndex < 0 || m_nCurrentLessonIndex < 0)
        return;

    m_Manager.MarkLessonCompleted(m_nCurrentCourseIndex, m_nCurrentLessonIndex);

    if (m_Manager.SaveProgress(m_strProgressFile))
    {
        UpdateTreeItemState(m_nCurrentCourseIndex, m_nCurrentLessonIndex);
        AfxMessageBox(L"학습 완료 상태가 저장되었습니다.", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        AfxMessageBox(L"학습 진행 상태 저장에 실패했습니다.", MB_OK | MB_ICONERROR);
    }
}

void CTrainingDlg::OnBnClickedBtnReload()
{
    EnsureCourseTree();
    ReloadCourses();
    RefreshPdfFileList();
    RefreshImageFileList();
    UpdateContentButtons();
}

void CTrainingDlg::CleanupResourcesBeforeExit()
{
    if (m_bExitCleanupDone)
        return;

    m_bExitCleanupDone = TRUE;

    CVideoViewDlg::HideActive();

    if (m_bPdfViewerActive)
        ClosePdfViewer();

    if (::IsWindow(m_pdfCoverView.GetSafeHwnd()))
        m_pdfCoverView.ClearView();

    if (::IsWindow(m_imageView.GetSafeHwnd()))
        m_imageView.ClearView();

    CVideoViewDlg::Shutdown();
    PdfRenderEngine::Shutdown();
}

void CTrainingDlg::DiscardPendingCloseMessages()
{
    MSG msg;
    while (::PeekMessage(&msg, m_hWnd, WM_CLOSE, WM_CLOSE, PM_REMOVE))
    {
    }
}

void CTrainingDlg::OnClose()
{
    if (m_bExitCleanupDone)
    {
        CDialogEx::OnClose();
        return;
    }

    if (m_bExitConfirmShowing)
        return;

    m_bExitConfirmShowing = TRUE;
    const BOOL bConfirmed =
        AfxMessageBox(
            L"프로그램을 종료하시겠습니까?",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
    m_bExitConfirmShowing = FALSE;

    if (!bConfirmed)
    {
        DiscardPendingCloseMessages();
        return;
    }

    CleanupResourcesBeforeExit();
    CDialogEx::OnClose();
}

void CTrainingDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == SC_CLOSE)
    {
        SendMessage(WM_CLOSE);
        return;
    }

    CDialogEx::OnSysCommand(nID, lParam);
}

void CTrainingDlg::OnDestroy()
{
    CleanupResourcesBeforeExit();
    CDialogEx::OnDestroy();
}
