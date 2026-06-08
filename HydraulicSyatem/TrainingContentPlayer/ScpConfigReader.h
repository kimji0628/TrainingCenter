#pragma once

struct SCP_OPENAI_CONFIG
{
    CStringW strApiKey;
    CStringW strModel;

    BOOL IsValid() const
    {
        return !strApiKey.IsEmpty() && !strModel.IsEmpty();
    }
};

namespace ScpConfigReader
{
    CStringW GetConfigFilePath();
    BOOL LoadOpenAiConfig(SCP_OPENAI_CONFIG& outConfig, CStringW& strError);
}
