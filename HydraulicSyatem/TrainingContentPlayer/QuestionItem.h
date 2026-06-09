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
    CStringW strSourcePdfPath;
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

struct QUESTION_DISPLAY_META
{
    CStringW strDifficulty;
    CStringW strQuestionType;
    BOOL     bAiGenerated;
    BOOL     bProfessorModified;
    BOOL     bModified;
    int      nUsageCount;

    QUESTION_DISPLAY_META()
        : bAiGenerated(TRUE)
        , bProfessorModified(FALSE)
        , bModified(FALSE)
        , nUsageCount(0)
    {
    }
};

struct QUESTION_LIST_LABEL_OPTIONS
{
    BOOL     bShowIndex;
    BOOL     bShowSourcePage;
    int      nMaxTextLength;
    CStringW strAdoptedMark;
    CStringW strModifiedMark;
    CStringW strAnswerMark;
    CStringW strDifficultyMark;
    CStringW strTypeMark;

    QUESTION_LIST_LABEL_OPTIONS()
        : bShowIndex(TRUE)
        , bShowSourcePage(TRUE)
        , nMaxTextLength(80)
    {
    }
};

struct QUESTION_BANK_ENTRY_OPTIONS
{
    BOOL bShowChoices;
    BOOL bShowSourcePage;
    BOOL bShowAnswer;
    BOOL bShowExplain;
    int  nMaxQuestionLength;

    QUESTION_BANK_ENTRY_OPTIONS()
        : bShowChoices(TRUE)
        , bShowSourcePage(TRUE)
        , bShowAnswer(FALSE)
        , bShowExplain(FALSE)
        , nMaxQuestionLength(0)
    {
    }
};

CStringW FormatQuestionDisplayText(const QUESTION_ITEM& item);
CStringW FormatQuestionListLabel(
    const QUESTION_ITEM& item,
    int nDisplayIndex,
    const QUESTION_LIST_LABEL_OPTIONS* pOptions = nullptr);
CStringW FormatQuestionBankEntry(
    const QUESTION_ITEM& item,
    int nDisplayIndex,
    const QUESTION_BANK_ENTRY_OPTIONS* pOptions = nullptr);
CStringW FormatAnswerChoiceLabel(const CStringW& strAnswerLetter);
BOOL IsQuestionInBank(const QUESTION_ITEM& item, const CQuestionItemArray& bank);
