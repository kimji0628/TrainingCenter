#pragma once

namespace ScpUiSettings
{
    constexpr double kGeneratedListSplitDefault = 0.6;
    constexpr double kPdfSplitDefault = 0.5;

    CStringW GetSettingsFilePath();

    BOOL LoadGeneratedListSplitRatio(double& outRatio);
    BOOL LoadPdfSplitRatio(double& outRatio);
    BOOL SaveQuizGenLayout(double dGeneratedListSplitRatio, double dPdfSplitRatio);
}
