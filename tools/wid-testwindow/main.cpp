/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-testwindow X Y WIDTH HEIGHT [SECONDS] [LAG]: shows a plain application window (window
    rect at X, Y), prints its handle (hex) and runs until it is closed or the time is up. With
    LAG (milliseconds) every move takes that long, like a busy program that falls behind.
    Used by the runtime smoke test (tests/runtime/smoke.ps1).
*/
#include <windows.h>

#include <cstdio>
#include <cstdlib>

namespace
{

DWORD s_lag = 0;

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_WINDOWPOSCHANGED && s_lag > 0 && !(reinterpret_cast<const WINDOWPOS *>(lParam)->flags & SWP_NOMOVE)) {
        Sleep(s_lag);
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        FillRect(dc, &paint.rcPaint, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
        EndPaint(hwnd, &paint);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

} // namespace

int wmain(int argc, wchar_t **argv)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const int x = argc > 1 ? _wtoi(argv[1]) : 200;
    const int y = argc > 2 ? _wtoi(argv[2]) : 200;
    const int width = argc > 3 ? _wtoi(argv[3]) : 640;
    const int height = argc > 4 ? _wtoi(argv[4]) : 400;
    const int seconds = argc > 5 ? _wtoi(argv[5]) : 300;
    s_lag = argc > 6 ? static_cast<DWORD>(_wtoi(argv[6])) : 0;

    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = L"WidTestWindow";
    RegisterClassW(&windowClass);
    HWND hwnd = CreateWindowExW(0, windowClass.lpszClassName, L"wid-testwindow", WS_OVERLAPPEDWINDOW, x, y, width, height, nullptr, nullptr,
                                windowClass.hInstance, nullptr);
    if (!hwnd) {
        return 1;
    }
    // Created where it shall be, not moved after showing: the canvas service restores
    // remembered places when a window appears, and a later move of our own would undo that.
    ShowWindow(hwnd, SW_SHOWNORMAL);
    std::printf("0x%llX\n", static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(hwnd)));
    std::fflush(stdout);
    SetTimer(hwnd, 1, static_cast<UINT>(seconds) * 1000, nullptr);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.hwnd == hwnd) {
            DestroyWindow(hwnd);
            continue;
        }
        DispatchMessageW(&message);
    }
    return 0;
}
