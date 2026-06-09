#include "pch.h"
#include "QuestionBookStorage.h"
#include "QuestionTestLoader.h"
#include "ScpPaths.h"
#include "OpenAiClient.h"
#include "Util.h"

#include <fstream>
#include <algorithm>

namespace
{
    CStringW TrimValue(const CStringW& strValue)
    {
        CStringW str = strValue;
        str.Trim();
        return str;
    }

    CStringW EscapeFieldValue(const CStringW& strValue)
    {
        CStringW str = strValue;
        str.Replace(L"\r\n", L"\\n");
        str.Replace(L"\n", L"\\n");
        str.Replace(L"\r", L"\\n");
        return str;
    }

    CStringW FormatQuestionBlock(const QUESTION_ITEM& item, int nDisplayNo)
    {
        CStringW strBlock;
        const int nNo = (item.nNo > 0) ? item.nNo : nDisplayNo;

        strBlock.Format(L"QSTART\r\nNO=%d\r\n", nNo);

        if (!item.strId.IsEmpty())
            strBlock += L"ID=" + item.strId + L"\r\n";
        if (!item.strCategory.IsEmpty())
            strBlock += L"CATEGORY=" + EscapeFieldValue(item.strCategory) + L"\r\n";
        if (item.nSourcePage > 0)
        {
            CStringW strPage;
            strPage.Format(L"SOURCE_PAGE=%d\r\n", item.nSourcePage);
            strBlock += strPage;
        }
        if (!item.strSourcePdfPath.IsEmpty())
            strBlock += L"SOURCE_PDF=" + EscapeFieldValue(item.strSourcePdfPath) + L"\r\n";
        if (!item.strCreated.IsEmpty())
            strBlock += L"CREATED=" + item.strCreated + L"\r\n";

        strBlock += L"QUESTION=" + EscapeFieldValue(item.strQuestion) + L"\r\n";
        strBlock += L"CHOICE1=" + EscapeFieldValue(item.strChoice1) + L"\r\n";
        strBlock += L"CHOICE2=" + EscapeFieldValue(item.strChoice2) + L"\r\n";
        strBlock += L"CHOICE3=" + EscapeFieldValue(item.strChoice3) + L"\r\n";
        strBlock += L"CHOICE4=" + EscapeFieldValue(item.strChoice4) + L"\r\n";
        strBlock += L"ANSWER=" + EscapeFieldValue(item.strAnswer) + L"\r\n";
        if (!item.strExplain.IsEmpty())
            strBlock += L"EXPLANATION=" + EscapeFieldValue(item.strExplain) + L"\r\n";

        strBlock += L"QEND\r\n";
        return strBlock;
    }

    BOOL WriteUtf8File(const CStringW& strFilePath, const CStringW& strContent)
    {
        try
        {
            std::ofstream ofs(strFilePath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
                return FALSE;

            const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            ofs.write(reinterpret_cast<const char*>(bom), sizeof(bom));

            const std::string strUtf8 = TrainingUtil::CStringWToUtf8(strContent);
            if (!strUtf8.empty())
                ofs.write(strUtf8.data(), static_cast<std::streamsize>(strUtf8.size()));

            return TRUE;
        }
        catch (...)
        {
            return FALSE;
        }
    }

    BOOL ParseBookHeader(
        const CStringW& strContent,
        QUESTION_BOOK_INFO& outInfo)
    {
        outInfo = QUESTION_BOOK_INFO();

        const int nBookStart = strContent.Find(L"BOOKSTART");
        if (nBookStart < 0)
            return FALSE;

        const int nBookEnd = strContent.Find(L"BOOKEND", nBookStart);
        if (nBookEnd < 0)
            return FALSE;

        CStringW strHeader = strContent.Mid(nBookStart, nBookEnd - nBookStart);

        int nPos = 0;
        CStringW strLine = strHeader.Tokenize(L"\r\n", nPos);
        while (!strLine.IsEmpty() || nPos >= 0)
        {
            strLine = TrimValue(strLine);
            const int nEq = strLine.Find(L'=');
            if (nEq > 0)
            {
                const CStringW strKey = TrimValue(strLine.Left(nEq));
                const CStringW strValue = strLine.Mid(nEq + 1);

                if (strKey.CompareNoCase(L"NAME") == 0)
                    outInfo.strName = strValue;
                else if (strKey.CompareNoCase(L"SAVED_AT") == 0)
                    outInfo.strSavedAt = strValue;
                else if (strKey.CompareNoCase(L"QUESTION_COUNT") == 0)
                    outInfo.nQuestionCount = _wtoi(strValue);
                else if (strKey.CompareNoCase(L"FORMAT_VERSION") == 0)
                    outInfo.nFormatVersion = _wtoi(strValue);
            }

            if (nPos < 0)
                break;
            strLine = strHeader.Tokenize(L"\r\n", nPos);
        }

        return !outInfo.strName.IsEmpty();
    }

    BOOL ReadFileContent(const CStringW& strFilePath, CStringW& outContent)
    {
        std::string strUtf8;
        if (!TrainingUtil::ReadTextFileUtf8(strFilePath, strUtf8))
            return FALSE;

        outContent = TrainingUtil::Utf8ToCStringW(strUtf8);
        return TRUE;
    }

    int CompareSavedAtDesc(const QUESTION_BOOK_INFO& a, const QUESTION_BOOK_INFO& b)
    {
        const int nCmp = b.strSavedAt.CompareNoCase(a.strSavedAt);
        if (nCmp != 0)
            return nCmp;
        return b.strName.CompareNoCase(a.strName);
    }
}

CStringW QuestionBookStorage::GetQuestionBooksFolder()
{
    const CStringW strFolder = TrainingUtil::GetAppDirectory() + L"QuestionBooks\\";
    ScpPaths::EnsureFolder(strFolder);
    return strFolder;
}

CStringW QuestionBookStorage::SanitizeBookName(const CStringW& strRawName)
{
    CStringW strName = strRawName;
    strName.Trim();

    static const wchar_t* kInvalidChars = L"\\/:*?\"<>|";
    for (int i = 0; kInvalidChars[i] != L'\0'; ++i)
        strName.Remove(kInvalidChars[i]);

    strName.Trim();
    return strName;
}

CStringW QuestionBookStorage::BuildBookFilePath(const CStringW& strBookName)
{
    const CStringW strSafeName = SanitizeBookName(strBookName);
    if (strSafeName.IsEmpty())
        return CStringW();

    return GetQuestionBooksFolder() + strSafeName + kFileExtension;
}

BOOL QuestionBookStorage::BookFileExists(const CStringW& strBookName)
{
    const CStringW strPath = BuildBookFilePath(strBookName);
    if (strPath.IsEmpty())
        return FALSE;

    const DWORD dwAttr = GetFileAttributesW(strPath);
    return dwAttr != INVALID_FILE_ATTRIBUTES && !(dwAttr & FILE_ATTRIBUTE_DIRECTORY);
}

BOOL QuestionBookStorage::ListQuestionBooks(
    CQuestionBookInfoArray& outBooks,
    CStringW& strError)
{
    outBooks.clear();
    strError.Empty();

    const CStringW strFolder = GetQuestionBooksFolder();
    const CStringW strPattern = strFolder + L"*" + kFileExtension;

    WIN32_FIND_DATAW fd = {};
    HANDLE hFind = FindFirstFileW(strPattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        const DWORD dwErr = GetLastError();
        if (dwErr == ERROR_FILE_NOT_FOUND)
            return TRUE;
        strError = L"저장된 문제집 목록을 읽을 수 없습니다.";
        return FALSE;
    }

    do
    {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            continue;

        const CStringW strFilePath = strFolder + fd.cFileName;
        CStringW strContent;
        if (!ReadFileContent(strFilePath, strContent))
            continue;

        QUESTION_BOOK_INFO info;
        if (!ParseBookHeader(strContent, info))
            continue;

        info.strFilePath = strFilePath;

        if (info.nQuestionCount <= 0)
        {
            CQuestionItemArray questions;
            CStringW strParseError;
            if (QuestionTestLoader::ParseFromContent(strContent, questions, strParseError))
                info.nQuestionCount = static_cast<int>(questions.size());
        }

        outBooks.push_back(info);
    }
    while (FindNextFileW(hFind, &fd));

    FindClose(hFind);

    std::sort(
        outBooks.begin(),
        outBooks.end(),
        [](const QUESTION_BOOK_INFO& a, const QUESTION_BOOK_INFO& b)
        {
            return CompareSavedAtDesc(a, b) < 0;
        });

    return TRUE;
}

BOOL QuestionBookStorage::SaveQuestionBook(
    const CStringW& strBookName,
    const CQuestionItemArray& questions,
    CStringW& strError)
{
    strError.Empty();

    if (questions.empty())
    {
        strError = L"임시 문제집에 저장할 문제가 없습니다.";
        return FALSE;
    }

    const CStringW strSafeName = SanitizeBookName(strBookName);
    if (strSafeName.IsEmpty())
    {
        strError = L"문제집 이름을 입력하세요.";
        return FALSE;
    }

    const CStringW strFilePath = BuildBookFilePath(strSafeName);
    const CStringW strSavedAt = OpenAiClient::GetCurrentTimestamp();

    CStringW strDocument;
    strDocument.Format(
        L"BOOKSTART\r\n"
        L"FORMAT_VERSION=%d\r\n"
        L"NAME=%s\r\n"
        L"SAVED_AT=%s\r\n"
        L"QUESTION_COUNT=%d\r\n"
        L"BOOKEND\r\n\r\n",
        kFormatVersion,
        strSafeName.GetString(),
        strSavedAt.GetString(),
        static_cast<int>(questions.size()));

    for (int i = 0; i < static_cast<int>(questions.size()); ++i)
        strDocument += FormatQuestionBlock(questions[i], i + 1);

    if (!WriteUtf8File(strFilePath, strDocument))
    {
        strError = L"문제집 파일을 저장할 수 없습니다.\r\n\r\n" + strFilePath;
        return FALSE;
    }

    return TRUE;
}

BOOL QuestionBookStorage::LoadQuestionBook(
    const CStringW& strFilePath,
    CQuestionItemArray& outQuestions,
    QUESTION_BOOK_INFO& outInfo,
    CStringW& strError)
{
    outQuestions.clear();
    outInfo = QUESTION_BOOK_INFO();
    strError.Empty();

    if (strFilePath.IsEmpty())
    {
        strError = L"문제집 파일 경로가 비어 있습니다.";
        return FALSE;
    }

    CStringW strContent;
    if (!ReadFileContent(strFilePath, strContent))
    {
        strError = L"문제집 파일을 읽을 수 없습니다.\r\n\r\n" + strFilePath;
        return FALSE;
    }

    if (!ParseBookHeader(strContent, outInfo))
    {
        strError = L"문제집 헤더(BOOKSTART/BOOKEND)를 읽을 수 없습니다.";
        return FALSE;
    }

    outInfo.strFilePath = strFilePath;

    if (!QuestionTestLoader::ParseFromContent(strContent, outQuestions, strError))
        return FALSE;

    if (outQuestions.empty())
    {
        strError = L"문제집에 유효한 문제가 없습니다.";
        return FALSE;
    }

    if (outInfo.nQuestionCount <= 0)
        outInfo.nQuestionCount = static_cast<int>(outQuestions.size());

    return TRUE;
}

BOOL QuestionBookStorage::DeleteQuestionBook(
    const CStringW& strFilePath,
    CStringW& strError)
{
    strError.Empty();

    if (strFilePath.IsEmpty())
    {
        strError = L"삭제할 문제집 파일 경로가 비어 있습니다.";
        return FALSE;
    }

    if (!DeleteFileW(strFilePath))
    {
        strError = L"문제집 파일을 삭제할 수 없습니다.\r\n\r\n" + strFilePath;
        return FALSE;
    }

    return TRUE;
}
