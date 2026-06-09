#pragma once

namespace PromptLoader
{
    CStringW GetQuestionPromptFilePath();
    BOOL LoadPromptFile(const CStringW& strFilePath, CStringW& outContent, CStringW& strError);
    CStringW BuildQuestionGenerationPrompt(
        const CStringW& strTemplate,
        const CStringW& strPdfText,
        int nQuestionCount,
        int nPageStart,
        int nPageEnd,
        const CStringW& strPdfFileName);
}
