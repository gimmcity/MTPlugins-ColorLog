#include "logcolorparser.h"

#include <iostream>
#include <string>
#include <vector>

using logcolors::ClassifyDiagnosticLine;
using logcolors::ClassifyDiagnosticColors;
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
        { L"^1linker diagnostic without an error prefix", DiagnosticKind::Error },
        { L"^3asset warning without a warning prefix", DiagnosticKind::Warning },
        { L"Empty asset name given, asset type 'warning'!!!!", DiagnosticKind::Warning },
        { L"Empty asset name given, asset type 'warning!!!!'", DiagnosticKind::Warning },
        { L"Empty asset name given, asset type \\warning!!!!'", DiagnosticKind::Warning },
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
    const logcolors::DiagnosticColors yellowMidLine = ClassifyDiagnosticColors(L"ordinary prefix ^3yellow payload");
    const logcolors::DiagnosticColors redMidLine = ClassifyDiagnosticColors(L"ordinary prefix ^1red payload");
    const logcolors::DiagnosticColors bothColors = ClassifyDiagnosticColors(L"^3yellow ^1red");
    if (!yellowMidLine.warning || yellowMidLine.error || redMidLine.warning || !redMidLine.error ||
        !bothColors.warning || !bothColors.error)
    {
        std::cerr << "rendered color diagnostic classification failed\n";
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

    std::vector<size_t> warnings;
    std::vector<size_t> errors;
    const std::wstring indexed = L"normal\nWARNING: first\nERROR: first\nwarning second\nBuild succeeded\nFATAL ERROR: second";
    logcolors::CollectDiagnosticPositions(indexed, warnings, errors);
    if (warnings != std::vector<size_t>{ 7, 35 } || errors != std::vector<size_t>{ 22, 66 })
    {
        std::cerr << "diagnostic position indexing failed\n";
        return 1;
    }
    const std::wstring nativeIndexed = L"normal\n^3asset warning\n^1fatal\nWARNING: later";
    logcolors::CollectDiagnosticPositions(nativeIndexed, warnings, errors);
    if (warnings != std::vector<size_t>{ 7, 27 } || errors != std::vector<size_t>{ 21 })
    {
        std::cerr << "native diagnostic position normalization failed\n";
        return 1;
    }
    const std::wstring cleanedNativeIndexed =
        L"WARNING: first\nnormal\nEmpty asset name given, asset type 'warning'!!!!\nERROR: later";
    logcolors::CollectDiagnosticPositions(cleanedNativeIndexed, warnings, errors);
    if (warnings != std::vector<size_t>{ 0, 22 } || errors != std::vector<size_t>{ 71 })
    {
        std::cerr << "cleaned native warning re-indexing failed\n";
        return 1;
    }

    for (int i = 0; i < 250000; ++i)
    {
        const wchar_t* line = i % 1000 == 0 ? L"WARNING: stress test" : L"normal compiler output";
        (void)ClassifyDiagnosticLine(line);
    }
    std::cout << "logcolorparser: diagnostics, positions, native ^0-^7, split chunks and 250000-line stress pass\n";
    return 0;
}
