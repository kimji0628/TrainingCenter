#pragma once
#include "TrainingCourse.h"

// ============================================================================
// TrainingManager.h - Course loading and progress management (CStringW)
// ============================================================================

class CTrainingManager
{
public:
    CArray<CTrainingCourse, CTrainingCourse&> m_Courses;

    CTrainingManager();

    BOOL LoadAllCourses(const CStringW& strDataFolder);
    BOOL LoadProgress(const CStringW& strProgressFile);
    BOOL SaveProgress(const CStringW& strProgressFile) const;

    void MarkLessonCompleted(int nCourseIndex, int nLessonIndex);

    CTrainingLesson* GetLesson(int nCourseIndex, int nLessonIndex);
    const CTrainingLesson* GetLesson(int nCourseIndex, int nLessonIndex) const;
};
