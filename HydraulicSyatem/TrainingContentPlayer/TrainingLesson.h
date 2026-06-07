#pragma once

// ============================================================================
// TrainingLesson.h - Individual lesson data class (CStringW / Unicode)
// ============================================================================

class CTrainingLesson
{
public:
    CStringW m_strTitle;
    CStringW m_strDescription;
    CStringW m_strVideo;
    CStringW m_strPdfFile;
    CStringW m_strImageFile;
    BOOL     m_bCompleted;

    CTrainingLesson();
    CTrainingLesson(const CTrainingLesson& other);
    CTrainingLesson& operator=(const CTrainingLesson& other);
};
