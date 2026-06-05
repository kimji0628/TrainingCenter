#pragma once
#include "TrainingLesson.h"

// ============================================================================
// TrainingCourse.h - Course data class (CStringW / Unicode)
// ============================================================================

class CTrainingCourse
{
public:
    CStringW m_strCourseName;
    CArray<CTrainingLesson, CTrainingLesson&> m_Lessons;

    CTrainingCourse();
    CTrainingCourse(const CTrainingCourse& other);
    CTrainingCourse& operator=(const CTrainingCourse& other);

    // Load course data from UTF-8 JSON file
    BOOL LoadJson(const CStringW& strFilePath);
};
