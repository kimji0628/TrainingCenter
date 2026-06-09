#include "pch.h"
#include "QuestionTestLoader.h"
#include "Util.h"

namespace
{
    CStringW TrimLine(const CStringW& strLine)
    {
        CStringW str = strLine;
        str.Trim();
        return str;
    }

    CStringW UnescapeFieldValue(const CStringW& strValue)
    {
        CStringW str = strValue;
        str.Replace(L"\\n", L"\r\n");
        return str;
    }

    void ApplyField(QUESTION_ITEM& item, const CStringW& strKey, const CStringW& strValue)
    {
        const CStringW strDecoded = UnescapeFieldValue(strValue);

        if (strKey.CompareNoCase(L"ID") == 0)
            item.strId = strDecoded;
        else if (strKey.CompareNoCase(L"NO") == 0)
            item.nNo = _wtoi(strDecoded);
        else if (strKey.CompareNoCase(L"CATEGORY") == 0)
            item.strCategory = strDecoded;
        else if (strKey.CompareNoCase(L"SOURCE_PAGE") == 0)
            item.nSourcePage = _wtoi(strDecoded);
        else if (strKey.CompareNoCase(L"SOURCE_PDF") == 0 ||
                 strKey.CompareNoCase(L"SOURCEPDFPATH") == 0)
            item.strSourcePdfPath = strDecoded;
        else if (strKey.CompareNoCase(L"CREATED") == 0)
            item.strCreated = strDecoded;
        else if (strKey.CompareNoCase(L"QUESTION") == 0)
            item.strQuestion = strDecoded;
        else if (strKey.CompareNoCase(L"A") == 0 || strKey.CompareNoCase(L"CHOICE1") == 0)
            item.strChoice1 = strDecoded;
        else if (strKey.CompareNoCase(L"B") == 0 || strKey.CompareNoCase(L"CHOICE2") == 0)
            item.strChoice2 = strDecoded;
        else if (strKey.CompareNoCase(L"C") == 0 || strKey.CompareNoCase(L"CHOICE3") == 0)
            item.strChoice3 = strDecoded;
        else if (strKey.CompareNoCase(L"D") == 0 || strKey.CompareNoCase(L"CHOICE4") == 0)
            item.strChoice4 = strDecoded;
        else if (strKey.CompareNoCase(L"ANSWER") == 0)
            item.strAnswer = strDecoded;
        else if (strKey.CompareNoCase(L"EXPLAIN") == 0 || strKey.CompareNoCase(L"EXPLANATION") == 0)
            item.strExplain = strDecoded;
    }

    void FinalizeQuestionItem(QUESTION_ITEM& item)
    {
        if (item.strId.IsEmpty())
        {
            if (item.nNo > 0)
                item.strId.Format(L"Q%04d", item.nNo);
            else
                item.strId = L"Q0000";
        }
    }

    BOOL ParseQuestionBlock(const CStringW& strBlock, QUESTION_ITEM& outItem, CStringW& strError)
    {
        outItem = QUESTION_ITEM();

        int nPos = 0;
        CStringW strLine = strBlock.Tokenize(L"\r\n", nPos);
        while (!strLine.IsEmpty() || nPos >= 0)
        {
            strLine = TrimLine(strLine);
            if (!strLine.IsEmpty() &&
                strLine.CompareNoCase(L"QSTART") != 0 &&
                strLine.CompareNoCase(L"QEND") != 0)
            {
                const int nEq = strLine.Find(L'=');
                if (nEq > 0)
                {
                    const CStringW strKey = TrimLine(strLine.Left(nEq));
                    const CStringW strValue = strLine.Mid(nEq + 1);
                    ApplyField(outItem, strKey, strValue);
                }
            }

            if (nPos < 0)
                break;
            strLine = strBlock.Tokenize(L"\r\n", nPos);
        }

        if (outItem.strQuestion.IsEmpty())
        {
            strError = L"QUESTION 필드가 비어 있는 항목이 있습니다.";
            return FALSE;
        }

        FinalizeQuestionItem(outItem);
        return TRUE;
    }
}

CStringW QuestionTestLoader::GetTestQuestionFilePath()
{
    return TrainingUtil::ResolveAppPath(L"Question\\QuestionListForTest.txt");
}

BOOL QuestionTestLoader::ParseFromContent(
    const CStringW& strContent,
    CQuestionItemArray& outQuestions,
    CStringW& strError)
{
    outQuestions.clear();
    strError.Empty();

    if (strContent.IsEmpty())
    {
        strError = L"응답 내용이 비어 있습니다.";
        return FALSE;
    }

    CStringW strNormalized = strContent;
    strNormalized.Replace(L"\r\n", L"\n");
    strNormalized.Replace(L"\r", L"\n");

    const CStringW strMarkerStart = L"QSTART";
    const CStringW strMarkerEnd = L"QEND";
    int nSearch = 0;

    while (nSearch < strNormalized.GetLength())
    {
        const int nStart = strNormalized.Find(strMarkerStart, nSearch);
        if (nStart < 0)
            break;

        const int nEnd = strNormalized.Find(strMarkerEnd, nStart + strMarkerStart.GetLength());
        if (nEnd < 0)
        {
            strError = L"QEND가 없는 문제 블록이 있습니다.";
            return FALSE;
        }

        CStringW strBlock = strNormalized.Mid(
            nStart + strMarkerStart.GetLength(),
            nEnd - (nStart + strMarkerStart.GetLength()));

        QUESTION_ITEM item;
        if (!ParseQuestionBlock(strBlock, item, strError))
            return FALSE;

        outQuestions.push_back(item);
        nSearch = nEnd + strMarkerEnd.GetLength();
    }

    if (outQuestions.empty())
    {
        strError = L"응답에서 유효한 QSTART/QEND 문제 블록을 찾을 수 없습니다.";
        return FALSE;
    }

    return TRUE;
}

BOOL QuestionTestLoader::LoadFromFile(
    const CStringW& strFilePath,
    CQuestionItemArray& outQuestions,
    CStringW& strError)
{
    outQuestions.clear();
    strError.Empty();

    if (strFilePath.IsEmpty())
    {
        strError = L"QuestionListForTest.txt 파일을 찾을 수 없습니다.";
        return FALSE;
    }

    DWORD dwAttr = GetFileAttributesW(strFilePath);
    if (dwAttr == INVALID_FILE_ATTRIBUTES || (dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        strError = L"QuestionListForTest.txt 파일을 찾을 수 없습니다.";
        return FALSE;
    }

    std::string strUtf8;
    if (!TrainingUtil::ReadTextFileUtf8(strFilePath, strUtf8))
    {
        strError = L"QuestionListForTest.txt 파일을 읽을 수 없습니다.";
        return FALSE;
    }

    const CStringW strContent = TrainingUtil::Utf8ToCStringW(strUtf8);
    if (!ParseFromContent(strContent, outQuestions, strError))
    {
        if (strError.Find(L"QSTART/QEND") >= 0)
            strError = L"QuestionListForTest.txt에 유효한 문제가 없습니다.";
        return FALSE;
    }

    return TRUE;
}

CStringW FormatAnswerChoiceLabel(const CStringW& strAnswerLetter)
{
    CStringW strKey = strAnswerLetter;
    strKey.Trim();
    strKey.MakeUpper();

    if (strKey == L"A") return L"①";
    if (strKey == L"B") return L"②";
    if (strKey == L"C") return L"③";
    if (strKey == L"D") return L"④";
    return strAnswerLetter;
}

CStringW FormatQuestionListLabel(
    const QUESTION_ITEM& item,
    int nDisplayIndex,
    const QUESTION_LIST_LABEL_OPTIONS* pOptions)
{
    QUESTION_LIST_LABEL_OPTIONS defaultOptions;
    const QUESTION_LIST_LABEL_OPTIONS& opts = (pOptions != nullptr) ? *pOptions : defaultOptions;

    CStringW strShort = item.strQuestion;
    strShort.Trim();

    const int nMaxLen = max(16, opts.nMaxTextLength);
    if (strShort.GetLength() > nMaxLen)
        strShort = strShort.Left(nMaxLen - 3) + L"...";

    CStringW strPrefix = opts.strAdoptedMark + opts.strModifiedMark + opts.strAnswerMark;
    if (opts.bShowIndex && nDisplayIndex > 0)
    {
        CStringW strIndex;
        strIndex.Format(L"%d. ", nDisplayIndex);
        strPrefix += strIndex;
    }

    CStringW strLabel;
    if (opts.bShowSourcePage && item.nSourcePage > 0)
        strLabel.Format(L"%s%s (P.%d)", strPrefix.GetString(), strShort.GetString(), item.nSourcePage);
    else
        strLabel.Format(L"%s%s", strPrefix.GetString(), strShort.GetString());

    return strLabel;
}

BOOL IsQuestionInBank(const QUESTION_ITEM& item, const CQuestionItemArray& bank)
{
    for (const QUESTION_ITEM& bankItem : bank)
    {
        if (bankItem.strId.CompareNoCase(item.strId) == 0)
            return TRUE;
    }
    return FALSE;
}

CStringW FormatQuestionBankEntry(
    const QUESTION_ITEM& item,
    int nDisplayIndex,
    const QUESTION_BANK_ENTRY_OPTIONS* pOptions)
{
    QUESTION_BANK_ENTRY_OPTIONS defaultOptions;
    const QUESTION_BANK_ENTRY_OPTIONS& opts = (pOptions != nullptr) ? *pOptions : defaultOptions;

    CStringW strQuestion = item.strQuestion;
    strQuestion.Trim();
    if (opts.nMaxQuestionLength > 0 && strQuestion.GetLength() > opts.nMaxQuestionLength)
        strQuestion = strQuestion.Left(opts.nMaxQuestionLength - 3) + L"...";

    CStringW strEntry;
    if (opts.bShowSourcePage && item.nSourcePage > 0)
    {
        strEntry.Format(
            L"%d. %s (P.%d)",
            nDisplayIndex,
            strQuestion.GetString(),
            item.nSourcePage);
    }
    else
    {
        strEntry.Format(L"%d. %s", nDisplayIndex, strQuestion.GetString());
    }

    if (!opts.bShowChoices)
        return strEntry;

    CStringW strChoices;
    if (!item.strChoice1.IsEmpty())
        strChoices += L"\r\n① " + item.strChoice1;
    else
        strChoices += L"\r\n①";

    if (!item.strChoice2.IsEmpty())
        strChoices += L"\r\n② " + item.strChoice2;
    else
        strChoices += L"\r\n②";

    if (!item.strChoice3.IsEmpty())
        strChoices += L"\r\n③ " + item.strChoice3;
    else
        strChoices += L"\r\n③";

    if (!item.strChoice4.IsEmpty())
        strChoices += L"\r\n④ " + item.strChoice4;
    else
        strChoices += L"\r\n④";

    strEntry += strChoices;

    if (opts.bShowAnswer && !item.strAnswer.IsEmpty())
    {
        CStringW strAnswer;
        strAnswer.Format(
            L"\r\n정답 : %s",
            FormatAnswerChoiceLabel(item.strAnswer).GetString());
        strEntry += strAnswer;
    }

    if (opts.bShowExplain && !item.strExplain.IsEmpty())
        strEntry += L"\r\n해설 : " + item.strExplain;

    return strEntry;
}

CStringW FormatQuestionDisplayText(const QUESTION_ITEM& item)
{
    CStringW strText;
    const int nDisplayNo = (item.nNo > 0) ? item.nNo : 1;

    if (item.nSourcePage > 0)
    {
        strText.Format(
            L"문제 %d. (출처 PDF p.%d)\r\n\r\n%s\r\n\r\n",
            nDisplayNo,
            item.nSourcePage,
            item.strQuestion.GetString());
    }
    else
    {
        strText.Format(L"문제 %d.\r\n\r\n%s\r\n\r\n", nDisplayNo, item.strQuestion.GetString());
    }

    if (!item.strChoice1.IsEmpty())
        strText += L"① " + item.strChoice1 + L"\r\n";
    else
        strText += L"①\r\n";

    if (!item.strChoice2.IsEmpty())
        strText += L"② " + item.strChoice2 + L"\r\n";
    else
        strText += L"②\r\n";

    if (!item.strChoice3.IsEmpty())
        strText += L"③ " + item.strChoice3 + L"\r\n";
    else
        strText += L"③\r\n";

    if (!item.strChoice4.IsEmpty())
        strText += L"④ " + item.strChoice4 + L"\r\n\r\n";
    else
        strText += L"④\r\n\r\n";

    if (!item.strAnswer.IsEmpty())
    {
        CStringW strAnswerLine;
        strAnswerLine.Format(
            L"정답 : %s",
            FormatAnswerChoiceLabel(item.strAnswer).GetString());
        strText += strAnswerLine;
    }

    if (!item.strExplain.IsEmpty())
        strText += L"\r\n\r\n해설 : " + item.strExplain;

    return strText;
}
