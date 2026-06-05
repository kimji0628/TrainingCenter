#pragma once

#ifndef __AFXWIN_H__
    #error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"

// ============================================================================
// TrainingContentPlayer.h - MFC 애플리케이션 클래스
// ============================================================================

class CTrainingContentPlayerApp : public CWinApp
{
public:
    CTrainingContentPlayerApp();

    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CTrainingContentPlayerApp theApp;
