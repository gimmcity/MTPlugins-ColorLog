#pragma once

#include <string>

namespace logcolors
{
    void Setup(const std::wstring& iniPath);
    void Tick();
    void Shutdown();
    bool Command(const std::string& command, std::string& reply);
}
