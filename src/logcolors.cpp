#include "logcolors.h"
#include "logcolorparser.h"
#include "modtools_plugin.hpp"

#include <windows.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace logcolors
{
namespace
{
    struct QArrayDataRaw { int ref; int size; unsigned alloc; ptrdiff_t offset; };

    struct Qt
    {
        bool bound = false;
        void (*qsDtor)(void**) = nullptr;
        void* (*qsFromUtf8)(void*, const char*, int) = nullptr;
        void** (*plainText)(const void*, void**) = nullptr;
        void (*plainAppend)(void*, const void*) = nullptr;
        void* (*plainCursor)(const void*, void*) = nullptr;
        void (*cursorDtor)(void*) = nullptr;
        void (*cursorSetPosition)(void*, int, int) = nullptr;
        void (*cursorMergeFormat)(void*, const void*) = nullptr;
        void (*cursorRemoveSelectedText)(void*) = nullptr;
        void* (*formatCtor)(void*) = nullptr;
        void (*formatDtor)(void*) = nullptr;
        void* (*colorCtor)(void*, int, int, int, int) = nullptr;
        void* (*brushCtor)(void*, const void*, int) = nullptr;
        void (*brushDtor)(void*) = nullptr;
        void (*formatSetForeground)(void*, const void*) = nullptr;
    } qt;

    template <class T> bool Get(T& fn, HMODULE module, const char* name)
    {
        fn = reinterpret_cast<T>(GetProcAddress(module, name));
        return fn != nullptr;
    }

    std::wstring g_ini;
    void* g_output = nullptr;
    std::wstring g_previousText;
    ULONGLONG g_nextPoll = 0;
    bool g_loggedReady = false;
    bool g_titleMarked = false;
    unsigned long long g_coloredLines = 0;

    bool BindQt()
    {
        HMODULE widgets = GetModuleHandleW(L"Qt5Widgets64r.dll");
        HMODULE core = GetModuleHandleW(L"Qt5Core64r.dll");
        HMODULE gui = GetModuleHandleW(L"Qt5Gui64r.dll");
        if (!widgets || !core || !gui) return false;

        bool ok = true;
        ok &= Get(qt.qsDtor, core, "??1QString@@QEAA@XZ");
        ok &= Get(qt.qsFromUtf8, core, "?fromUtf8@QString@@SA?AV1@PEBDH@Z");
        ok &= Get(qt.plainText, widgets, "?toPlainText@QPlainTextEdit@@QEBA?AVQString@@XZ");
        ok &= Get(qt.plainAppend, widgets, "?appendPlainText@QPlainTextEdit@@QEAAXAEBVQString@@@Z");
        ok &= Get(qt.plainCursor, widgets, "?textCursor@QPlainTextEdit@@QEBA?AVQTextCursor@@XZ");
        ok &= Get(qt.cursorDtor, gui, "??1QTextCursor@@QEAA@XZ");
        ok &= Get(qt.cursorSetPosition, gui, "?setPosition@QTextCursor@@QEAAXHW4MoveMode@1@@Z");
        ok &= Get(qt.cursorMergeFormat, gui, "?mergeCharFormat@QTextCursor@@QEAAXAEBVQTextCharFormat@@@Z");
        ok &= Get(qt.cursorRemoveSelectedText, gui, "?removeSelectedText@QTextCursor@@QEAAXXZ");
        ok &= Get(qt.formatCtor, gui, "??0QTextCharFormat@@QEAA@XZ");
        ok &= Get(qt.formatDtor, gui, "??1QTextCharFormat@@QEAA@XZ");
        ok &= Get(qt.colorCtor, gui, "??0QColor@@QEAA@HHHH@Z");
        ok &= Get(qt.brushCtor, gui, "??0QBrush@@QEAA@AEBVQColor@@W4BrushStyle@Qt@@@Z");
        ok &= Get(qt.brushDtor, gui, "??1QBrush@@QEAA@XZ");
        ok &= Get(qt.formatSetForeground, gui, "?setForeground@QTextFormat@@QEAAXAEBVQBrush@@@Z");
        qt.bound = ok;
        return ok;
    }

    void MarkTitle()
    {
        if (g_titleMarked || !rp::Has(&rp::host->MainWindow)) return;
        HWND hwnd = static_cast<HWND>(rp::host->MainWindow(rp::self));
        if (!hwnd) return;
        wchar_t title[512] = {};
        GetWindowTextW(hwnd, title, int(_countof(title)));
        std::wstring text(title);
        if (text.find(L"Color Log") == std::wstring::npos)
        {
            text += L" (Color Log)";
            SetWindowTextW(hwnd, text.c_str());
        }
        g_titleMarked = true;
    }

    void* FindOutput()
    {
        using FindFn = void* (*)(uintptr_t);
        static FindFn find = nullptr;
        if (!find)
            find = reinterpret_cast<FindFn>(GetProcAddress(GetModuleHandleW(L"Qt5Widgets64r.dll"), "?find@QWidget@@SAPEAV1@_K@Z"));
        const ModToolsUiApi* ui = rp::host->ui;
        HWND hwnd = rp::Has(&rp::host->MainWindow) ? static_cast<HWND>(rp::host->MainWindow(rp::self)) : nullptr;
        void* main = find && hwnd ? find(reinterpret_cast<uintptr_t>(hwnd)) : nullptr;
        return ui && main && rp::HasUi(&ui->FindChild) ? ui->FindChild(main, "QPlainTextEdit", 0) : nullptr;
    }

    std::wstring PlainText(void* edit)
    {
        void* qstring = nullptr;
        qt.plainText(edit, &qstring);
        const auto* data = static_cast<const QArrayDataRaw*>(qstring);
        std::wstring result;
        if (data && data->size > 0)
        {
            const wchar_t* chars = reinterpret_cast<const wchar_t*>(reinterpret_cast<const char*>(data) + data->offset);
            result.assign(chars, chars + data->size);
        }
        qt.qsDtor(&qstring);
        return result;
    }

    void ApplyColor(int start, int length, int red, int green, int blue)
    {
        if (length <= 0) return;
        alignas(16) unsigned char cursor[64] = {};
        alignas(16) unsigned char format[64] = {};
        alignas(16) unsigned char color[32] = {};
        alignas(16) unsigned char brush[32] = {};

        qt.plainCursor(g_output, cursor);
        qt.cursorSetPosition(cursor, start, 0);       // QTextCursor::MoveAnchor
        qt.cursorSetPosition(cursor, start + length, 1); // QTextCursor::KeepAnchor
        qt.formatCtor(format);
        qt.colorCtor(color, red, green, blue, 255);
        qt.brushCtor(brush, color, 1);                // Qt::SolidPattern
        qt.formatSetForeground(format, brush);
        qt.cursorMergeFormat(cursor, format);
        ++g_coloredLines;
        qt.brushDtor(brush);
        qt.formatDtor(format);
        qt.cursorDtor(cursor);
    }

    void RemoveText(int start, int length)
    {
        alignas(16) unsigned char cursor[64] = {};
        qt.plainCursor(g_output, cursor);
        qt.cursorSetPosition(cursor, start, 0);
        qt.cursorSetPosition(cursor, start + length, 1);
        qt.cursorRemoveSelectedText(cursor);
        qt.cursorDtor(cursor);
    }

    bool RepairSplitDiagnosticPrefixes(std::wstring& text)
    {
        bool changed = false;
        size_t newline = text.find(L'\n');
        while (newline != std::wstring::npos)
        {
            const size_t leftStartAt = newline == 0 ? 0 : text.rfind(L'\n', newline - 1) + 1;
            const size_t rightEndAt = text.find(L'\n', newline + 1);
            const size_t rightEnd = rightEndAt == std::wstring::npos ? text.size() : rightEndAt;
            const std::wstring left = text.substr(leftStartAt, newline - leftStartAt);
            const std::wstring right = text.substr(newline + 1, rightEnd - newline - 1);
            if (ShouldJoinDiagnosticChunks(left, right))
            {
                RemoveText(int(newline), 1);
                text.erase(newline, 1);
                changed = true;
                newline = leftStartAt == 0 ? text.find(L'\n') : text.find(L'\n', leftStartAt - 1);
                continue;
            }
            newline = text.find(L'\n', newline + 1);
        }
        return changed;
    }

    void ApplyNativeColor(int code, int start, int length)
    {
        switch (code)
        {
        case 0: ApplyColor(start, length, 0, 0, 0); break;
        case 1: ApplyColor(start, length, 255, 82, 82); break;
        case 2: ApplyColor(start, length, 70, 210, 105); break;
        case 3: ApplyColor(start, length, 255, 204, 0); break;
        case 4: ApplyColor(start, length, 70, 140, 255); break;
        case 5: ApplyColor(start, length, 0, 220, 220); break;
        case 6: ApplyColor(start, length, 230, 90, 230); break;
        case 7: break; // Default console foreground is already present on this untouched span.
        }
    }

    void ColorNewText(const std::wstring& text)
    {
        size_t scan = 0;
        if (!g_previousText.empty() && text.size() >= g_previousText.size() &&
            std::equal(g_previousText.begin(), g_previousText.end(), text.begin()))
        {
            scan = g_previousText.size();
            if (scan < text.size() && text[scan] == L'\n') ++scan;
        }

        struct Line { size_t start; size_t end; };
        std::vector<Line> lines;
        while (scan < text.size())
        {
            size_t end = text.find(L'\n', scan);
            if (end == std::wstring::npos) end = text.size();
            lines.push_back(Line{ scan, end });
            scan = end + 1;
        }

        // Work backwards so removing a ^n token never invalidates positions in lines still to process.
        for (auto it = lines.rbegin(); it != lines.rend(); ++it)
        {
            const std::wstring line = text.substr(it->start, it->end - it->start);
            std::wstring clean;
            std::vector<NativeColorToken> tokens;
            if (ParseNativeColorCodes(line, clean, tokens))
            {
                for (auto token = tokens.rbegin(); token != tokens.rend(); ++token)
                    RemoveText(int(it->start + token->sourcePosition), 2);
                for (size_t n = 0; n < tokens.size(); ++n)
                {
                    const size_t begin = tokens[n].cleanPosition;
                    const size_t finish = n + 1 < tokens.size() ? tokens[n + 1].cleanPosition : clean.size();
                    if (finish > begin) ApplyNativeColor(tokens[n].code, int(it->start + begin), int(finish - begin));
                }
                continue;
            }

            switch (ClassifyDiagnosticLine(line))
            {
            case DiagnosticKind::Error:   ApplyColor(int(it->start), int(it->end - it->start), 255, 82, 82); break;
            case DiagnosticKind::Warning: ApplyColor(int(it->start), int(it->end - it->start), 255, 204, 0); break;
            case DiagnosticKind::Success: ApplyColor(int(it->start), int(it->end - it->start), 70, 210, 105); break;
            default: break;
            }
        }
        g_previousText = PlainText(g_output); // token removal changed the visible document text
    }
}

void Setup(const std::wstring& iniPath)
{
    g_ini = iniPath;
    g_output = nullptr;
    g_previousText.clear();
    g_nextPoll = 0;
    g_loggedReady = false;
    g_titleMarked = false;
    g_coloredLines = 0;
    if (!BindQt()) rp::Log("Color Log: BO3 Qt formatting exports were not found; diagnostic colors are disabled.");
}

void Tick()
{
    if (!qt.bound || !GetPrivateProfileIntW(L"logcolors", L"enabled", 1, g_ini.c_str())) return;
    const ULONGLONG now = GetTickCount64();
    if (now < g_nextPoll) return;
    g_nextPoll = now + 100;
    MarkTitle();
    if (!g_output) g_output = FindOutput();
    if (!g_output) return;
    if (!g_loggedReady)
    {
        rp::Log("Color Log: attached to the stock launcher's output console (warnings yellow, errors red).");
        g_loggedReady = true;
    }
    std::wstring text = PlainText(g_output);
    if (RepairSplitDiagnosticPrefixes(text)) g_previousText.clear();
    if (text != g_previousText) ColorNewText(text);
}

void Shutdown()
{
    g_output = nullptr;
    g_previousText.clear();
}

bool Command(const std::string& command, std::string& reply)
{
    if (command == "logcolors test")
    {
        if (!qt.bound || !g_output) { reply = "Color Log is not attached."; return true; }
        const char* lines[] = { "normal Color Log self-test", "WARNING: Color Log self-test", "ERROR: Color Log self-test",
                                "Build succeeded", "done: Color Log self-test" };
        for (const char* line : lines)
        {
            void* qstring = nullptr;
            qt.qsFromUtf8(&qstring, line, int(strlen(line)));
            qt.plainAppend(g_output, &qstring);
            qt.qsDtor(&qstring);
        }
        reply = "Color Log self-test lines appended.";
        return true;
    }
    if (command == "logcolors status")
    {
        reply = !qt.bound ? "Color Log unavailable: BO3 Qt exports were not found."
            : !GetPrivateProfileIntW(L"logcolors", L"enabled", 1, g_ini.c_str()) ? "Color Log disabled."
            : g_output ? "Color Log active on the stock launcher console; formatted lines: " + std::to_string(g_coloredLines) + "."
                       : "Color Log waiting for the launcher console.";
        return true;
    }
    return false;
}
}

