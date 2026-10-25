/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    wid-media: real input and screenshots for the runtime tests and the images of the
    documentation (tools/wid-media/README.md). Runs one command per call; coordinates are
    physical screen pixels. No Qt, a single executable.
*/
#include <windows.h>

#include <objbase.h>
#include <wincodec.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace
{

int fail(const wchar_t *message)
{
    std::fwprintf(stderr, L"wid-media: %ls\n", message);
    return 1;
}

int number(const wchar_t *text)
{
    return static_cast<int>(std::wcstol(text, nullptr, 10));
}

// --- Keyboard ------------------------------------------------------------------------------

WORD keyCode(std::wstring name)
{
    for (wchar_t &c : name) {
        c = static_cast<wchar_t>(towlower(c));
    }
    struct Named
    {
        const wchar_t *name;
        WORD key;
    };
    static const Named names[] = {
        {L"win", VK_LWIN},       {L"ctrl", VK_LCONTROL}, {L"alt", VK_LMENU},     {L"shift", VK_LSHIFT}, {L"escape", VK_ESCAPE},
        {L"esc", VK_ESCAPE},     {L"enter", VK_RETURN},  {L"tab", VK_TAB},       {L"space", VK_SPACE},  {L"backspace", VK_BACK},
        {L"delete", VK_DELETE},  {L"left", VK_LEFT},     {L"right", VK_RIGHT},   {L"up", VK_UP},        {L"down", VK_DOWN},
        {L"home", VK_HOME},      {L"end", VK_END},       {L"plus", VK_OEM_PLUS}, {L"minus", VK_OEM_MINUS},
    };
    for (const Named &named : names) {
        if (name == named.name) {
            return named.key;
        }
    }
    if (name.size() >= 2 && name[0] == L'f') {
        const int index = number(name.c_str() + 1);
        if (index >= 1 && index <= 12) {
            return static_cast<WORD>(VK_F1 + index - 1);
        }
    }
    if (name.size() == 1 && iswalnum(name[0])) {
        return static_cast<WORD>(towupper(name[0]));
    }
    return 0;
}

INPUT keyInput(WORD key, bool up)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = key;
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_DELETE || key == VK_HOME || key == VK_END || key == VK_LWIN) {
        input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    }
    return input;
}

/// "win+alt+m": modifiers down in order, the last key down and up, modifiers up in reverse.
bool parseCombo(const std::wstring &combo, std::vector<WORD> *keys)
{
    std::size_t start = 0;
    while (start <= combo.size()) {
        const std::size_t plus = combo.find(L'+', start);
        const std::wstring part = combo.substr(start, plus == std::wstring::npos ? std::wstring::npos : plus - start);
        const WORD key = keyCode(part);
        if (!key) {
            return false;
        }
        keys->push_back(key);
        if (plus == std::wstring::npos) {
            break;
        }
        start = plus + 1;
    }
    return !keys->empty();
}

void pressModifiers(const std::vector<WORD> &modifiers, bool up)
{
    std::vector<INPUT> inputs;
    if (up) {
        for (auto it = modifiers.rbegin(); it != modifiers.rend(); ++it) {
            inputs.push_back(keyInput(*it, true));
        }
    } else {
        for (WORD key : modifiers) {
            inputs.push_back(keyInput(key, false));
        }
    }
    if (!inputs.empty()) {
        SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    }
}

int key(const std::wstring &combo)
{
    std::vector<WORD> keys;
    if (!parseCombo(combo, &keys)) {
        return fail(L"unknown key");
    }
    const WORD last = keys.back();
    keys.pop_back();
    pressModifiers(keys, false);
    INPUT press[2] = {keyInput(last, false), keyInput(last, true)};
    SendInput(2, press, sizeof(INPUT));
    pressModifiers(keys, true);
    return 0;
}

int type(const std::wstring &text)
{
    for (wchar_t c : text) {
        INPUT inputs[2] = {};
        for (INPUT &input : inputs) {
            input.type = INPUT_KEYBOARD;
            input.ki.wScan = c;
            input.ki.dwFlags = KEYEVENTF_UNICODE;
        }
        inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(2, inputs, sizeof(INPUT));
        Sleep(15);
    }
    return 0;
}

// --- Mouse ---------------------------------------------------------------------------------

std::vector<WORD> modifiersFrom(int argc, wchar_t **argv, int first)
{
    std::vector<WORD> modifiers;
    for (int i = first; i < argc; ++i) {
        parseCombo(argv[i], &modifiers);
    }
    return modifiers;
}

void moveTo(int x, int y)
{
    SetCursorPos(x, y);
    // A zero move as input, so that hooks and the window below see the position.
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

DWORD buttonFlag(const std::wstring &button, bool up)
{
    if (button == L"right") {
        return up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
    }
    if (button == L"middle") {
        return up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
    }
    return up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
}

void button(const std::wstring &name, bool up)
{
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = buttonFlag(name, up);
    SendInput(1, &input, sizeof(INPUT));
}

int click(int x, int y, const std::wstring &name, const std::vector<WORD> &modifiers, int count)
{
    moveTo(x, y);
    Sleep(30);
    pressModifiers(modifiers, false);
    for (int i = 0; i < count; ++i) {
        button(name, false);
        Sleep(20);
        button(name, true);
        Sleep(40);
    }
    pressModifiers(modifiers, true);
    return 0;
}

int drag(int x1, int y1, int x2, int y2, int steps, const std::wstring &name, const std::vector<WORD> &modifiers)
{
    steps = steps < 1 ? 1 : steps;
    moveTo(x1, y1);
    Sleep(30);
    pressModifiers(modifiers, false);
    button(name, false);
    Sleep(30);
    for (int i = 1; i <= steps; ++i) {
        moveTo(x1 + (x2 - x1) * i / steps, y1 + (y2 - y1) * i / steps);
        Sleep(16);
    }
    Sleep(60);
    button(name, true);
    pressModifiers(modifiers, true);
    return 0;
}

int wheel(int x, int y, int clicks, const std::vector<WORD> &modifiers)
{
    moveTo(x, y);
    Sleep(30);
    pressModifiers(modifiers, false);
    const int direction = clicks < 0 ? -1 : 1;
    for (int i = 0; i < std::abs(clicks); ++i) {
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_WHEEL;
        input.mi.mouseData = static_cast<DWORD>(direction * WHEEL_DELTA);
        SendInput(1, &input, sizeof(INPUT));
        Sleep(60);
    }
    pressModifiers(modifiers, true);
    return 0;
}

// --- Screenshots ---------------------------------------------------------------------------

template<typename T>
void release(T *&pointer)
{
    if (pointer) {
        pointer->Release();
        pointer = nullptr;
    }
}

bool writePng(const std::wstring &path, int width, int height, const std::vector<BYTE> &bgra)
{
    IWICImagingFactory *factory = nullptr;
    IWICStream *stream = nullptr;
    IWICBitmapEncoder *encoder = nullptr;
    IWICBitmapFrameEncode *frame = nullptr;
    bool ok = SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    ok = ok && SUCCEEDED(factory->CreateStream(&stream));
    ok = ok && SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE));
    ok = ok && SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder));
    ok = ok && SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache));
    ok = ok && SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr));
    ok = ok && SUCCEEDED(frame->Initialize(nullptr));
    ok = ok && SUCCEEDED(frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height)));
    // PNG has no 32-bit format without alpha; the encoder would pick another one silently.
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    ok = ok && SUCCEEDED(frame->SetPixelFormat(&format)) && IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA);
    ok = ok && SUCCEEDED(frame->WritePixels(static_cast<UINT>(height), static_cast<UINT>(width) * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE *>(bgra.data())));
    ok = ok && SUCCEEDED(frame->Commit());
    ok = ok && SUCCEEDED(encoder->Commit());
    release(frame);
    release(encoder);
    release(stream);
    release(factory);
    return ok;
}

int shot(const std::wstring &path, int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0) {
        x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    HDC screen = GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (bitmap && bits) {
        HGDIOBJ old = SelectObject(memory, bitmap);
        // CAPTUREBLT: layered windows (markers, overlays) are part of the picture.
        ok = BitBlt(memory, 0, 0, width, height, screen, x, y, SRCCOPY | CAPTUREBLT) != FALSE;
        SelectObject(memory, old);
        if (ok) {
            const BYTE *data = static_cast<const BYTE *>(bits);
            std::vector<BYTE> pixels(data, data + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
            // BitBlt leaves the alpha byte empty: the screen is opaque.
            for (std::size_t i = 3; i < pixels.size(); i += 4) {
                pixels[i] = 255;
            }
            ok = writePng(path, width, height, pixels);
        }
        DeleteObject(bitmap);
    }
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    return ok ? 0 : fail(L"screenshot failed");
}

// --- Windows -------------------------------------------------------------------------------

struct Search
{
    std::wstring program;
    HWND found = nullptr;
};

BOOL CALLBACK findProgramWindow(HWND hwnd, LPARAM lParam)
{
    auto *search = reinterpret_cast<Search *>(lParam);
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) || (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
        return TRUE;
    }
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) {
        return TRUE;
    }
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(process, 0, path, &size) != FALSE;
    CloseHandle(process);
    if (!ok) {
        return TRUE;
    }
    const std::wstring full(path);
    const std::wstring name = full.substr(full.find_last_of(L'\\') + 1);
    if (_wcsicmp(name.c_str(), search->program.c_str()) == 0) {
        search->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

/// Moves the topmost visible main window of a program (e.g. notepad.exe), window rect.
int place(const std::wstring &program, int x, int y, int width, int height)
{
    Search search{program};
    EnumWindows(&findProgramWindow, reinterpret_cast<LPARAM>(&search));
    if (!search.found) {
        return fail(L"place: no window of that program");
    }
    ShowWindow(search.found, SW_RESTORE);
    SetWindowPos(search.found, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    return 0;
}

BOOL CALLBACK closeClassWindow(HWND hwnd, LPARAM lParam)
{
    const auto *className = reinterpret_cast<const wchar_t *>(lParam);
    wchar_t name[256] = {};
    if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, name, 256) && _wcsicmp(name, className) == 0) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

/// Prints "X Y WIDTH HEIGHT" of the topmost visible window of a class (#32768: an open menu).
int rectOf(const std::wstring &className)
{
    HWND found = nullptr;
    for (HWND hwnd = GetTopWindow(nullptr); hwnd && !found; hwnd = GetWindow(hwnd, GW_HWNDNEXT)) {
        wchar_t name[256] = {};
        if (IsWindowVisible(hwnd) && GetClassNameW(hwnd, name, 256) && _wcsicmp(name, className.c_str()) == 0) {
            found = hwnd;
        }
    }
    RECT rect = {};
    if (!found || !GetWindowRect(found, &rect)) {
        return fail(L"rect: no such window");
    }
    std::fwprintf(stdout, L"%ld %ld %ld %ld\n", rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    return 0;
}

/// Asks every visible top-level window of a class to close (e.g. CabinetWClass: File Explorer).
int closeClass(const std::wstring &className)
{
    EnumWindows(&closeClassWindow, reinterpret_cast<LPARAM>(className.c_str()));
    return 0;
}

// --- Light and dark mode -------------------------------------------------------------------

int theme(const std::wstring &mode)
{
    if (mode != L"dark" && mode != L"light") {
        return fail(L"theme: dark or light");
    }
    const DWORD light = mode == L"light" ? 1 : 0;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                        nullptr)
        != ERROR_SUCCESS) {
        return fail(L"theme: cannot open the registry key");
    }
    RegSetValueExW(key, L"AppsUseLightTheme", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&light), sizeof(light));
    RegSetValueExW(key, L"SystemUsesLightTheme", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&light), sizeof(light));
    RegCloseKey(key);
    // As the Settings app does: everyone hears about it.
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"), SMTO_ABORTIFHUNG, 2000, &result);
    return 0;
}

void usage()
{
    std::fwprintf(stdout,
                  L"wid-media - input and screenshots for tests and documentation images\n\n"
                  L"  wid-media shot FILE.png [X Y WIDTH HEIGHT]      screenshot (default: all screens)\n"
                  L"  wid-media move X Y\n"
                  L"  wid-media click X Y [left|right|middle] [COUNT] [MODIFIERS]\n"
                  L"  wid-media drag X1 Y1 X2 Y2 [STEPS] [left|middle] [MODIFIERS]\n"
                  L"  wid-media wheel X Y CLICKS [MODIFIERS]          positive = away from the user\n"
                  L"  wid-media key COMBO                             e.g. win+alt+m, escape, ctrl+e\n"
                  L"  wid-media type TEXT\n"
                  L"  wid-media place PROGRAM.exe X Y WIDTH HEIGHT    moves the main window of a program\n"
                  L"  wid-media close CLASS                           closes the windows of a window class\n"
                  L"  wid-media rect CLASS                            X Y WIDTH HEIGHT of a window (#32768: menu)\n"
                  L"  wid-media theme dark|light                      app and system mode (current user)\n"
                  L"  wid-media screen                                virtual screen, work area, DPI\n\n"
                  L"MODIFIERS: win, ctrl, alt, shift, combined with + (win+alt).\n");
}

} // namespace

int wmain(int argc, wchar_t **argv)
{
    // Physical pixels, like the canvas service.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (argc < 2) {
        usage();
        return 1;
    }
    const std::wstring command = argv[1];
    const auto arg = [&](int index) -> std::wstring {
        return index < argc ? std::wstring(argv[index]) : std::wstring();
    };
    if (command == L"shot" && argc >= 3) {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const int result = argc >= 7 ? shot(argv[2], number(argv[3]), number(argv[4]), number(argv[5]), number(argv[6])) : shot(argv[2], 0, 0, 0, 0);
        CoUninitialize();
        return result;
    }
    if (command == L"move" && argc >= 4) {
        moveTo(number(argv[2]), number(argv[3]));
        return 0;
    }
    if (command == L"click" && argc >= 4) {
        const std::wstring name = arg(4).empty() ? L"left" : arg(4);
        int next = 5;
        int count = 1;
        if (next < argc && iswdigit(argv[next][0])) {
            count = number(argv[next]);
            ++next;
        }
        return click(number(argv[2]), number(argv[3]), name, modifiersFrom(argc, argv, next), count);
    }
    if (command == L"drag" && argc >= 6) {
        const int steps = argc >= 7 ? number(argv[6]) : 20;
        const std::wstring name = arg(7).empty() ? L"left" : arg(7);
        return drag(number(argv[2]), number(argv[3]), number(argv[4]), number(argv[5]), steps, name, modifiersFrom(argc, argv, 8));
    }
    if (command == L"wheel" && argc >= 5) {
        return wheel(number(argv[2]), number(argv[3]), number(argv[4]), modifiersFrom(argc, argv, 5));
    }
    if (command == L"key" && argc >= 3) {
        return key(argv[2]);
    }
    if (command == L"type" && argc >= 3) {
        return type(argv[2]);
    }
    if (command == L"place" && argc >= 7) {
        return place(argv[2], number(argv[3]), number(argv[4]), number(argv[5]), number(argv[6]));
    }
    if (command == L"close" && argc >= 3) {
        return closeClass(argv[2]);
    }
    if (command == L"rect" && argc >= 3) {
        return rectOf(argv[2]);
    }
    if (command == L"theme" && argc >= 3) {
        return theme(argv[2]);
    }
    if (command == L"screen") {
        RECT work = {};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        std::fwprintf(stdout, L"%d %d %d %d work %ld %ld %ld %ld dpi %u\n", GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                      GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN), work.left, work.top, work.right - work.left,
                      work.bottom - work.top, GetDpiForSystem());
        return 0;
    }
    usage();
    return 1;
}
