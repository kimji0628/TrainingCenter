#include "pch.h"
#include "TrainingDlg.h"
#include "ImageViewDlg.h"
#include "Util.h"

// Tree 아이템 데이터: 상위 16비트 = 코스 인덱스, 하위 16비트 = 레슨 인덱스 (0xFFFF = 코스 노드)
#define MAKE_TREE_DATA(course, lesson)  ((static_cast<DWORD_PTR>(course) << 16) | (lesson))
#define GET_COURSE_INDEX(data)          (static_cast<int>((data) >> 16))
#define GET_LESSON_INDEX(data)          (static_cast<int>((data) & 0xFFFF))
#define COURSE_NODE_LESSON_INDEX        0xFFFF
#define PDF_TREE_COURSE_MARKER          0xFFFE

#define MAKE_PDF_TREE_DATA(index)       ((static_cast<DWORD_PTR>(PDF_TREE_COURSE_MARKER) << 16) | static_cast<DWORD_PTR>(index))
#define IS_PDF_TREE_ITEM(data)          (GET_COURSE_INDEX(data) == PDF_TREE_COURSE_MARKER)

// ============================================================================
// TrainingDlg.cpp - 메인 교육 콘텐츠 플레이어 다이얼로그 구현
// ============================================================================

CTrainingDlg::CTrainingDlg(CWnd* pParent)
    : CDialogEx(IDD_TRAININGCONTENTPLAYER_DIALOG, pParent)
    , m_nCurrentCourseIndex(-1)
    , m_nCurrentLessonIndex(-1)
    , m_bControlsReady(FALSE)
    , m_bPdfListMode(FALSE)
    , m_bSuppressTreeSelChange(FALSE)
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
    ON_BN_CLICKED(IDC_BTN_NEXT, &CTrainingDlg::OnBnClickedBtnNext)
    ON_BN_CLICKED(IDC_BTN_PREV, &CTrainingDlg::OnBnClickedBtnPrev)
    ON_BN_CLICKED(IDC_BTN_COMPLETE, &CTrainingDlg::OnBnClickedBtnComplete)
    ON_BN_CLICKED(IDC_BTN_RELOAD, &CTrainingDlg::OnBnClickedBtnReload)
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_USER + 100, &CTrainingDlg::OnEnsureVideoPlayer)
END_MESSAGE_MAP()

void CTrainingDlg::SetupKoreanUI()
{
    // Unicode CStringW literals for Korean UI text
    GetDlgItem(IDC_BTN_YOUTUBE)->SetWindowText(L"영상보기");
    GetDlgItem(IDC_BTN_PDF)->SetWindowText(L"PDF보기");
    GetDlgItem(IDC_BTN_IMAGE)->SetWindowText(L"이미지보기");
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

    SetIcon(LoadIcon(nullptr, IDI_APPLICATION), TRUE);
    SetIcon(LoadIcon(nullptr, IDI_APPLICATION), FALSE);

    SetWindowText(L"Training Content Player");
    SetupKoreanUI();

    // Pre-create WebView2 player after the modal message loop starts
    PostMessage(WM_USER + 100, 0, 0);

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

    RefreshPdfFileList();
    UpdateNavigationButtons();

    m_bControlsReady = TRUE;

    CRect rcClient;
    GetClientRect(&rcClient);
    LayoutControls(rcClient.Width(), rcClient.Height());

    return TRUE;
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
        !m_staticThumbnail.GetSafeHwnd())
    {
        return;
    }

    const int nMargin = 10;
    const int nTreeWidth = 250;
    const int nGap = 10;
    const int nContentLeft = nMargin + nTreeWidth + nGap;
    const int nRightMargin = 10;
    const int nContentWidth = max(100, cx - nContentLeft - nRightMargin);
    const int nTitleTop = 10;
    const int nTitleHeight = 24;
    const int nDescTop = nTitleTop + nTitleHeight + 6;
    const int nDescHeight = GetDescriptionMinHeight();
    const int nBtnWidth = 110;
    const int nBtnHeight = 28;
    const int nBtnY = nDescTop + nDescHeight + 8;
    const int nCompleteWidth = 110;
    const int nCompleteHeight = 28;
    const int nThumbTop = nBtnY + nBtnHeight + 10;
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

    const UINT arrButtonIds[] = {
        IDC_BTN_YOUTUBE, IDC_BTN_PDF, IDC_BTN_IMAGE,
        IDC_BTN_RELOAD, IDC_BTN_NEXT, IDC_BTN_PREV
    };

    int nBtnX = nContentLeft;
    for (UINT nButtonId : arrButtonIds)
    {
        CWnd* pButton = GetDlgItem(nButtonId);
        if (pButton != nullptr && pButton->GetSafeHwnd() != nullptr)
        {
            pButton->SetWindowPos(
                nullptr, nBtnX, nBtnY, nBtnWidth, nBtnHeight,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        nBtnX += nBtnWidth + 9;
    }

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

    m_staticThumbnail.SetWindowPos(
        nullptr,
        nContentLeft,
        nThumbTop,
        nContentWidth,
        nThumbBottom - nThumbTop,
        SWP_NOZORDER | SWP_NOACTIVATE);

    CVideoViewDlg::SyncHostArea(&m_staticThumbnail);
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
            CStringW strItemText = lesson.m_strTitle;

            if (lesson.m_bCompleted)
                strItemText = L"[완료] " + strItemText;

            HTREEITEM hLesson = m_treeCourse.InsertItem(strItemText, hCourse, TVI_LAST);
            m_treeCourse.SetItemData(hLesson, MAKE_TREE_DATA(c, l));
        }

        m_treeCourse.Expand(hCourse, TVE_EXPAND);
    }
}

void CTrainingDlg::RefreshPdfFileList()
{
    m_arrPdfFiles.RemoveAll();
    TrainingUtil::FindPdfFiles(TrainingUtil::GetPdfFolder(), m_arrPdfFiles);
}

void CTrainingDlg::BuildPdfTree()
{
    m_treeCourse.DeleteAllItems();

    HTREEITEM hRoot = m_treeCourse.InsertItem(L"PDF 파일 목록", TVI_ROOT, TVI_LAST);
    m_treeCourse.SetItemData(hRoot, MAKE_TREE_DATA(0, COURSE_NODE_LESSON_INDEX));

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
        CStringW strPath = m_arrPdfFiles[i];
        int nPos = strPath.ReverseFind(L'\\');
        CStringW strName = (nPos >= 0) ? strPath.Mid(nPos + 1) : strPath;

        HTREEITEM hItem = m_treeCourse.InsertItem(strName, hRoot, TVI_LAST);
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

void CTrainingDlg::ShowCourseTree()
{
    if (!m_bPdfListMode)
        return;

    m_bPdfListMode = FALSE;
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
    if (m_bPdfListMode)
        ShowCourseTree();
}

void CTrainingDlg::ShowPdfTree()
{
    CVideoViewDlg::HideActive();
    RefreshPdfFileList();

    if (m_arrPdfFiles.GetSize() == 0)
    {
        AfxMessageBox(
            L"Pdf 폴더에 PDF 파일이 없습니다.\n"
            L"Bin\\Pdf 폴더에 PDF 파일을 추가하세요.",
            MB_OK | MB_ICONWARNING);
        return;
    }

    m_bPdfListMode = TRUE;
    BuildPdfTree();

    m_staticTitle.SetWindowText(L"PDF 보기");
    m_editDescription.SetWindowText(
        L"좌측 목록에서 열 PDF 파일을 선택하세요.\n\n"
        L"Pdf 폴더에 저장된 모든 PDF 파일이 표시됩니다.");
}

void CTrainingDlg::OpenPdfByIndex(int nPdfIndex)
{
    if (nPdfIndex < 0 || nPdfIndex >= m_arrPdfFiles.GetSize())
        return;

    CStringW strFullPath = m_arrPdfFiles[nPdfIndex];
    if (GetFileAttributesW(strFullPath) == INVALID_FILE_ATTRIBUTES)
    {
        AfxMessageBox(L"파일을 찾을 수 없습니다.\n" + strFullPath,
            MB_OK | MB_ICONWARNING);
        return;
    }

    HINSTANCE hResult = ShellExecuteW(
        m_hWnd, L"open", strFullPath, nullptr, nullptr, SW_SHOWNORMAL);

    if (reinterpret_cast<INT_PTR>(hResult) <= 32)
        AfxMessageBox(L"PDF 파일을 열 수 없습니다.", MB_OK | MB_ICONERROR);
}

void CTrainingDlg::UpdateContentButtons()
{
    const CTrainingLesson* pLesson = m_Manager.GetLesson(
        m_nCurrentCourseIndex, m_nCurrentLessonIndex);

    BOOL bHasLesson = (pLesson != nullptr);
    GetDlgItem(IDC_BTN_YOUTUBE)->EnableWindow(
        bHasLesson && !pLesson->m_strYoutubeUrl.IsEmpty());
    GetDlgItem(IDC_BTN_PDF)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_IMAGE)->EnableWindow(
        bHasLesson && !pLesson->m_strImageFile.IsEmpty());
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
    {
        CStringW strText = pLesson->m_strTitle;
        if (pLesson->m_bCompleted)
            strText = L"[완료] " + strText;
        m_treeCourse.SetItemText(hLesson, strText);
    }
}

void CTrainingDlg::DisplayLesson(int nCourseIndex, int nLessonIndex)
{
    CVideoViewDlg::HideActive();

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

    CRect rc;
    m_staticThumbnail.GetClientRect(&rc);
    CDC* pDC = m_staticThumbnail.GetDC();
    if (pDC != nullptr)
    {
        pDC->FillSolidRect(&rc, GetSysColor(COLOR_3DFACE));
        m_staticThumbnail.ReleaseDC(pDC);
    }

    GetDlgItem(IDC_BTN_YOUTUBE)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_PDF)->EnableWindow(m_arrPdfFiles.GetSize() > 0);
    GetDlgItem(IDC_BTN_IMAGE)->EnableWindow(FALSE);
}

void CTrainingDlg::LoadThumbnail(const CStringW& strImagePath)
{
    CRect rc;
    m_staticThumbnail.GetClientRect(&rc);
    CDC* pDC = m_staticThumbnail.GetDC();
    if (pDC == nullptr)
        return;

    pDC->FillSolidRect(&rc, GetSysColor(COLOR_3DFACE));

    if (!strImagePath.IsEmpty())
    {
        CStringW strFullPath = TrainingUtil::ResolveAppPath(strImagePath);
        CImage image;
        if (SUCCEEDED(image.Load(strFullPath)))
        {
            int nImgW = image.GetWidth();
            int nImgH = image.GetHeight();
            int nDstW = rc.Width();
            int nDstH = rc.Height();

            // 비율 유지하며 썸네일 영역에 맞춤
            double dScale = min(
                static_cast<double>(nDstW) / nImgW,
                static_cast<double>(nDstH) / nImgH);
            int nDrawW = static_cast<int>(nImgW * dScale);
            int nDrawH = static_cast<int>(nImgH * dScale);
            int nOffsetX = (nDstW - nDrawW) / 2;
            int nOffsetY = (nDstH - nDrawH) / 2;

            image.StretchBlt(pDC->m_hDC, nOffsetX, nOffsetY, nDrawW, nDrawH,
                0, 0, nImgW, nImgH, SRCCOPY);
        }
    }

    m_staticThumbnail.ReleaseDC(pDC);
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

    CVideoViewDlg::PlayVideo(this, strUrl, &m_staticThumbnail);
}

LRESULT CTrainingDlg::OnEnsureVideoPlayer(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    CVideoViewDlg::EnsureCreated(this, &m_staticThumbnail);
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

        if (m_bPdfListMode)
        {
            if (IS_PDF_TREE_ITEM(dwData))
                OpenPdfByIndex(GET_LESSON_INDEX(dwData));
        }
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

    const CTrainingLesson* pLesson = m_Manager.GetLesson(
        m_nCurrentCourseIndex, m_nCurrentLessonIndex);
    if (pLesson != nullptr)
        LaunchUrl(pLesson->m_strYoutubeUrl);
}

void CTrainingDlg::OnBnClickedBtnPdf()
{
    ShowPdfTree();
}

void CTrainingDlg::OnBnClickedBtnImage()
{
    EnsureCourseTree();
    CVideoViewDlg::HideActive();

    const CTrainingLesson* pLesson = m_Manager.GetLesson(
        m_nCurrentCourseIndex, m_nCurrentLessonIndex);
    if (pLesson != nullptr && !pLesson->m_strImageFile.IsEmpty())
    {
        CImageViewDlg dlg(pLesson->m_strImageFile, this);
        dlg.DoModal();
    }
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
    UpdateContentButtons();
}

void CTrainingDlg::OnClose()
{
    CVideoViewDlg::Shutdown();
    CDialogEx::OnClose();
}

void CTrainingDlg::OnDestroy()
{
    CVideoViewDlg::Shutdown();
    CDialogEx::OnDestroy();
}
