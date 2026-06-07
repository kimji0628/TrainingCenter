#pragma once

#include <vector>

// ============================================================================
// QuestionItem.h - AI 문제 생성 데이터 구조 (1단계: 구조만 준비)
// ============================================================================

struct QUESTION_ITEM
{
    int      nNo;
    CStringW strQuestion;
    CStringW strChoice1;
    CStringW strChoice2;
    CStringW strChoice3;
    CStringW strChoice4;
    CStringW strAnswer;
    BOOL     bUseFlag;

    QUESTION_ITEM()
        : nNo(0)
        , bUseFlag(FALSE)
    {
    }
};

using CQuestionItemArray = std::vector<QUESTION_ITEM>;

struct QUIZ_GEN_SETTINGS
{
    CStringW strPdfPath;
    BOOL     bAllPages;
    int      nPageStart;
    int      nPageEnd;
    int      nQuestionCount;

    QUIZ_GEN_SETTINGS()
        : bAllPages(TRUE)
        , nPageStart(1)
        , nPageEnd(1)
        , nQuestionCount(5)
    {
    }
};
