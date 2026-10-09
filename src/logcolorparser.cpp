#include "logcolorparser.h"

#include <cwctype>

namespace logcolors
{
namespace
{
    bool StartsWithToken(const std::wstring& line, size_t at, const wchar_t* token)
    {
        size_t i = 0;
        while (token[i])
        {
            if (at + i >= line.size() || std::towupper(line[at + i]) != std::towupper(token[i])) return false;
            ++i;
        }
        if (at + i == line.size()) return true;
        const wchar_t next = line[at + i];
        return next == L':' || next == L']' || iswspace(next);
    }
}

DiagnosticKind ClassifyDiagnosticLine(const std::wstring& line)
{
    size_t p = 0;
    while (p < line.size() && iswspace(line[p])) ++p;
    while (p + 1 < line.size() && line[p] == L'^' && line[p + 1] >= L'0' && line[p + 1] <= L'9') p += 2;
    while (p < line.size() && iswspace(line[p])) ++p;
    // Linker/compiler diagnostics are sometimes surrounded by visual separators.
    while (p < line.size() && (line[p] == L'*' || line[p] == L'=' || line[p] == L'-')) ++p;
    while (p < line.size() && iswspace(line[p])) ++p;
    const bool bracketed = p < line.size() && line[p] == L'[';
    if (bracketed) ++p;

    if (StartsWithToken(line, p, L"UNRECOVERABLE ERROR") ||
        StartsWithToken(line, p, L"FATAL ERROR") ||
        StartsWithToken(line, p, L"SCRIPT ERROR") ||
        StartsWithToken(line, p, L"ERROR")) return DiagnosticKind::Error;
    if (StartsWithToken(line, p, L"WARNING")) return DiagnosticKind::Warning;
    if (!bracketed && (StartsWithToken(line, p, L"BUILD SUCCEEDED") || StartsWithToken(line, p, L"DONE")))
        return DiagnosticKind::Success;
    return DiagnosticKind::Normal;
}

bool ParseNativeColorCodes(const std::wstring& line, std::wstring& clean, std::vector<NativeColorToken>& tokens)
{
    clean.clear();
    tokens.clear();
    clean.reserve(line.size());
    for (size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == L'^' && i + 1 < line.size() && line[i + 1] >= L'0' && line[i + 1] <= L'7')
        {
            tokens.push_back(NativeColorToken{ i, clean.size(), int(line[i + 1] - L'0') });
            ++i;
            continue;
        }
        clean.push_back(line[i]);
    }
    return !tokens.empty();
}

bool ShouldJoinDiagnosticChunks(const std::wstring& left, const std::wstring& right)
{
    if (left.empty() || right.empty()) return false;
    if (left == L"^" && right[0] >= L'0' && right[0] <= L'7') return true;

    const std::wstring joined = left + right;
    auto normalized = [](const std::wstring& value)
    {
        size_t p = 0;
        while (p < value.size() && iswspace(value[p])) ++p;
        if (p + 1 < value.size() && value[p] == L'^' && value[p + 1] >= L'0' && value[p + 1] <= L'7') p += 2;
        std::wstring result;
        for (; p < value.size(); ++p) result.push_back(std::towupper(value[p]));
        return result;
    };
    const std::wstring a = normalized(left);
    const std::wstring both = normalized(joined);
    static const wchar_t* const words[] = {
        L"WARNING", L"ERROR", L"FATAL ERROR", L"UNRECOVERABLE ERROR", L"SCRIPT ERROR", L"DONE", L"BUILD SUCCEEDED"
    };
    for (const wchar_t* wordText : words)
    {
        const std::wstring word(wordText);
        const bool leftIsProperPrefix = a.size() < word.size() && word.compare(0, a.size(), a) == 0;
        const bool joinedContinuesWord = (both.size() <= word.size() && word.compare(0, both.size(), both) == 0) ||
                                         (both.size() > word.size() && both.compare(0, word.size(), word) == 0);
        if (leftIsProperPrefix && joinedContinuesWord) return true;
    }
    return false;
}
}

