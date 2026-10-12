#include "logcolors.h"
#include "logcolorparser.h"
#include "modtools_plugin.hpp"

#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

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
        void (*plainSetCursor)(void*, const void*) = nullptr;
        void (*plainSetCurrentFormat)(void*, const void*) = nullptr;
        void (*plainEnsureCursorVisible)(void*) = nullptr;
        void (*cursorDtor)(void*) = nullptr;
        bool (*cursorHasSelection)(const void*) = nullptr;
        void (*cursorSetPosition)(void*, int, int) = nullptr;
        void (*cursorSetFormat)(void*, const void*) = nullptr;
        void (*cursorSetBlockFormat)(void*, const void*) = nullptr;
        void (*cursorRemoveSelectedText)(void*) = nullptr;
        void* (*formatCtor)(void*) = nullptr;
        void (*formatDtor)(void*) = nullptr;
        void* (*blockFormatCtor)(void*) = nullptr;
        void (*blockFormatDtor)(void*) = nullptr;
        void* (*colorCtor)(void*, int, int, int, int) = nullptr;
        void* (*brushCtor)(void*, const void*, int) = nullptr;
        void (*brushDtor)(void*) = nullptr;
        void (*formatSetForeground)(void*, const void*) = nullptr;
        void (*formatSetBackground)(void*, const void*) = nullptr;
        uintptr_t (*widgetWinId)(const void*) = nullptr;
        void* (*verticalScrollBar)(const void*) = nullptr;
        int (*sliderValue)(const void*) = nullptr;
        void (*sliderSetValue)(void*, int) = nullptr;
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
    HWND g_counterWindow = nullptr;
    HWND g_mainWindow = nullptr;
    HWND g_outputWindow = nullptr;
    HINSTANCE g_module = nullptr;
    HFONT g_counterFont = nullptr;
    ULONG_PTR g_gdiplusToken = 0;
    Gdiplus::Bitmap* g_errorGlow = nullptr;
    Gdiplus::Bitmap* g_warningGlow = nullptr;
    IStream* g_errorGlowStream = nullptr;
    IStream* g_warningGlowStream = nullptr;
    wchar_t g_counterClass[96] = {};
    std::vector<size_t> g_warningPositions;
    std::vector<size_t> g_errorPositions;
    std::unordered_map<std::wstring, DiagnosticColors> g_rememberedNativeColors;
    size_t g_nextWarning = 0;
    size_t g_nextError = 0;
    int g_mouseDownButton = 0;
    int g_clickButton = 0;
    ULONGLONG g_clickStarted = 0;

    struct ScrollAnimation
    {
        bool active = false;
        void* bar = nullptr;
        int from = 0;
        int to = 0;
        ULONGLONG started = 0;
        int highlightPosition = -1;
        DiagnosticKind highlightKind = DiagnosticKind::Normal;
    } g_scroll;

    struct HighlightAnimation
    {
        bool active = false;
        int start = 0;
        int length = 0;
        DiagnosticKind kind = DiagnosticKind::Normal;
        int phase = -1;
        ULONGLONG started = 0;
    } g_highlight;

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
        ok &= Get(qt.plainSetCursor, widgets, "?setTextCursor@QPlainTextEdit@@QEAAXAEBVQTextCursor@@@Z");
        ok &= Get(qt.plainSetCurrentFormat, widgets, "?setCurrentCharFormat@QPlainTextEdit@@QEAAXAEBVQTextCharFormat@@@Z");
        ok &= Get(qt.plainEnsureCursorVisible, widgets, "?ensureCursorVisible@QPlainTextEdit@@QEAAXXZ");
        ok &= Get(qt.cursorDtor, gui, "??1QTextCursor@@QEAA@XZ");
        ok &= Get(qt.cursorHasSelection, gui, "?hasSelection@QTextCursor@@QEBA_NXZ");
        ok &= Get(qt.cursorSetPosition, gui, "?setPosition@QTextCursor@@QEAAXHW4MoveMode@1@@Z");
        ok &= Get(qt.cursorSetFormat, gui, "?setCharFormat@QTextCursor@@QEAAXAEBVQTextCharFormat@@@Z");
        ok &= Get(qt.cursorSetBlockFormat, gui, "?setBlockFormat@QTextCursor@@QEAAXAEBVQTextBlockFormat@@@Z");
        ok &= Get(qt.cursorRemoveSelectedText, gui, "?removeSelectedText@QTextCursor@@QEAAXXZ");
        ok &= Get(qt.formatCtor, gui, "??0QTextCharFormat@@QEAA@XZ");
        ok &= Get(qt.formatDtor, gui, "??1QTextCharFormat@@QEAA@XZ");
        ok &= Get(qt.blockFormatCtor, gui, "??0QTextBlockFormat@@QEAA@XZ");
        ok &= Get(qt.blockFormatDtor, gui, "??1QTextBlockFormat@@QEAA@XZ");
        ok &= Get(qt.colorCtor, gui, "??0QColor@@QEAA@HHHH@Z");
        ok &= Get(qt.brushCtor, gui, "??0QBrush@@QEAA@AEBVQColor@@W4BrushStyle@Qt@@@Z");
        ok &= Get(qt.brushDtor, gui, "??1QBrush@@QEAA@XZ");
        ok &= Get(qt.formatSetForeground, gui, "?setForeground@QTextFormat@@QEAAXAEBVQBrush@@@Z");
        ok &= Get(qt.formatSetBackground, gui, "?setBackground@QTextFormat@@QEAAXAEBVQBrush@@@Z");
        ok &= Get(qt.widgetWinId, widgets, "?winId@QWidget@@QEBA_KXZ");
        ok &= Get(qt.verticalScrollBar, widgets, "?verticalScrollBar@QAbstractScrollArea@@QEBAPEAVQScrollBar@@XZ");
        ok &= Get(qt.sliderValue, widgets, "?value@QAbstractSlider@@QEBAHXZ");
        ok &= Get(qt.sliderSetValue, widgets, "?setValue@QAbstractSlider@@QEAAXH@Z");
        qt.bound = ok;
        return ok;
    }

    void UpdateCounterButtons()
    {
        g_nextError = g_errorPositions.empty() ? 0 : g_nextError % g_errorPositions.size();
        g_nextWarning = g_warningPositions.empty() ? 0 : g_nextWarning % g_warningPositions.size();
        if (g_counterWindow) InvalidateRect(g_counterWindow, nullptr, FALSE);
    }

    void RefreshDiagnosticPositions(const std::wstring& sourceText)
    {
        g_warningPositions.clear();
        g_errorPositions.clear();
        size_t start = 0;
        size_t removedCharacters = 0;
        while (start < sourceText.size())
        {
            size_t end = sourceText.find(L'\n', start);
            if (end == std::wstring::npos) end = sourceText.size();
            const std::wstring line = sourceText.substr(start, end - start);
            std::wstring clean;
            std::vector<NativeColorToken> tokens;
            const bool hasNativeColors = ParseNativeColorCodes(line, clean, tokens);
            if (!hasNativeColors) clean = line;

            DiagnosticColors colors = ClassifyDiagnosticColors(line);
            if (hasNativeColors && !clean.empty() && (colors.warning || colors.error))
            {
                DiagnosticColors& remembered = g_rememberedNativeColors[clean];
                remembered.warning = remembered.warning || colors.warning;
                remembered.error = remembered.error || colors.error;
            }
            const auto remembered = g_rememberedNativeColors.find(clean);
            if (remembered != g_rememberedNativeColors.end())
            {
                colors.warning = colors.warning || remembered->second.warning;
                colors.error = colors.error || remembered->second.error;
            }

            const size_t visibleStart = start - removedCharacters;
            if (colors.warning) g_warningPositions.push_back(visibleStart);
            if (colors.error) g_errorPositions.push_back(visibleStart);
            if (hasNativeColors) removedCharacters += tokens.size() * 2;
            start = end + 1;
        }
        UpdateCounterButtons();
    }

    void StartHighlight(int position, DiagnosticKind kind);

    void ResetInsertionFormat()
    {
        if (!g_output) return;
        alignas(16) unsigned char cursor[64] = {};
        alignas(16) unsigned char format[64] = {};
        qt.plainCursor(g_output, cursor);
        // setCurrentCharFormat would recolor selected text. Preserve user selections and
        // reset the insertion style as soon as the console cursor has no selection again.
        if (!qt.cursorHasSelection(cursor))
        {
            qt.formatCtor(format);
            qt.plainSetCurrentFormat(g_output, format);
            qt.formatDtor(format);
        }
        qt.cursorDtor(cursor);
    }

    void NavigateTo(const std::vector<size_t>& positions, size_t& next, DiagnosticKind kind)
    {
        if (!g_output || positions.empty()) return;
        const int position = int(positions[next % positions.size()]);
        next = (next + 1) % positions.size();

        alignas(16) unsigned char originalCursor[64] = {};
        alignas(16) unsigned char targetCursor[64] = {};
        qt.plainCursor(g_output, originalCursor);
        qt.plainCursor(g_output, targetCursor);
        qt.cursorSetPosition(targetCursor, position, 0);
        void* bar = qt.verticalScrollBar(g_output);
        const int oldValue = bar ? qt.sliderValue(bar) : 0;
        qt.plainSetCursor(g_output, targetCursor);
        qt.plainEnsureCursorVisible(g_output);
        const int targetValue = bar ? qt.sliderValue(bar) : oldValue;
        // Restore the launcher's insertion cursor before returning. Leaving it on a yellow/red line makes
        // QPlainTextEdit inherit that character format for later compiler output.
        qt.plainSetCursor(g_output, originalCursor);
        ResetInsertionFormat();
        if (bar)
        {
            qt.sliderSetValue(bar, oldValue);
            g_scroll = ScrollAnimation{ oldValue != targetValue, bar, oldValue, targetValue, GetTickCount64(),
                                        position, kind };
        }
        qt.cursorDtor(targetCursor);
        qt.cursorDtor(originalCursor);
        if (!g_scroll.active) StartHighlight(position, kind);
    }

    Gdiplus::Bitmap* LoadPngResource(int resourceId, IStream** retainedStream)
    {
        if (!g_module || !retainedStream) return nullptr;
        HRSRC resource = FindResourceW(g_module, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!resource) return nullptr;
        HGLOBAL loaded = LoadResource(g_module, resource);
        const DWORD size = SizeofResource(g_module, resource);
        const void* bytes = loaded ? LockResource(loaded) : nullptr;
        if (!bytes || size == 0) return nullptr;

        HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!copy) return nullptr;
        void* destination = GlobalLock(copy);
        if (!destination)
        {
            GlobalFree(copy);
            return nullptr;
        }
        std::memcpy(destination, bytes, size);
        GlobalUnlock(copy);

        IStream* stream = nullptr;
        if (FAILED(CreateStreamOnHGlobal(copy, TRUE, &stream)))
        {
            GlobalFree(copy);
            return nullptr;
        }
        Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(stream, FALSE);
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok)
        {
            delete bitmap;
            stream->Release();
            return nullptr;
        }
        *retainedStream = stream;
        return bitmap;
    }

    void EnsureGlowIcons()
    {
        if (g_gdiplusToken || !g_module) return;
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &input, nullptr) != Gdiplus::Ok)
        {
            g_gdiplusToken = 0;
            return;
        }
        g_errorGlow = LoadPngResource(401, &g_errorGlowStream);
        g_warningGlow = LoadPngResource(402, &g_warningGlowStream);
    }

    void ShutdownGlowIcons()
    {
        delete g_errorGlow;
        delete g_warningGlow;
        g_errorGlow = nullptr;
        g_warningGlow = nullptr;
        if (g_errorGlowStream) g_errorGlowStream->Release();
        if (g_warningGlowStream) g_warningGlowStream->Release();
        g_errorGlowStream = nullptr;
        g_warningGlowStream = nullptr;
        if (g_gdiplusToken) Gdiplus::GdiplusShutdown(g_gdiplusToken);
        g_gdiplusToken = 0;
    }

    void DrawCounterButton(HDC dc, RECT rect, int buttonId, COLORREF accent, size_t count, const wchar_t* suffix)
    {
        const bool pressed = g_mouseDownButton == buttonId;
        double pulse = 0.0;
        if (g_clickButton == buttonId)
            pulse = std::max(0.0, 1.0 - double(GetTickCount64() - g_clickStarted) / 190.0);
        if (pressed)
        {
            InflateRect(&rect, -1, -1);
            OffsetRect(&rect, 0, 2);
        }
        const int lift = int(std::lround(15.0 * pulse));
        HBRUSH fill = CreateSolidBrush(pressed ? RGB(18, 20, 22) : RGB(26 + lift, 28 + lift, 30 + lift));
        const COLORREF borderColor = pulse > 0.0 ? accent : RGB(55, 58, 62);
        HPEN border = CreatePen(PS_SOLID, 1, borderColor);
        HGDIOBJ oldBrush = SelectObject(dc, fill);
        HGDIOBJ oldPen = SelectObject(dc, border);
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(border);
        DeleteObject(fill);

        const int centerY = (rect.top + rect.bottom) / 2;
        Gdiplus::Bitmap* glow = buttonId == 1 ? g_errorGlow : g_warningGlow;
        if (glow)
        {
            Gdiplus::Graphics graphics(dc);
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            graphics.DrawImage(glow, rect.left + 4, centerY - 9, 18, 18);
        }
        else
        {
            HBRUSH dotBrush = CreateSolidBrush(accent);
            oldBrush = SelectObject(dc, dotBrush);
            oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
            Ellipse(dc, rect.left + 7, centerY - 5, rect.left + 17, centerY + 5);
            SelectObject(dc, oldPen);
            SelectObject(dc, oldBrush);
            DeleteObject(dotBrush);
        }

        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_counterFont ? g_counterFont : GetStockObject(DEFAULT_GUI_FONT));
        const std::wstring number = std::to_wstring(count);
        RECT numberRect = rect;
        numberRect.left += 26;
        SetTextColor(dc, accent);
        DrawTextW(dc, number.c_str(), int(number.size()), &numberRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SIZE numberSize = {};
        GetTextExtentPoint32W(dc, number.c_str(), int(number.size()), &numberSize);
        RECT suffixRect = rect;
        suffixRect.left += 26 + numberSize.cx + 4;
        SetTextColor(dc, RGB(220, 222, 224));
        DrawTextW(dc, suffix, -1, &suffixRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    LRESULT CALLBACK CounterWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_LBUTTONDOWN:
        {
            const int x = GET_X_LPARAM(lParam);
            g_mouseDownButton = x < 100 ? 1 : x >= 106 ? 2 : 0;
            if (g_mouseDownButton)
            {
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                UpdateWindow(hwnd);
            }
            return 0;
        }
        case WM_LBUTTONUP:
        {
            const int x = GET_X_LPARAM(lParam);
            const int releasedButton = x < 100 ? 1 : x >= 106 ? 2 : 0;
            const int clickedButton = releasedButton == g_mouseDownButton ? releasedButton : 0;
            if (GetCapture() == hwnd) ReleaseCapture();
            g_mouseDownButton = 0;
            if (clickedButton)
            {
                g_clickButton = clickedButton;
                g_clickStarted = GetTickCount64();
                if (clickedButton == 1) NavigateTo(g_errorPositions, g_nextError, DiagnosticKind::Error);
                else NavigateTo(g_warningPositions, g_nextWarning, DiagnosticKind::Warning);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_CAPTURECHANGED:
            g_mouseDownButton = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_PAINT:
        {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(hwnd, &ps);
            RECT client = {};
            GetClientRect(hwnd, &client);
            HDC buffer = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
            HGDIOBJ oldBitmap = SelectObject(buffer, bitmap);
            HBRUSH background = CreateSolidBrush(RGB(24, 26, 28));
            FillRect(buffer, &client, background);
            DeleteObject(background);
            DrawCounterButton(buffer, RECT{ 0, 0, 100, 30 }, 1, RGB(255, 91, 124), g_errorPositions.size(), L"errors");
            DrawCounterButton(buffer, RECT{ 106, 0, 228, 30 }, 2, RGB(234, 179, 79), g_warningPositions.size(), L"warnings");
            BitBlt(dc, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
            SelectObject(buffer, oldBitmap);
            DeleteObject(bitmap);
            DeleteDC(buffer);
            EndPaint(hwnd, &ps);
            return 0;
        }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    void CreateCounterWindow()
    {
        if (g_counterWindow || !rp::Has(&rp::host->MainWindow)) return;
        g_mainWindow = static_cast<HWND>(rp::host->MainWindow(rp::self));
        if (!g_mainWindow) return;
        g_outputWindow = g_output ? reinterpret_cast<HWND>(qt.widgetWinId(g_output)) : nullptr;
        if (g_outputWindow && !IsWindow(g_outputWindow)) g_outputWindow = nullptr;
        if (!g_module)
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&CounterWindowProc), &g_module);
        if (!g_module) return;
        EnsureGlowIcons();
        _snwprintf_s(g_counterClass, _countof(g_counterClass), _TRUNCATE, L"ColorLogCounter_%lu_%p", GetCurrentProcessId(), g_module);
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof wc;
        wc.lpfnWndProc = CounterWindowProc;
        wc.hInstance = g_module;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.lpszClassName = g_counterClass;
        if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
        if (!g_counterFont)
            g_counterFont = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HWND parent = g_outputWindow ? g_outputWindow : g_mainWindow;
        g_counterWindow = CreateWindowExW(WS_EX_NOACTIVATE, g_counterClass, L"", WS_CHILD | WS_VISIBLE,
                                          0, 0, 228, 30, parent, nullptr, g_module, nullptr);
        UpdateCounterButtons();
    }

    void PositionCounterWindow()
    {
        if (!g_counterWindow || !g_mainWindow) return;
        HWND parent = g_outputWindow && IsWindow(g_outputWindow) ? g_outputWindow : g_mainWindow;
        RECT client = {};
        if (!GetClientRect(parent, &client)) return;
        const int x = std::max(4, int(client.right) - 228 - 14);
        const int y = 12;
        SetWindowPos(g_counterWindow, HWND_TOP, x, y, 228, 30, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    void TickScrollAnimation(ULONGLONG now)
    {
        if (!g_scroll.active || !g_scroll.bar) return;
        constexpr double duration = 260.0;
        const double elapsed = double(now - g_scroll.started);
        const double t = std::min(1.0, elapsed / duration);
        const double inverse = 1.0 - t;
        const double eased = 1.0 - inverse * inverse * inverse;
        const int value = int(std::lround(double(g_scroll.from) + double(g_scroll.to - g_scroll.from) * eased));
        qt.sliderSetValue(g_scroll.bar, value);
        if (t >= 1.0)
        {
            const int position = g_scroll.highlightPosition;
            const DiagnosticKind kind = g_scroll.highlightKind;
            g_scroll.active = false;
            if (position >= 0) StartHighlight(position, kind);
        }
    }

    void TickCounterAnimation(ULONGLONG now)
    {
        if (!g_clickButton || !g_counterWindow) return;
        if (now - g_clickStarted >= 190) g_clickButton = 0;
        InvalidateRect(g_counterWindow, nullptr, FALSE);
    }

    void DestroyCounters()
    {
        if (g_counterWindow) DestroyWindow(g_counterWindow);
        g_counterWindow = nullptr;
        if (g_module && g_counterClass[0]) UnregisterClassW(g_counterClass, g_module);
        if (g_counterFont) DeleteObject(g_counterFont);
        g_counterFont = nullptr;
        g_counterClass[0] = L'\0';
        g_mainWindow = nullptr;
        g_outputWindow = nullptr;
        g_module = nullptr;
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

    void ApplyDefaultFormat(int start, int length)
    {
        if (length <= 0) return;
        alignas(16) unsigned char cursor[64] = {};
        alignas(16) unsigned char format[64] = {};
        qt.plainCursor(g_output, cursor);
        qt.cursorSetPosition(cursor, start, 0);
        qt.cursorSetPosition(cursor, start + length, 1);
        qt.formatCtor(format);
        qt.cursorSetFormat(cursor, format);
        qt.formatDtor(format);
        qt.cursorDtor(cursor);
    }

    void ApplyColor(int start, int length, int red, int green, int blue, bool countLine = true)
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
        // Replace instead of merge so inherited yellow/red/bold formatting cannot leak into later output.
        qt.cursorSetFormat(cursor, format);
        if (countLine) ++g_coloredLines;
        qt.brushDtor(brush);
        qt.formatDtor(format);
        qt.cursorDtor(cursor);
    }

    void SetHighlightBlockBackground(bool visible)
    {
        if (!g_highlight.active || g_highlight.length <= 0) return;
        alignas(16) unsigned char cursor[64] = {};
        alignas(16) unsigned char blockFormat[64] = {};
        alignas(16) unsigned char color[32] = {};
        alignas(16) unsigned char brush[32] = {};
        qt.plainCursor(g_output, cursor);
        qt.cursorSetPosition(cursor, g_highlight.start, 0);
        qt.blockFormatCtor(blockFormat);
        if (visible)
        {
            if (g_highlight.kind == DiagnosticKind::Error) qt.colorCtor(color, 105, 42, 52, 105);
            else qt.colorCtor(color, 105, 88, 28, 105);
            qt.brushCtor(brush, color, 1);
            qt.formatSetBackground(blockFormat, brush);
            qt.brushDtor(brush);
        }
        qt.cursorSetBlockFormat(cursor, blockFormat);
        qt.blockFormatDtor(blockFormat);
        qt.cursorDtor(cursor);
    }

    void RestoreHighlightBase() { SetHighlightBlockBackground(false); }
    void ApplyHighlightBackground() { SetHighlightBlockBackground(true); }

    void StartHighlight(int position, DiagnosticKind kind)
    {
        if (!g_output || position < 0 || size_t(position) >= g_previousText.size()) return;
        if (g_highlight.active) RestoreHighlightBase();
        size_t end = g_previousText.find(L'\n', size_t(position));
        if (end == std::wstring::npos) end = g_previousText.size();
        g_highlight = HighlightAnimation{ true, position, int(end - size_t(position)), kind, 0, GetTickCount64() };
        ApplyHighlightBackground();
    }

    void TickHighlightAnimation(ULONGLONG now)
    {
        if (!g_highlight.active) return;
        const ULONGLONG elapsed = now - g_highlight.started;
        const int phase = elapsed < 90 ? 0 : elapsed < 160 ? 1 : elapsed < 250 ? 2 : 3;
        if (phase == g_highlight.phase) return;
        g_highlight.phase = phase;
        if (phase == 0 || phase == 2) ApplyHighlightBackground();
        else RestoreHighlightBase();
        if (phase == 3) g_highlight.active = false;
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
        case 7: ApplyDefaultFormat(start, length); break;
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
            else if (scan < text.size())
            {
                const size_t previousLine = g_previousText.rfind(L'\n');
                scan = previousLine == std::wstring::npos ? 0 : previousLine + 1;
            }
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
                ApplyDefaultFormat(int(it->start), int(clean.size()));
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
            default: ApplyDefaultFormat(int(it->start), int(it->end - it->start)); break;
            }
        }
        g_previousText = PlainText(g_output); // token removal changed the visible document text
        // Diagnostic selections must never become the format inherited by later linker output.
        ResetInsertionFormat();
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
    g_counterWindow = nullptr;
    g_mainWindow = nullptr;
    g_outputWindow = nullptr;
    g_module = nullptr;
    g_counterFont = nullptr;
    g_counterClass[0] = L'\0';
    g_warningPositions.clear();
    g_errorPositions.clear();
    g_rememberedNativeColors.clear();
    g_nextWarning = 0;
    g_nextError = 0;
    g_mouseDownButton = 0;
    g_clickButton = 0;
    g_clickStarted = 0;
    g_scroll = ScrollAnimation();
    g_highlight = HighlightAnimation();
    if (!BindQt()) rp::Log("Color Log: BO3 Qt formatting exports were not found; diagnostic colors are disabled.");
}

void Tick()
{
    if (!qt.bound) return;
    const bool enabled = GetPrivateProfileIntW(L"logcolors", L"enabled", 1, g_ini.c_str()) != 0;
    if (!enabled)
    {
        if (g_counterWindow) ShowWindow(g_counterWindow, SW_HIDE);
        return;
    }
    const ULONGLONG now = GetTickCount64();
    TickScrollAnimation(now);
    TickHighlightAnimation(now);
    TickCounterAnimation(now);
    if (now < g_nextPoll) return;
    g_nextPoll = now + 100;
    MarkTitle();
    if (!g_output) g_output = FindOutput();
    if (!g_output) return;
    CreateCounterWindow();
    PositionCounterWindow();
    ResetInsertionFormat();
    if (!g_loggedReady)
    {
        rp::Log("Color Log: attached to the stock launcher's output console (warnings yellow, errors red).");
        g_loggedReady = true;
    }
    std::wstring text = PlainText(g_output);
    if (text.size() < g_previousText.size()) g_rememberedNativeColors.clear();
    if (RepairSplitDiagnosticPrefixes(text)) g_previousText.clear();
    if (text != g_previousText)
    {
        ColorNewText(text);
        RefreshDiagnosticPositions(text);
    }
}

void Shutdown()
{
    g_scroll.active = false;
    if (g_highlight.active) RestoreHighlightBase();
    g_highlight.active = false;
    ShutdownGlowIcons();
    DestroyCounters();
    g_output = nullptr;
    g_previousText.clear();
    g_warningPositions.clear();
    g_errorPositions.clear();
    g_rememberedNativeColors.clear();
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
            : g_output ? "Color Log active on the stock launcher console; " + std::to_string(g_errorPositions.size()) +
                         " errors, " + std::to_string(g_warningPositions.size()) + " warnings, formatted lines: " +
                         std::to_string(g_coloredLines) + "."
                       : "Color Log waiting for the launcher console.";
        return true;
    }
    return false;
}
}
