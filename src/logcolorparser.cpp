#include "logcolorparser.h"

#include <cwchar>
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

    bool ContainsInsensitive(const std::wstring& line, const wchar_t* text)
    {
        const size_t length = std::wcslen(text);
        if (length == 0 || length > line.size()) return false;
        for (size_t start = 0; start + length <= line.size(); ++start)
        {
            size_t i = 0;
            while (i < length && std::towupper(line[start + i]) == std::towupper(text[i])) ++i;
            if (i == length) return true;
        }
        return false;
    }
}

DiagnosticKind ClassifyDiagnosticLine(const std::wstring& line)
{
    size_t p = 0;
    int firstNativeColor = -1;
    while (p < line.size() && iswspace(line[p])) ++p;
    while (p + 1 < line.size() && line[p] == L'^' && line[p + 1] >= L'0' && line[p + 1] <= L'9')
    {
        if (firstNativeColor < 0) firstNativeColor = int(line[p + 1] - L'0');
        p += 2;
    }
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
    // The linker emits this warning with only a leading ^3 color code. Once ColorLog removes
    // that code, later output refreshes must still recognize and index the existing line.
    if (ContainsInsensitive(line, L"EMPTY ASSET NAME GIVEN") &&
        ContainsInsensitive(line, L"ASSET TYPE") &&
        ContainsInsensitive(line, L"WARNING"))
        return DiagnosticKind::Warning;
    if (!bracketed && (StartsWithToken(line, p, L"BUILD SUCCEEDED") || StartsWithToken(line, p, L"DONE")))
        return DiagnosticKind::Success;
    // BO3 tools also emit diagnostics that carry only a native color prefix, without a WARNING:/ERROR: word.
    if (firstNativeColor == 1) return DiagnosticKind::Error;
    if (firstNativeColor == 3) return DiagnosticKind::Warning;
    return DiagnosticKind::Normal;
}

DiagnosticColors ClassifyDiagnosticColors(const std::wstring& line)
{
    DiagnosticColors colors{ false, false };
    std::wstring clean;
    std::vector<NativeColorToken> tokens;
    if (ParseNativeColorCodes(line, clean, tokens))
    {
        // Native colors drive the rendered line. Count every line containing yellow/red,
        // even when the color code appears after ordinary text.
        for (const NativeColorToken& token : tokens)
        {
            if (token.code == 3) colors.warning = true;
            if (token.code == 1) colors.error = true;
        }
        return colors;
    }

    const DiagnosticKind kind = ClassifyDiagnosticLine(line);
    colors.warning = kind == DiagnosticKind::Warning;
    colors.error = kind == DiagnosticKind::Error;
    return colors;
}

void CollectDiagnosticPositions(const std::wstring& text, std::vector<size_t>& warnings, std::vector<size_t>& errors)
{
    warnings.clear();
    errors.clear();
    size_t start = 0;
    size_t removedCharacters = 0;
    while (start < text.size())
    {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        const std::wstring line = text.substr(start, end - start);
        const size_t cleanStart = start - removedCharacters;
        const DiagnosticColors colors = ClassifyDiagnosticColors(line);
        if (colors.warning) warnings.push_back(cleanStart);
        if (colors.error) errors.push_back(cleanStart);
        std::wstring clean;
        std::vector<NativeColorToken> tokens;
        if (ParseNativeColorCodes(line, clean, tokens)) removedCharacters += tokens.size() * 2;
        start = end + 1;
    }
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
