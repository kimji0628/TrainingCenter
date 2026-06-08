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

    void ApplyField(QUESTION_ITEM& item, const CStringW& strKey, const CStringW& strValue)
    {
        if (strKey.CompareNoCase(L"ID") == 0)
            item.strId = strValue;
        else if (strKey.CompareNoCase(L"NO") == 0)
            item.nNo = _wtoi(strValue);
        else if (strKey.CompareNoCase(L"CATEGORY") == 0)
            item.strCategory = strValue;
        else if (strKey.CompareNoCase(L"SOURCE_PAGE") == 0)
            item.nSourcePage = _wtoi(strValue);
        else if (strKey.CompareNoCase(L"CREATED") == 0)
            item.strCreated = strValue;
        else if (strKey.CompareNoCase(L"QUESTION") == 0)
            item.strQuestion = strValue;
        else if (strKey.CompareNoCase(L"A") == 0)
            item.strChoice1 = strValue;
        else if (strKey.CompareNoCase(L"B") == 0)
            item.strChoice2 = strValue;
        else if (strKey.CompareNoCase(L"C") == 0)
            item.strChoice3 = strValue;
        else if (strKey.CompareNoCase(L"D") == 0)
            item.strChoice4 = strValue;
        else if (strKey.CompareNoCase(L"ANSWER") == 0)
            item.strAnswer = strValue;
        else if (strKey.CompareNoCase(L"EXPLAIN") == 0)
            item.strExplain = strValue;
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

    CStringW strContent = TrainingUtil::Utf8ToCStringW(strUtf8);
    strContent.Replace(L"\r\n", L"\n");
    strContent.Replace(L"\r", L"\n");

    const CStringW strMarkerStart = L"QSTART";
    const CStringW strMarkerEnd = L"QEND";
    int nSearch = 0;

    while (nSearch < strContent.GetLength())
    {
        const int nStart = strContent.Find(strMarkerStart, nSearch);
        if (nStart < 0)
            break;

        const int nEnd = strContent.Find(strMarkerEnd, nStart + strMarkerStart.GetLength());
        if (nEnd < 0)
        {
            strError = L"QEND가 없는 문제 블록이 있습니다.";
            return FALSE;
        }

        CStringW strBlock = strContent.Mid(
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

CStringW FormatQuestionDisplayText(const QUESTION_ITEM& item)
{
    CStringW strText;
    const int nDisplayNo = (item.nNo > 0) ? item.nNo : 1;

    strText.Format(L"문제 %d.\r\n\r\n%s\r\n\r\n", nDisplayNo, item.strQuestion);

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
