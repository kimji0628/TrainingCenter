#include "pch.h"
#include "TrainingCourse.h"
#include "Util.h"

// ============================================================================
// TrainingCourse.cpp - Course data class implementation
// ============================================================================

CTrainingCourse::CTrainingCourse()
{
}

CTrainingCourse::CTrainingCourse(const CTrainingCourse& other)
    : m_strCourseName(other.m_strCourseName)
{
    m_Lessons.Copy(other.m_Lessons);
}

CTrainingCourse& CTrainingCourse::operator=(const CTrainingCourse& other)
{
    if (this != &other)
    {
        m_strCourseName = other.m_strCourseName;
        m_Lessons.Copy(other.m_Lessons);
    }
    return *this;
}

BOOL CTrainingCourse::LoadJson(const CStringW& strFilePath)
{
    m_Lessons.RemoveAll();
    m_strCourseName.Empty();

    // Step 1: Read JSON file as raw UTF-8 bytes
    std::string strUtf8Content;
    if (!TrainingUtil::ReadTextFileUtf8(strFilePath, strUtf8Content))
        return FALSE;

    try
    {
        // Step 2: Parse JSON (nlohmann stores strings as UTF-8)
        nlohmann::json jRoot = nlohmann::json::parse(strUtf8Content);

        // Step 3: Convert each UTF-8 string field to CStringW (Unicode)
        m_strCourseName = TrainingUtil::JsonGetStringW(jRoot, "CourseName");
        if (m_strCourseName.IsEmpty())
            return FALSE;

        if (!jRoot.contains("Lessons") || !jRoot["Lessons"].is_array())
            return FALSE;

        for (const auto& jLesson : jRoot["Lessons"])
        {
            CTrainingLesson lesson;

            lesson.m_strTitle       = TrainingUtil::JsonGetStringW(jLesson, "Title");
            lesson.m_strDescription = TrainingUtil::JsonGetStringW(jLesson, "Description");
            lesson.m_strYoutubeUrl  = TrainingUtil::JsonGetStringW(jLesson, "YoutubeUrl");
            lesson.m_strPdfFile     = TrainingUtil::JsonGetStringW(jLesson, "PdfFile");
            lesson.m_strImageFile   = TrainingUtil::JsonGetStringW(jLesson, "ImageFile");

            if (!lesson.m_strTitle.IsEmpty())
            {
                m_Lessons.Add(lesson);
            }
        }

        return m_Lessons.GetSize() > 0;
    }
    catch (const std::exception&)
    {
        return FALSE;
    }
}
