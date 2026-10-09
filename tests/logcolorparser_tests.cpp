#include "../src/logcolorparser.h"

#include <iostream>
#include <string>
#include <vector>

using logcolors::ClassifyDiagnosticLine;
using logcolors::DiagnosticKind;

int main()
{
    struct Case { const wchar_t* text; DiagnosticKind expected; };
    const Case cases[] = {
        { L"WARNING: Missing asset", DiagnosticKind::Warning },
        { L"   warning something", DiagnosticKind::Warning },
        { L"[WARNING] Missing texture", DiagnosticKind::Warning },
        { L"^3WARNING: Missing texture", DiagnosticKind::Warning },
        { L"ERROR: Script compilation failed", DiagnosticKind::Error },
        { L"Fatal Error: link failed", DiagnosticKind::Error },
        { L"UNRECOVERABLE ERROR: linker stopped", DiagnosticKind::Error },
        { L"script error: unknown function", DiagnosticKind::Error },
        { L"**** SCRIPT ERROR ****", DiagnosticKind::Error },
        { L"^1ERROR: Script compilation failed", DiagnosticKind::Error },
        { L"[ERROR] Script compilation failed", DiagnosticKind::Error },
        { L"Build succeeded", DiagnosticKind::Success },
        { L"Build succeeded in 2s", DiagnosticKind::Success },
        { L"done: 0m2s", DiagnosticKind::Success },
        { L"^2done 0m2s", DiagnosticKind::Success },
        { L"0 errors, 2 warnings", DiagnosticKind::Normal },
        { L"The warning count is zero", DiagnosticKind::Normal },
        { L"ERRORCODE is a variable", DiagnosticKind::Normal },
        { L"Rebuild succeeded", DiagnosticKind::Normal },
        { L"", DiagnosticKind::Normal },
    };

    for (const Case& c : cases)
    {
        if (ClassifyDiagnosticLine(c.text) != c.expected)
        {
            std::wcerr << L"classification failed: " << c.text << L'\n';
            return 1;
        }
    }

    std::wstring clean;
    std::vector<logcolors::NativeColorToken> tokens;
    if (!logcolors::ParseNativeColorCodes(L"^1ERROR:^7 Failed ^3zm_test", clean, tokens) ||
        clean != L"ERROR: Failed zm_test" || tokens.size() != 3 ||
        tokens[0].code != 1 || tokens[0].cleanPosition != 0 ||
        tokens[1].code != 7 || tokens[1].cleanPosition != 6 ||
        tokens[2].code != 3 || tokens[2].cleanPosition != 14)
    {
        std::cerr << "native inline color parsing failed\n";
        return 1;
    }
    if (logcolors::ParseNativeColorCodes(L"literal ^ caret and ^9", clean, tokens) || clean != L"literal ^ caret and ^9")
    {
        std::cerr << "literal caret parsing failed\n";
        return 1;
    }
    if (!logcolors::ShouldJoinDiagnosticChunks(L"W", L"ARNING: missing material") ||
        !logcolors::ShouldJoinDiagnosticChunks(L"WARN", L"ING: missing material") ||
        !logcolors::ShouldJoinDiagnosticChunks(L"^", L"1ERROR: failed") ||
        !logcolors::ShouldJoinDiagnosticChunks(L"^1ERR", L"OR: failed") ||
        logcolors::ShouldJoinDiagnosticChunks(L"normal output", L"continues here"))
    {
        std::cerr << "streamed diagnostic prefix repair failed\n";
        return 1;
    }

    for (int i = 0; i < 250000; ++i)
    {
        const wchar_t* line = i % 1000 == 0 ? L"WARNING: stress test" : L"normal compiler output";
        (void)ClassifyDiagnosticLine(line);
    }
    std::cout << "logcolorparser: diagnostics, native ^0-^7, split chunks and 250000-line stress pass\n";
    return 0;
}
