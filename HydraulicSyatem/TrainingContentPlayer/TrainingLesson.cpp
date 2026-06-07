#include "pch.h"
#include "TrainingLesson.h"

// ============================================================================
// TrainingLesson.cpp - 개별 학습 Lesson 데이터 클래스 구현
// ============================================================================

CTrainingLesson::CTrainingLesson()
    : m_bCompleted(FALSE)
{
}

CTrainingLesson::CTrainingLesson(const CTrainingLesson& other)
    : m_strTitle(other.m_strTitle)
    , m_strDescription(other.m_strDescription)
    , m_strVideo(other.m_strVideo)
    , m_strPdfFile(other.m_strPdfFile)
    , m_strImageFile(other.m_strImageFile)
    , m_bCompleted(other.m_bCompleted)
{
}

CTrainingLesson& CTrainingLesson::operator=(const CTrainingLesson& other)
{
    if (this != &other)
    {
        m_strTitle = other.m_strTitle;
        m_strDescription = other.m_strDescription;
        m_strVideo = other.m_strVideo;
        m_strPdfFile = other.m_strPdfFile;
        m_strImageFile = other.m_strImageFile;
        m_bCompleted = other.m_bCompleted;
    }
    return *this;
}
