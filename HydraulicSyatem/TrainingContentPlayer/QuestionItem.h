#pragma once

#include <vector>

// ============================================================================
// QuestionItem.h - AI 문제 생성 / 기능 시험 데이터 구조
// ============================================================================

struct QUESTION_ITEM
{
    CStringW strId;
    int      nNo;
    CStringW strCategory;
    int      nSourcePage;
    CStringW strCreated;
    CStringW strQuestion;
    CStringW strChoice1;
    CStringW strChoice2;
    CStringW strChoice3;
    CStringW strChoice4;
    CStringW strAnswer;
    CStringW strExplain;
    BOOL     bUseFlag;

    QUESTION_ITEM()
        : nNo(0)
        , nSourcePage(0)
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

CStringW FormatQuestionDisplayText(const QUESTION_ITEM& item);
CStringW FormatAnswerChoiceLabel(const CStringW& strAnswerLetter);
