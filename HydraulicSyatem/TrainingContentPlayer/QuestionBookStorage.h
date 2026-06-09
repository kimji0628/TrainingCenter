#pragma once

#include "QuestionItem.h"
#include <vector>

struct QUESTION_BOOK_INFO
{
    CStringW strName;
    CStringW strFilePath;
    CStringW strSavedAt;
    int      nQuestionCount;
    int      nFormatVersion;

    QUESTION_BOOK_INFO()
        : nQuestionCount(0)
        , nFormatVersion(1)
    {
    }
};

using CQuestionBookInfoArray = std::vector<QUESTION_BOOK_INFO>;

namespace QuestionBookStorage
{
    constexpr int kFormatVersion = 1;
    constexpr wchar_t kFileExtension[] = L".scpbook";

    CStringW GetQuestionBooksFolder();
    CStringW SanitizeBookName(const CStringW& strRawName);
    CStringW BuildBookFilePath(const CStringW& strBookName);

    BOOL ListQuestionBooks(CQuestionBookInfoArray& outBooks, CStringW& strError);
    BOOL SaveQuestionBook(
        const CStringW& strBookName,
        const CQuestionItemArray& questions,
        CStringW& strError);
    BOOL LoadQuestionBook(
        const CStringW& strFilePath,
        CQuestionItemArray& outQuestions,
        QUESTION_BOOK_INFO& outInfo,
        CStringW& strError);
    BOOL DeleteQuestionBook(const CStringW& strFilePath, CStringW& strError);
    BOOL BookFileExists(const CStringW& strBookName);
}
