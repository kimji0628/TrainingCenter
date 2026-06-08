#pragma once

#include "QuestionItem.h"

namespace QuestionTestLoader
{
    CStringW GetTestQuestionFilePath();
    BOOL LoadFromFile(const CStringW& strFilePath, CQuestionItemArray& outQuestions, CStringW& strError);
}
