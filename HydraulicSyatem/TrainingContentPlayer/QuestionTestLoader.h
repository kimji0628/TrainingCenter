#pragma once

#include "QuestionItem.h"

namespace QuestionTestLoader
{
    CStringW GetTestQuestionFilePath();
    BOOL ParseFromContent(const CStringW& strContent, CQuestionItemArray& outQuestions, CStringW& strError);
    BOOL LoadFromFile(const CStringW& strFilePath, CQuestionItemArray& outQuestions, CStringW& strError);
}
