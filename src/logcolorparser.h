#pragma once

#include <string>
#include <vector>

namespace logcolors
{
    enum class DiagnosticKind { Normal, Warning, Error, Success };
    struct NativeColorToken
    {
        size_t sourcePosition;
        size_t cleanPosition;
        int code;
    };

    DiagnosticKind ClassifyDiagnosticLine(const std::wstring& line);
    bool ParseNativeColorCodes(const std::wstring& line, std::wstring& clean, std::vector<NativeColorToken>& tokens);
    bool ShouldJoinDiagnosticChunks(const std::wstring& left, const std::wstring& right);
}

