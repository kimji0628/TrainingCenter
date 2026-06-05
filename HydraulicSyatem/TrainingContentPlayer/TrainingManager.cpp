#include "pch.h"
#include "TrainingManager.h"
#include "Util.h"
#include <fstream>

// ============================================================================
// TrainingManager.cpp - Course loading and progress management
// ============================================================================

CTrainingManager::CTrainingManager()
{
}

BOOL CTrainingManager::LoadAllCourses(const CStringW& strDataFolder)
{
    m_Courses.RemoveAll();

    CStringArray arrJsonFiles;
    TrainingUtil::FindJsonFiles(strDataFolder, arrJsonFiles);

    if (arrJsonFiles.GetSize() == 0)
        return FALSE;

    for (int i = 0; i < arrJsonFiles.GetSize(); ++i)
    {
        CTrainingCourse course;
        if (course.LoadJson(arrJsonFiles[i]))
        {
            m_Courses.Add(course);
        }
    }

    return m_Courses.GetSize() > 0;
}

BOOL CTrainingManager::LoadProgress(const CStringW& strProgressFile)
{
    for (int c = 0; c < m_Courses.GetSize(); ++c)
    {
        for (int l = 0; l < m_Courses[c].m_Lessons.GetSize(); ++l)
        {
            m_Courses[c].m_Lessons[l].m_bCompleted = FALSE;
        }
    }

    std::string strUtf8Content;
    if (!TrainingUtil::ReadTextFileUtf8(strProgressFile, strUtf8Content))
        return FALSE;

    if (strUtf8Content.empty())
        return TRUE;

    try
    {
        nlohmann::json jRoot = nlohmann::json::parse(strUtf8Content);

        if (!jRoot.contains("CompletedLessons") || !jRoot["CompletedLessons"].is_array())
            return TRUE;

        for (const auto& jItem : jRoot["CompletedLessons"])
        {
            CStringW strCourseName = TrainingUtil::JsonGetStringW(jItem, "CourseName");
            CStringW strLessonTitle = TrainingUtil::JsonGetStringW(jItem, "LessonTitle");

            if (strCourseName.IsEmpty() || strLessonTitle.IsEmpty())
                continue;

            for (int c = 0; c < m_Courses.GetSize(); ++c)
            {
                if (m_Courses[c].m_strCourseName != strCourseName)
                    continue;

                for (int l = 0; l < m_Courses[c].m_Lessons.GetSize(); ++l)
                {
                    if (m_Courses[c].m_Lessons[l].m_strTitle == strLessonTitle)
                    {
                        m_Courses[c].m_Lessons[l].m_bCompleted = TRUE;
                        break;
                    }
                }
                break;
            }
        }

        return TRUE;
    }
    catch (const std::exception&)
    {
        return FALSE;
    }
}

BOOL CTrainingManager::SaveProgress(const CStringW& strProgressFile) const
{
    try
    {
        nlohmann::json jRoot;
        nlohmann::json jCompleted = nlohmann::json::array();

        for (int c = 0; c < m_Courses.GetSize(); ++c)
        {
            const CTrainingCourse& course = m_Courses[c];
            for (int l = 0; l < course.m_Lessons.GetSize(); ++l)
            {
                const CTrainingLesson& lesson = course.m_Lessons[l];
                if (lesson.m_bCompleted)
                {
                    nlohmann::json jItem;
                    jItem["CourseName"]  = TrainingUtil::CStringWToUtf8(course.m_strCourseName);
                    jItem["LessonTitle"] = TrainingUtil::CStringWToUtf8(lesson.m_strTitle);
                    jCompleted.push_back(jItem);
                }
            }
        }

        jRoot["CompletedLessons"] = jCompleted;

        CStringW strFolder = strProgressFile;
        int nPos = strFolder.ReverseFind(L'\\');
        if (nPos >= 0)
        {
            strFolder = strFolder.Left(nPos);
            CreateDirectoryW(strFolder, nullptr);
        }

        std::string strUtf8Json = jRoot.dump(2);

        CFile file;
        if (!file.Open(strProgressFile, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
            return FALSE;

        file.Write(strUtf8Json.data(), static_cast<UINT>(strUtf8Json.size()));
        file.Close();
        return TRUE;
    }
    catch (const std::exception&)
    {
        return FALSE;
    }
}

void CTrainingManager::MarkLessonCompleted(int nCourseIndex, int nLessonIndex)
{
    CTrainingLesson* pLesson = GetLesson(nCourseIndex, nLessonIndex);
    if (pLesson != nullptr)
    {
        pLesson->m_bCompleted = TRUE;
    }
}

CTrainingLesson* CTrainingManager::GetLesson(int nCourseIndex, int nLessonIndex)
{
    if (nCourseIndex < 0 || nCourseIndex >= m_Courses.GetSize())
        return nullptr;

    CTrainingCourse& course = m_Courses[nCourseIndex];
    if (nLessonIndex < 0 || nLessonIndex >= course.m_Lessons.GetSize())
        return nullptr;

    return &course.m_Lessons[nLessonIndex];
}

const CTrainingLesson* CTrainingManager::GetLesson(int nCourseIndex, int nLessonIndex) const
{
    if (nCourseIndex < 0 || nCourseIndex >= m_Courses.GetSize())
        return nullptr;

    const CTrainingCourse& course = m_Courses[nCourseIndex];
    if (nLessonIndex < 0 || nLessonIndex >= course.m_Lessons.GetSize())
        return nullptr;

    return &course.m_Lessons[nLessonIndex];
}
