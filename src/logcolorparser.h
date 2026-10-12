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
    struct DiagnosticColors
    {
        bool warning;
        bool error;
    };

    DiagnosticKind ClassifyDiagnosticLine(const std::wstring& line);
    DiagnosticColors ClassifyDiagnosticColors(const std::wstring& line);
    void CollectDiagnosticPositions(const std::wstring& text, std::vector<size_t>& warnings, std::vector<size_t>& errors);
    bool ParseNativeColorCodes(const std::wstring& line, std::wstring& clean, std::vector<NativeColorToken>& tokens);
    bool ShouldJoinDiagnosticChunks(const std::wstring& left, const std::wstring& right);
}
