#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif
#include <string>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>
#include <ctime>
#include <cwchar>
#include <cstdio>

#include "cartridge.h"
#include "bus.h"
#include "cpu6502.h"

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

static const wchar_t* kWindowClass = L"NesEmuWindowClass";
static const wchar_t* kInputConfigClass = L"NesEmuInputConfigClass";
static const int kNesW = 256, kNesH = 240;
static int g_scale = 3;

enum : UINT_PTR
{
    ID_FILE_OPEN = 1,
    ID_INPUT_CONFIG = 2,
    ID_OPTIONS = 3,
    ID_HEX_EDITOR = 4,
    ID_GAME_GENIE = 7,
    ID_TOAST_CONFIG = 8,
    ID_SAVE_STATE_ZIP = 5,
    ID_LOAD_STATE_ZIP = 6,
    ID_SAVE_SLOT_BASE = 10,
    ID_LOAD_SLOT_BASE = 30,
};

Cartridge g_cart;
Bus       g_bus;
Cpu6502   g_cpu;

void GameGenieClearAll();

bool g_romLoaded = false;
bool g_running = false;
bool g_windowActive = true;
std::wstring g_romPath;
HWND g_mainHwnd = nullptr;
BITMAPINFO g_bmi{};
std::vector<u32> g_frameBuf(kNesW * kNesH, 0xFF000000);

struct KeyBindings
{
    int A;
    int B;
    int Select;
    int Start;
    int Up;
    int Down;
    int Left;
    int Right;
};

static KeyBindings g_keys[2] = {
    { 'Z', 'X', VK_RSHIFT, VK_RETURN, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT },
    { 'K', 'J', 'U', 'I', 'W', 'S', 'A', 'D' }
};

static const wchar_t* kButtonNames[8] = { L"A", L"B", L"Select", L"Start", L"Up", L"Down", L"Left", L"Right" };

int* BindingSlot(KeyBindings* kb, int index)
{
    int p = index / 8;
    int b = index % 8;
    switch (b)
    {
    case 0: return &kb[p].A;
    case 1: return &kb[p].B;
    case 2: return &kb[p].Select;
    case 3: return &kb[p].Start;
    case 4: return &kb[p].Up;
    case 5: return &kb[p].Down;
    case 6: return &kb[p].Left;
    default: return &kb[p].Right;
    }
}

std::wstring KeyName(int vk)
{
    if (vk <= 0) return L"(none)";

    switch (vk)
    {
    case VK_BACK: return L"Backspace";
    case VK_TAB: return L"Tab";
    case VK_RETURN: return L"Enter";
    case VK_PAUSE: return L"Pause";
    case VK_CAPITAL: return L"Caps Lock";
    case VK_ESCAPE: return L"Escape";
    case VK_SPACE: return L"Space";
    case VK_PRIOR: return L"Page Up";
    case VK_NEXT: return L"Page Down";
    case VK_END: return L"End";
    case VK_HOME: return L"Home";
    case VK_LEFT: return L"Left Arrow";
    case VK_UP: return L"Up Arrow";
    case VK_RIGHT: return L"Right Arrow";
    case VK_DOWN: return L"Down Arrow";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_LWIN: return L"Left Windows";
    case VK_RWIN: return L"Right Windows";
    case VK_NUMPAD0: return L"Numpad 0";
    case VK_NUMPAD1: return L"Numpad 1";
    case VK_NUMPAD2: return L"Numpad 2";
    case VK_NUMPAD3: return L"Numpad 3";
    case VK_NUMPAD4: return L"Numpad 4";
    case VK_NUMPAD5: return L"Numpad 5";
    case VK_NUMPAD6: return L"Numpad 6";
    case VK_NUMPAD7: return L"Numpad 7";
    case VK_NUMPAD8: return L"Numpad 8";
    case VK_NUMPAD9: return L"Numpad 9";
    case VK_MULTIPLY: return L"Numpad *";
    case VK_ADD: return L"Numpad +";
    case VK_SUBTRACT: return L"Numpad -";
    case VK_DECIMAL: return L"Numpad .";
    case VK_DIVIDE: return L"Numpad /";
    case VK_NUMLOCK: return L"Num Lock";
    case VK_SCROLL: return L"Scroll Lock";
    case VK_LSHIFT: return L"Left Shift";
    case VK_RSHIFT: return L"Right Shift";
    case VK_LCONTROL: return L"Left Ctrl";
    case VK_RCONTROL: return L"Right Ctrl";
    case VK_LMENU: return L"Left Alt";
    case VK_RMENU: return L"Right Alt";
    case VK_OEM_1: return L";";
    case VK_OEM_PLUS: return L"=";
    case VK_OEM_COMMA: return L",";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_PERIOD: return L".";
    case VK_OEM_2: return L"/";
    case VK_OEM_3: return L"`";
    case VK_OEM_4: return L"[";
    case VK_OEM_5: return L"\\";
    case VK_OEM_6: return L"]";
    case VK_OEM_7: return L"'";
    case VK_SNAPSHOT: return L"Print Screen";
    default: break;
    }

    if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
    {
        wchar_t buf[2] = { (wchar_t)vk, 0 };
        return buf;
    }
    if (vk >= VK_F1 && vk <= VK_F24)
    {
        wchar_t buf[8];
        wsprintfW(buf, L"F%d", vk - VK_F1 + 1);
        return buf;
    }

    wchar_t fallback[32];
    wsprintfW(fallback, L"Key 0x%02X", vk);
    return fallback;
}

std::wstring GetConfigDir()
{
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path)))
    {
        std::wstring dir = std::wstring(path) + L"\\Toast";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return L".";
}

std::wstring GetConfigFilePath()
{
    return GetConfigDir() + L"\\keybinds.cfg";
}

std::string NarrowPath(const std::wstring& path)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return std::string();
    std::string narrow(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &narrow[0], len, nullptr, nullptr);
    if (!narrow.empty() && narrow.back() == '\0') narrow.pop_back();
    return narrow;
}

int ShowMsg(const char* text, const char* title, UINT flags)
{
    int r = MessageBoxA(g_mainHwnd, text, title, flags);
    SendMessage(g_mainHwnd, WM_CANCELMODE, 0, 0);
    SetFocus(g_mainHwnd);
    return r;
}

void SaveKeyBindings()
{
    std::wofstream f(NarrowPath(GetConfigFilePath()).c_str());
    if (!f.is_open()) return;
    for (int i = 0; i < 16; i++)
    {
        wchar_t prefix[16];
        wsprintfW(prefix, L"P%d_", (i / 8) + 1);
        f << prefix << kButtonNames[i % 8] << L"=" << *BindingSlot(g_keys, i) << L"\n";
    }
}

void LoadKeyBindings()
{
    std::wifstream f(NarrowPath(GetConfigFilePath()).c_str());
    if (!f.is_open()) return;

    std::wstring line;
    while (std::getline(f, line))
    {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring name = line.substr(0, eq);
        int val = _wtoi(line.substr(eq + 1).c_str());
        if (val <= 0) continue;

        for (int i = 0; i < 16; i++)
        {
            wchar_t expected[32];
            wsprintfW(expected, L"P%d_%s", (i / 8) + 1, kButtonNames[i % 8]);
            if (name == expected)
            {
                *BindingSlot(g_keys, i) = val;
                break;
            }
        }
    }
}

struct ToastBindings
{
    int Reset = 'R';
    int OpenRom = 'O';
    int InputConfig = 'I';
    int ToastConfig = 'T';
    int Options = 'P';
    int HexEditor = 'H';
    int GameGenie = 'G';
    int PauseResume = VK_F8;
};

static ToastBindings g_toastKeys;
static const wchar_t* kToastActionNames[8] = {
    L"Reset", L"OpenRom", L"InputConfig", L"ToastConfig",
    L"Options", L"HexEditor", L"GameGenie", L"PauseResume"
};

int* ToastBindingSlotOf(ToastBindings& kb, int index)
{
    switch (index)
    {
    case 0: return &kb.Reset;
    case 1: return &kb.OpenRom;
    case 2: return &kb.InputConfig;
    case 3: return &kb.ToastConfig;
    case 4: return &kb.Options;
    case 5: return &kb.HexEditor;
    case 6: return &kb.GameGenie;
    default: return &kb.PauseResume;
    }
}

std::wstring GetToastBindingsFilePath()
{
    return GetConfigDir() + L"\\toastbinds.cfg";
}

void SaveToastBindings()
{
    std::wofstream f(NarrowPath(GetToastBindingsFilePath()).c_str());
    if (!f.is_open()) return;
    for (int i = 0; i < 8; i++)
        f << kToastActionNames[i] << L"=" << *ToastBindingSlotOf(g_toastKeys, i) << L"\n";
}

void LoadToastBindings()
{
    std::wifstream f(NarrowPath(GetToastBindingsFilePath()).c_str());
    if (!f.is_open()) return;

    std::wstring line;
    while (std::getline(f, line))
    {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring name = line.substr(0, eq);
        int val = _wtoi(line.substr(eq + 1).c_str());
        if (val <= 0) continue;

        for (int i = 0; i < 8; i++)
        {
            if (name == kToastActionNames[i]) { *ToastBindingSlotOf(g_toastKeys, i) = val; break; }
        }
    }
}

HMENU g_fileMenu = nullptr;
HMENU g_inputMenu = nullptr;
HMENU g_optionsMenu = nullptr;
HMENU g_toolsMenu = nullptr;

std::wstring MenuAccelText(int vk)
{
    return L"\tCtrl+" + KeyName(vk);
}

void RebuildMenuAccelerators()
{
    if (g_fileMenu)
        ModifyMenuW(g_fileMenu, ID_FILE_OPEN, MF_BYCOMMAND | MF_STRING, ID_FILE_OPEN,
            (L"Open ROM..." + MenuAccelText(g_toastKeys.OpenRom)).c_str());

    if (g_inputMenu)
    {
        ModifyMenuW(g_inputMenu, ID_INPUT_CONFIG, MF_BYCOMMAND | MF_STRING, ID_INPUT_CONFIG,
            (L"Configure NES..." + MenuAccelText(g_toastKeys.InputConfig)).c_str());
        ModifyMenuW(g_inputMenu, ID_TOAST_CONFIG, MF_BYCOMMAND | MF_STRING, ID_TOAST_CONFIG,
            (L"Configure Toast..." + MenuAccelText(g_toastKeys.ToastConfig)).c_str());
    }

    if (g_optionsMenu)
        ModifyMenuW(g_optionsMenu, ID_OPTIONS, MF_BYCOMMAND | MF_STRING, ID_OPTIONS,
            (L"Preferences..." + MenuAccelText(g_toastKeys.Options)).c_str());

    if (g_toolsMenu)
    {
        ModifyMenuW(g_toolsMenu, ID_HEX_EDITOR, MF_BYCOMMAND | MF_STRING, ID_HEX_EDITOR,
            (L"Hex Editor..." + MenuAccelText(g_toastKeys.HexEditor)).c_str());
        ModifyMenuW(g_toolsMenu, ID_GAME_GENIE, MF_BYCOMMAND | MF_STRING, ID_GAME_GENIE,
            (L"Game Genie..." + MenuAccelText(g_toastKeys.GameGenie)).c_str());
    }

    if (g_mainHwnd) DrawMenuBar(g_mainHwnd);
}

static HWND g_toastConfigHwnd = nullptr;
static ToastBindings g_toastTempKeys;
static int g_toastListeningIndex = -1;
static bool g_toastConflict[8]{};
static const int ID_TOAST_REBIND_BASE = 500;
static const int ID_TOAST_SAVE = 600;
static const int ID_TOAST_CANCEL = 601;
static const int ID_TOAST_TIMER_KEYPOLL = 3;
static const wchar_t* kToastConfigClass = L"NesEmuToastConfigClass";
static const wchar_t* kToastActionLabels[8] = {
    L"Reset", L"Open ROM", L"Configure NES", L"Configure Toast",
    L"Preferences", L"Hex Editor", L"Game Genie", L"Pause / Resume"
};

void RefreshToastRebindButtonText(HWND hwnd, int index)
{
    HWND btn = GetDlgItem(hwnd, ID_TOAST_REBIND_BASE + index);
    std::wstring text = g_toastListeningIndex == index ? L"Press a key..." : KeyName(*ToastBindingSlotOf(g_toastTempKeys, index));
    SetWindowTextW(btn, text.c_str());
}

void RecomputeToastConflicts(HWND hwnd)
{
    for (int i = 0; i < 8; i++) g_toastConflict[i] = false;
    for (int i = 0; i < 8; i++)
    {
        int vi = *ToastBindingSlotOf(g_toastTempKeys, i);
        if (vi <= 0) continue;
        for (int j = i + 1; j < 8; j++)
        {
            int vj = *ToastBindingSlotOf(g_toastTempKeys, j);
            if (vi == vj) { g_toastConflict[i] = true; g_toastConflict[j] = true; }
        }
    }
    for (int i = 0; i < 8; i++)
        InvalidateRect(GetDlgItem(hwnd, ID_TOAST_REBIND_BASE + i), nullptr, TRUE);
}

LRESULT CALLBACK ToastConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_toastConfigHwnd = hwnd;
        g_toastTempKeys = g_toastKeys;
        const int rowH = 32, labelW = 130, btnW = 150, padX = 16, padY = 16;
        for (int i = 0; i < 8; i++)
        {
            int y = padY + i * rowH;
            CreateWindowW(L"STATIC", kToastActionLabels[i], WS_CHILD | WS_VISIBLE | SS_LEFT,
                padX, y + 5, labelW, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                padX + labelW, y, btnW, 24, hwnd, (HMENU)(UINT_PTR)(ID_TOAST_REBIND_BASE + i), nullptr, nullptr);
        }
        int bottomY = padY + 8 * rowH + 8;
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            padX + labelW, bottomY, 72, 26, hwnd, (HMENU)(UINT_PTR)ID_TOAST_SAVE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            padX + labelW + 82, bottomY, 72, 26, hwnd, (HMENU)(UINT_PTR)ID_TOAST_CANCEL, nullptr, nullptr);

        for (int i = 0; i < 8; i++) RefreshToastRebindButtonText(hwnd, i);
        RecomputeToastConflicts(hwnd);
        SetTimer(hwnd, ID_TOAST_TIMER_KEYPOLL, 40, nullptr);
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= ID_TOAST_REBIND_BASE && id < ID_TOAST_REBIND_BASE + 8)
        {
            g_toastListeningIndex = id - ID_TOAST_REBIND_BASE;
            for (int i = 0; i < 8; i++) RefreshToastRebindButtonText(hwnd, i);
        }
        else if (id == ID_TOAST_SAVE)
        {
            g_toastKeys = g_toastTempKeys;
            SaveToastBindings();
            RebuildMenuAccelerators();
            DestroyWindow(hwnd);
        }
        else if (id == ID_TOAST_CANCEL)
        {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TOAST_TIMER_KEYPOLL && g_toastListeningIndex >= 0)
        {
            for (int vk = 0x08; vk <= 0xFE; vk++)
            {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
                if (!(GetAsyncKeyState(vk) & 0x8000)) continue;

                if (vk != VK_ESCAPE)
                    *ToastBindingSlotOf(g_toastTempKeys, g_toastListeningIndex) = vk;

                g_toastListeningIndex = -1;
                for (int i = 0; i < 8; i++) RefreshToastRebindButtonText(hwnd, i);
                RecomputeToastConflicts(hwnd);
                break;
            }
        }
        return 0;

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID >= ID_TOAST_REBIND_BASE && dis->CtlID < ID_TOAST_REBIND_BASE + 8)
        {
            int idx = dis->CtlID - ID_TOAST_REBIND_BASE;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            HBRUSH bg = CreateSolidBrush(g_toastConflict[idx] ? RGB(220, 60, 60) : GetSysColor(COLOR_BTNFACE));
            FillRect(dis->hDC, &dis->rcItem, bg);
            DeleteObject(bg);

            DrawEdge(dis->hDC, &dis->rcItem, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

            wchar_t text[64];
            GetWindowTextW(dis->hwndItem, text, 64);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, g_toastConflict[idx] ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));
            RECT rc = dis->rcItem;
            DrawTextW(dis->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TOAST_TIMER_KEYPOLL);
        g_toastConfigHwnd = nullptr;
        g_toastListeningIndex = -1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenToastConfig(HWND owner)
{
    if (g_toastConfigHwnd)
    {
        SetForegroundWindow(g_toastConfigHwnd);
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ToastConfigWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = kToastConfigClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 330, 340 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);

    HWND hwnd = CreateWindowW(kToastConfigClass, L"Configure Toast",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);

    ShowWindow(hwnd, SW_SHOW);
}

static bool g_runInBackground = false;
static bool g_discordEnabled = true;

std::wstring GetOptionsFilePath()
{
    return GetConfigDir() + L"\\options.cfg";
}

void SaveOptions()
{
    std::wofstream f(NarrowPath(GetOptionsFilePath()).c_str());
    if (!f.is_open()) return;
    f << L"Scale=" << g_scale << L"\n";
    f << L"Discord=" << (g_discordEnabled ? 1 : 0) << L"\n";
    f << L"Background=" << (g_runInBackground ? 1 : 0) << L"\n";
}

void LoadOptions()
{
    std::wifstream f(NarrowPath(GetOptionsFilePath()).c_str());
    if (!f.is_open()) return;
    std::wstring line;
    while (std::getline(f, line))
    {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring name = line.substr(0, eq);
        int val = _wtoi(line.substr(eq + 1).c_str());
        if (name == L"Scale" && val >= 1 && val <= 5) g_scale = val;
        else if (name == L"Discord") g_discordEnabled = (val != 0);
        else if (name == L"Background") g_runInBackground = (val != 0);
    }
}

static const char* kDiscordClientId = "1550792213561352334";

static HANDLE g_discordPipe = INVALID_HANDLE_VALUE;
static volatile bool g_discordThreadRunning = false;
static HANDLE g_discordThreadHandle = nullptr;
static CRITICAL_SECTION g_discordCs;
static bool g_discordCsInit = false;

static bool g_discordActivityDirty = true;
static std::string g_discordDetails = "In the Menu";
static long long g_discordStartEpoch = 0;
static bool g_discordHasTimestamp = false;

static std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if ((unsigned char)c < 0x20)
            {
                char buf[8];
                wsprintfA(buf, "\\u%04x", (unsigned char)c);
                out += buf;
            }
            else out += c;
        }
    }
    return out;
}

static bool DiscordWriteFrame(int opcode, const std::string& json)
{
    if (g_discordPipe == INVALID_HANDLE_VALUE) return false;
    u32 op = (u32)opcode;
    u32 len = (u32)json.size();
    DWORD written = 0;
    if (!WriteFile(g_discordPipe, &op, 4, &written, nullptr) || written != 4) return false;
    if (!WriteFile(g_discordPipe, &len, 4, &written, nullptr) || written != 4) return false;
    if (len > 0)
    {
        if (!WriteFile(g_discordPipe, json.data(), len, &written, nullptr) || written != len) return false;
    }
    return true;
}

static bool DiscordReadFrame(std::string& outJson, DWORD waitMs)
{
    DWORD avail = 0;
    if (!PeekNamedPipe(g_discordPipe, nullptr, 0, nullptr, &avail, nullptr)) return false;
    if (avail < 8)
    {
        Sleep(waitMs);
        if (!PeekNamedPipe(g_discordPipe, nullptr, 0, nullptr, &avail, nullptr)) return false;
        if (avail < 8) return false;
    }
    u32 op = 0, len = 0;
    DWORD readBytes = 0;
    if (!ReadFile(g_discordPipe, &op, 4, &readBytes, nullptr) || readBytes != 4) return false;
    if (!ReadFile(g_discordPipe, &len, 4, &readBytes, nullptr) || readBytes != 4) return false;
    outJson.resize(len);
    if (len > 0)
    {
        if (!ReadFile(g_discordPipe, &outJson[0], len, &readBytes, nullptr) || readBytes != len) return false;
    }
    return true;
}

static bool DiscordConnect()
{
    for (int i = 0; i < 10; i++)
    {
        wchar_t name[64];
        wsprintfW(name, L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            g_discordPipe = h;
            return true;
        }
    }
    return false;
}

static void DiscordDisconnect()
{
    if (g_discordPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_discordPipe);
        g_discordPipe = INVALID_HANDLE_VALUE;
    }
}

static bool DiscordHandshake()
{
    std::string payload = "{\"v\":1,\"client_id\":\"" + std::string(kDiscordClientId) + "\"}";
    if (!DiscordWriteFrame(0, payload)) return false;
    std::string resp;
    return DiscordReadFrame(resp, 300);
}

static std::string BuildActivityJson()
{
    static long long nonceCounter = 0;
    char nonceBuf[24];
    wsprintfA(nonceBuf, "%lld", ++nonceCounter);

    std::string details = JsonEscape(g_discordDetails);

    std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    json += std::to_string((long long)GetCurrentProcessId());
    json += ",\"activity\":{\"details\":\"" + details + "\"";

    if (g_discordHasTimestamp)
    {
        json += ",\"timestamps\":{\"start\":";
        json += std::to_string(g_discordStartEpoch);
        json += "}";
    }

    json += ",\"assets\":{\"large_image\":\"icon\",\"large_text\":\"Toast\"}";
    json += "}},\"nonce\":\"" + std::string(nonceBuf) + "\"}";
    return json;
}

DWORD WINAPI DiscordThreadProc(LPVOID)
{
    while (g_discordThreadRunning)
    {
        if (!g_discordEnabled)
        {
            if (g_discordPipe != INVALID_HANDLE_VALUE) DiscordDisconnect();
            Sleep(500);
            continue;
        }

        if (g_discordPipe == INVALID_HANDLE_VALUE)
        {
            if (!DiscordConnect()) { Sleep(2000); continue; }
            if (!DiscordHandshake()) { DiscordDisconnect(); Sleep(2000); continue; }
            EnterCriticalSection(&g_discordCs);
            g_discordActivityDirty = true;
            LeaveCriticalSection(&g_discordCs);
        }

        bool dirty = false;
        std::string json;
        EnterCriticalSection(&g_discordCs);
        dirty = g_discordActivityDirty;
        if (dirty) { json = BuildActivityJson(); g_discordActivityDirty = false; }
        LeaveCriticalSection(&g_discordCs);

        if (dirty)
        {
            if (!DiscordWriteFrame(1, json))
            {
                DiscordDisconnect();
                Sleep(1000);
                continue;
            }
        }

        std::string drain;
        DiscordReadFrame(drain, 0);

        Sleep(500);
    }
    DiscordDisconnect();
    return 0;
}

void StartDiscord()
{
    if (!g_discordCsInit) { InitializeCriticalSection(&g_discordCs); g_discordCsInit = true; }
    g_discordThreadRunning = true;
    g_discordThreadHandle = CreateThread(nullptr, 0, DiscordThreadProc, nullptr, 0, nullptr);
}

void StopDiscord()
{
    g_discordThreadRunning = false;
    if (g_discordThreadHandle)
    {
        WaitForSingleObject(g_discordThreadHandle, 2000);
        CloseHandle(g_discordThreadHandle);
        g_discordThreadHandle = nullptr;
    }
    if (g_discordCsInit) { DeleteCriticalSection(&g_discordCs); g_discordCsInit = false; }
}

void SetDiscordMenu()
{
    if (!g_discordCsInit) return;
    EnterCriticalSection(&g_discordCs);
    g_discordDetails = "In the Menu";
    g_discordHasTimestamp = false;
    g_discordActivityDirty = true;
    LeaveCriticalSection(&g_discordCs);
}

void SetDiscordPlaying(const std::wstring& romPath)
{
    if (!g_discordCsInit) return;
    size_t slash = romPath.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? romPath : romPath.substr(slash + 1);
    std::string narrowName = NarrowPath(name);

    EnterCriticalSection(&g_discordCs);
    g_discordDetails = "Playing - " + narrowName;
    g_discordStartEpoch = (long long)time(nullptr);
    g_discordHasTimestamp = true;
    g_discordActivityDirty = true;
    LeaveCriticalSection(&g_discordCs);
}

typedef UINT(WINAPI* TimeBeginPeriodFn)(UINT);
typedef UINT(WINAPI* TimeEndPeriodFn)(UINT);
static HMODULE g_winmm = nullptr;
static TimeBeginPeriodFn pTimeBeginPeriod = nullptr;
static TimeEndPeriodFn   pTimeEndPeriod = nullptr;

void LoadWinmmTimers()
{
    g_winmm = LoadLibraryW(L"winmm.dll");
    if (!g_winmm) return;
    pTimeBeginPeriod = (TimeBeginPeriodFn)GetProcAddress(g_winmm, "timeBeginPeriod");
    pTimeEndPeriod = (TimeEndPeriodFn)GetProcAddress(g_winmm, "timeEndPeriod");
}

void UnloadWinmmTimers()
{
    if (pTimeEndPeriod) pTimeEndPeriod(1);
    if (g_winmm) FreeLibrary(g_winmm);
    g_winmm = nullptr;
    pTimeBeginPeriod = nullptr;
    pTimeEndPeriod = nullptr;
}

typedef HANDLE(WINAPI* AvSetMmThreadCharacteristicsWFn)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* AvRevertMmThreadCharacteristicsFn)(HANDLE);

static volatile bool g_audioThreadRunning = false;
static HANDLE g_audioThreadHandle = nullptr;

static bool GetDefaultDeviceId(IMMDeviceEnumerator* enumr, std::wstring& outId)
{
    IMMDevice* dev = nullptr;
    if (FAILED(enumr->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) return false;
    LPWSTR id = nullptr;
    dev->GetId(&id);
    outId = id ? id : L"";
    if (id) CoTaskMemFree(id);
    dev->Release();
    return true;
}

DWORD WINAPI AudioThreadProc(LPVOID)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HMODULE avrt = LoadLibraryW(L"avrt.dll");
    AvSetMmThreadCharacteristicsWFn pAvSet = avrt ? (AvSetMmThreadCharacteristicsWFn)GetProcAddress(avrt, "AvSetMmThreadCharacteristicsW") : nullptr;
    AvRevertMmThreadCharacteristicsFn pAvRevert = avrt ? (AvRevertMmThreadCharacteristicsFn)GetProcAddress(avrt, "AvRevertMmThreadCharacteristics") : nullptr;
    DWORD mmcssTaskIndex = 0;
    HANDLE mmcssHandle = pAvSet ? pAvSet(L"Pro Audio", &mmcssTaskIndex) : nullptr;

    IMMDeviceEnumerator* enumr = nullptr;
    CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&enumr);

    while (g_audioThreadRunning && enumr)
    {
        std::wstring deviceId;
        if (!GetDefaultDeviceId(enumr, deviceId)) { Sleep(500); continue; }

        IMMDevice* device = nullptr;
        if (FAILED(enumr->GetDefaultAudioEndpoint(eRender, eConsole, &device))) { Sleep(500); continue; }

        IAudioClient* client = nullptr;
        HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
        device->Release();
        if (FAILED(hr) || !client) { Sleep(500); continue; }

        WAVEFORMATEX wf{};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nChannels = 1;
        wf.nSamplesPerSec = (DWORD)Apu2A03::kSampleRate;
        wf.wBitsPerSample = 16;
        wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
        wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

        REFERENCE_TIME bufDuration = 200000;
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            bufDuration, 0, &wf, nullptr);
        if (FAILED(hr)) { client->Release(); Sleep(500); continue; }

        UINT32 bufferFrames = 0;
        client->GetBufferSize(&bufferFrames);

        IAudioRenderClient* renderClient = nullptr;
        hr = client->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient);
        if (FAILED(hr)) { client->Release(); Sleep(500); continue; }

        client->Start();

        DWORD lastDeviceCheck = GetTickCount();

        for (;;)
        {
            if (!g_audioThreadRunning) break;

            DWORD now = GetTickCount();
            if (now - lastDeviceCheck > 1000)
            {
                lastDeviceCheck = now;
                std::wstring curId;
                if (GetDefaultDeviceId(enumr, curId) && curId != deviceId) break;
            }

            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) break;
            UINT32 framesAvailable = bufferFrames - padding;
            if (framesAvailable == 0) { Sleep(5); continue; }

            BYTE* data = nullptr;
            if (FAILED(renderClient->GetBuffer(framesAvailable, &data))) break;

            int16_t* out = (int16_t*)data;
            static int16_t lastSample = 0;
            for (UINT32 i = 0; i < framesAvailable; i++)
            {
                size_t r = g_bus.apu.ringRead.load(std::memory_order_relaxed);
                if (r == g_bus.apu.ringWrite.load(std::memory_order_acquire))
                {
                    lastSample = (int16_t)(lastSample * 0.9);
                    out[i] = lastSample;
                }
                else
                {
                    lastSample = g_bus.apu.ringBuffer[r];
                    out[i] = lastSample;
                    g_bus.apu.ringRead.store((r + 1) & (Apu2A03::kRingSize - 1), std::memory_order_release);
                }
            }
            renderClient->ReleaseBuffer(framesAvailable, 0);
            Sleep(5);
        }

        client->Stop();
        renderClient->Release();
        client->Release();
    }

    if (enumr) enumr->Release();
    if (mmcssHandle && pAvRevert) pAvRevert(mmcssHandle);
    if (avrt) FreeLibrary(avrt);
    CoUninitialize();
    return 0;
}

void StartAudio()
{
    g_audioThreadRunning = true;
    g_audioThreadHandle = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);
}

void StopAudio()
{
    g_audioThreadRunning = false;
    if (g_audioThreadHandle)
    {
        WaitForSingleObject(g_audioThreadHandle, 2000);
        CloseHandle(g_audioThreadHandle);
        g_audioThreadHandle = nullptr;
    }
}

void UpdateWindowTitle(HWND hwnd)
{
    std::wstring title = L"Toast";
    if (g_romLoaded)
    {
        size_t slash = g_romPath.find_last_of(L"\\/");
        std::wstring name = (slash == std::wstring::npos) ? g_romPath : g_romPath.substr(slash + 1);
        title += L" - " + name;
    }
    SetWindowTextW(hwnd, title.c_str());
}

bool LoadRom(HWND hwnd, const std::wstring& path)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string narrow(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &narrow[0], len, nullptr, nullptr);
    if (!narrow.empty() && narrow.back() == '\0') narrow.pop_back();

    Cartridge newCart;
    if (!newCart.LoadFromFile(narrow))
    {
        ShowMsg(newCart.lastError.c_str(), "Failed to load ROM", MB_OK | MB_ICONERROR);
        return false;
    }

    g_cart = std::move(newCart);
    g_bus.ConnectCartridge(&g_cart);
    g_bus.Reset();
    g_cpu.Reset();
    g_bus.totalCycles = 0;
    GameGenieClearAll();

    if (!g_cart.lastError.empty())
        ShowMsg(g_cart.lastError.c_str(), "Notice", MB_OK | MB_ICONWARNING);

    g_romLoaded = true;
    g_running = true;
    g_romPath = path;
    UpdateWindowTitle(hwnd);
    SetDiscordPlaying(path);
    return true;
}

void OpenFileDialog(HWND hwnd)
{
    wchar_t fileBuf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"NES ROMs (*.nes)\0*.nes\0All Files\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open NES ROM";

    if (GetOpenFileNameW(&ofn))
    {
        g_running = false;
        LoadRom(hwnd, fileBuf);
    }
}

void PollInput()
{
    if (!g_romLoaded) return;
    for (int p = 0; p < 2; p++)
    {
        u8 s = 0;
        if (GetAsyncKeyState(g_keys[p].A) & 0x8000)      s |= 0x01;
        if (GetAsyncKeyState(g_keys[p].B) & 0x8000)      s |= 0x02;
        if (GetAsyncKeyState(g_keys[p].Select) & 0x8000) s |= 0x04;
        if (GetAsyncKeyState(g_keys[p].Start) & 0x8000)  s |= 0x08;
        if (GetAsyncKeyState(g_keys[p].Up) & 0x8000)     s |= 0x10;
        if (GetAsyncKeyState(g_keys[p].Down) & 0x8000)   s |= 0x20;
        if (GetAsyncKeyState(g_keys[p].Left) & 0x8000)   s |= 0x40;
        if (GetAsyncKeyState(g_keys[p].Right) & 0x8000)  s |= 0x80;
        g_bus.controllerState[p] = s;
    }
}

void RunOneFrame()
{
    if (!g_running) return;
    g_bus.ppu.frameComplete = false;
    int guard = 400000;
    while (!g_bus.ppu.frameComplete && guard-- > 0)
    {
        g_bus.Clock();
    }
    for (int i = 0; i < kNesW * kNesH; i++)
        g_frameBuf[i] = g_bus.ppu.screen[i];
}

void PaintFrame(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    RECT rc; GetClientRect(hwnd, &rc);
    int destW = rc.right - rc.left;
    int destH = rc.bottom - rc.top;

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc,
        0, 0, destW, destH,
        0, 0, kNesW, kNesH,
        g_frameBuf.data(), &g_bmi,
        DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(hwnd, hdc);
}

static HWND g_inputConfigHwnd = nullptr;
static KeyBindings g_tempKeys[2];
static int g_listeningIndex = -1;
static bool g_inputConflict[16]{};
static const int ID_REBIND_BASE = 100;
static const int ID_SAVE = 200;
static const int ID_CANCEL = 201;
static const int ID_TIMER_KEYPOLL = 1;

void RefreshRebindButtonText(HWND hwnd, int flatIndex)
{
    HWND btn = GetDlgItem(hwnd, ID_REBIND_BASE + flatIndex);
    std::wstring text = g_listeningIndex == flatIndex ? L"Press a key..." : KeyName(*BindingSlot(g_tempKeys, flatIndex));
    SetWindowTextW(btn, text.c_str());
}

void RecomputeInputConflicts(HWND hwnd)
{
    for (int i = 0; i < 16; i++) g_inputConflict[i] = false;
    for (int i = 0; i < 16; i++)
    {
        int vi = *BindingSlot(g_tempKeys, i);
        if (vi <= 0) continue;
        int playerI = i / 8;
        for (int j = i + 1; j < 16; j++)
        {
            if (j / 8 != playerI) continue;
            int vj = *BindingSlot(g_tempKeys, j);
            if (vi == vj) { g_inputConflict[i] = true; g_inputConflict[j] = true; }
        }
    }
    for (int i = 0; i < 16; i++)
        InvalidateRect(GetDlgItem(hwnd, ID_REBIND_BASE + i), nullptr, TRUE);
}

LRESULT CALLBACK InputConfigWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_tempKeys[0] = g_keys[0];
        g_tempKeys[1] = g_keys[1];

        const int rowH = 32, labelW = 90, btnW = 150, padX = 16, padY = 32;

        CreateWindowW(L"STATIC", L"Player 1", WS_CHILD | WS_VISIBLE | SS_LEFT, padX, 8, labelW, 20, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"STATIC", L"Player 2", WS_CHILD | WS_VISIBLE | SS_LEFT, padX + 260, 8, labelW, 20, hwnd, nullptr, nullptr, nullptr);

        for (int i = 0; i < 16; i++)
        {
            int p = i / 8;
            int b = i % 8;
            int offsetX = p * 260;
            int y = padY + b * rowH;

            CreateWindowW(L"STATIC", kButtonNames[b], WS_CHILD | WS_VISIBLE | SS_LEFT,
                padX + offsetX, y + 5, labelW, 20, hwnd, nullptr, nullptr, nullptr);
            HWND btn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                padX + labelW + offsetX, y, btnW, 24, hwnd, (HMENU)(UINT_PTR)(ID_REBIND_BASE + i), nullptr, nullptr);
            (void)btn;
        }

        int bottomY = padY + 8 * rowH + 8;
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, bottomY, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_SAVE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            280, bottomY, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_CANCEL, nullptr, nullptr);

        for (int i = 0; i < 16; i++) RefreshRebindButtonText(hwnd, i);
        RecomputeInputConflicts(hwnd);
        SetTimer(hwnd, ID_TIMER_KEYPOLL, 40, nullptr);
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= ID_REBIND_BASE && id < ID_REBIND_BASE + 16)
        {
            g_listeningIndex = id - ID_REBIND_BASE;
            SetFocus(hwnd);
            for (int i = 0; i < 16; i++) RefreshRebindButtonText(hwnd, i);
        }
        else if (id == ID_SAVE)
        {
            g_keys[0] = g_tempKeys[0];
            g_keys[1] = g_tempKeys[1];
            SaveKeyBindings();
            DestroyWindow(hwnd);
        }
        else if (id == ID_CANCEL)
        {
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_KEYPOLL && g_listeningIndex >= 0)
        {
            for (int vk = 0x08; vk <= 0xFE; vk++)
            {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
                if (!(GetAsyncKeyState(vk) & 0x8000)) continue;

                if (vk != VK_ESCAPE)
                    *BindingSlot(g_tempKeys, g_listeningIndex) = vk;

                g_listeningIndex = -1;
                for (int i = 0; i < 16; i++) RefreshRebindButtonText(hwnd, i);
                RecomputeInputConflicts(hwnd);
                break;
            }
        }
        return 0;

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
        if (dis->CtlID >= ID_REBIND_BASE && dis->CtlID < ID_REBIND_BASE + 16)
        {
            int idx = dis->CtlID - ID_REBIND_BASE;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            HBRUSH bg = CreateSolidBrush(g_inputConflict[idx] ? RGB(220, 60, 60) : GetSysColor(COLOR_BTNFACE));
            FillRect(dis->hDC, &dis->rcItem, bg);
            DeleteObject(bg);

            DrawEdge(dis->hDC, &dis->rcItem, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

            wchar_t text[64];
            GetWindowTextW(dis->hwndItem, text, 64);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, g_inputConflict[idx] ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));
            RECT rc = dis->rcItem;
            DrawTextW(dis->hDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_KEYPOLL);
        g_inputConfigHwnd = nullptr;
        g_listeningIndex = -1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenInputConfig(HWND owner)
{
    if (g_inputConfigHwnd)
    {
        SetForegroundWindow(g_inputConfigHwnd);
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = InputConfigWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = kInputConfigClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 540, 360 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);

    g_inputConfigHwnd = CreateWindowW(kInputConfigClass, L"Configure Controls",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);

    ShowWindow(g_inputConfigHwnd, SW_SHOW);
}

static HWND g_optionsHwnd = nullptr;
static const wchar_t* kOptionsClass = L"NesEmuOptionsClass";
static const int ID_OPT_SCALE_BASE = 400;
static const int ID_OPT_DISCORD = 410;
static const int ID_OPT_BACKGROUND = 411;
static const int ID_OPT_SAVE = 412;
static const int ID_OPT_CANCEL = 413;

LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"Window Size", WS_CHILD | WS_VISIBLE, 16, 12, 120, 20, hwnd, nullptr, nullptr, nullptr);
        const wchar_t* labels[5] = { L"1x", L"2x", L"3x", L"4x", L"5x" };
        for (int i = 0; i < 5; i++)
        {
            DWORD style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0);
            HWND rb = CreateWindowW(L"BUTTON", labels[i], style, 16 + i * 60, 36, 56, 22, hwnd, (HMENU)(UINT_PTR)(ID_OPT_SCALE_BASE + i), nullptr, nullptr);
            if (g_scale == i + 1) SendMessageW(rb, BM_SETCHECK, BST_CHECKED, 0);
        }

        HWND discordCb = CreateWindowW(L"BUTTON", L"Enable Discord Rich Presence", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            16, 72, 280, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_DISCORD, nullptr, nullptr);
        SendMessageW(discordCb, BM_SETCHECK, g_discordEnabled ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND bgCb = CreateWindowW(L"BUTTON", L"Continue emulating when window is not focused", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            16, 100, 300, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_BACKGROUND, nullptr, nullptr);
        SendMessageW(bgCb, BM_SETCHECK, g_runInBackground ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 140, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_OPT_SAVE, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 200, 140, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_OPT_CANCEL, nullptr, nullptr);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_OPT_SAVE)
        {
            for (int i = 0; i < 5; i++)
            {
                HWND rb = GetDlgItem(hwnd, ID_OPT_SCALE_BASE + i);
                if (SendMessageW(rb, BM_GETCHECK, 0, 0) == BST_CHECKED)
                {
                    g_scale = i + 1;
                }
            }
            g_discordEnabled = SendMessageW(GetDlgItem(hwnd, ID_OPT_DISCORD), BM_GETCHECK, 0, 0) == BST_CHECKED;
            g_runInBackground = SendMessageW(GetDlgItem(hwnd, ID_OPT_BACKGROUND), BM_GETCHECK, 0, 0) == BST_CHECKED;

            SaveOptions();

            HWND owner = GetWindow(hwnd, GW_OWNER);
            if (owner)
            {
                RECT wr{ 0, 0, kNesW * g_scale, kNesH * g_scale };
                AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, TRUE);
                SetWindowPos(owner, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER);
            }

            DestroyWindow(hwnd);
        }
        else if (LOWORD(wParam) == ID_OPT_CANCEL)
        {
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        g_optionsHwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenOptions(HWND owner)
{
    if (g_optionsHwnd) { SetForegroundWindow(g_optionsHwnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = OptionsWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = kOptionsClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 340, 210 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);
    g_optionsHwnd = CreateWindowW(kOptionsClass, L"Options",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(g_optionsHwnd, SW_SHOW);
}

static u32 Crc32(const u8* data, size_t len)
{
    static u32 table[256];
    static bool init = false;
    if (!init)
    {
        for (u32 i = 0; i < 256; i++)
        {
            u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    u32 crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

static bool WriteZipStored(const std::wstring& path, const std::string& entryName, const std::vector<u8>& data)
{
    std::ofstream f(NarrowPath(path).c_str(), std::ios::binary);
    if (!f.is_open()) return false;

    u32 crc = Crc32(data.data(), data.size());
    u32 size = (u32)data.size();
    u16 nameLen = (u16)entryName.size();
    u32 localHeaderOffset = 0;
    u32 sig = 0x04034b50;
    u16 version = 20, flags = 0, method = 0, modTime = 0, modDate = 0;
    u16 extraLen = 0;

    f.write((char*)&sig, 4);
    f.write((char*)&version, 2);
    f.write((char*)&flags, 2);
    f.write((char*)&method, 2);
    f.write((char*)&modTime, 2);
    f.write((char*)&modDate, 2);
    f.write((char*)&crc, 4);
    f.write((char*)&size, 4);
    f.write((char*)&size, 4);
    f.write((char*)&nameLen, 2);
    f.write((char*)&extraLen, 2);
    f.write(entryName.data(), nameLen);
    if (!data.empty()) f.write((char*)data.data(), data.size());

    u32 centralOffset = (u32)f.tellp();

    u32 cdSig = 0x02014b50;
    u16 versionMadeBy = 20;
    f.write((char*)&cdSig, 4);
    f.write((char*)&versionMadeBy, 2);
    f.write((char*)&version, 2);
    f.write((char*)&flags, 2);
    f.write((char*)&method, 2);
    f.write((char*)&modTime, 2);
    f.write((char*)&modDate, 2);
    f.write((char*)&crc, 4);
    f.write((char*)&size, 4);
    f.write((char*)&size, 4);
    f.write((char*)&nameLen, 2);
    f.write((char*)&extraLen, 2);
    u16 commentLen = 0;
    f.write((char*)&commentLen, 2);
    u16 diskNum = 0;
    f.write((char*)&diskNum, 2);
    u16 intAttr = 0;
    f.write((char*)&intAttr, 2);
    u32 extAttr = 0;
    f.write((char*)&extAttr, 4);
    f.write((char*)&localHeaderOffset, 4);
    f.write(entryName.data(), nameLen);

    u32 cdSize = (u32)((u32)f.tellp() - centralOffset);

    u32 eocdSig = 0x06054b50;
    u16 diskNum2 = 0, cdStartDisk = 0;
    u16 cdEntriesThisDisk = 1, cdEntriesTotal = 1;
    u16 zipCommentLen = 0;
    f.write((char*)&eocdSig, 4);
    f.write((char*)&diskNum2, 2);
    f.write((char*)&cdStartDisk, 2);
    f.write((char*)&cdEntriesThisDisk, 2);
    f.write((char*)&cdEntriesTotal, 2);
    f.write((char*)&cdSize, 4);
    f.write((char*)&centralOffset, 4);
    f.write((char*)&zipCommentLen, 2);

    return true;
}

static bool ReadZipStoredFirstEntry(const std::wstring& path, std::vector<u8>& outData)
{
    std::ifstream f(NarrowPath(path).c_str(), std::ios::binary);
    if (!f.is_open()) return false;

    u32 sig = 0;
    f.read((char*)&sig, 4);
    if (sig != 0x04034b50) return false;

    u16 version, flags, method, modTime, modDate, nameLen, extraLen;
    u32 crc, compSize, uncompSize;
    f.read((char*)&version, 2);
    f.read((char*)&flags, 2);
    f.read((char*)&method, 2);
    f.read((char*)&modTime, 2);
    f.read((char*)&modDate, 2);
    f.read((char*)&crc, 4);
    f.read((char*)&compSize, 4);
    f.read((char*)&uncompSize, 4);
    f.read((char*)&nameLen, 2);
    f.read((char*)&extraLen, 2);
    f.seekg(nameLen + extraLen, std::ios::cur);

    if (method != 0) return false;

    outData.resize(compSize);
    if (compSize > 0) f.read((char*)outData.data(), compSize);
    return true;
}

std::wstring GetStateDir()
{
    std::wstring dir = GetConfigDir() + L"\\states";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring GetRomBaseName()
{
    size_t slash = g_romPath.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? g_romPath : g_romPath.substr(slash + 1);
    size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    return name;
}

std::wstring GetStateFilePath(int slot)
{
    wchar_t buf[24];
    wsprintfW(buf, L"_slot%d.zip", slot + 1);
    return GetStateDir() + L"\\" + GetRomBaseName() + buf;
}

bool StateBufferMatchesLoadedRom(const std::vector<u8>& data)
{
    if (data.size() < 4) return false;
    u32 savedCrc = (u32)data[0] | ((u32)data[1] << 8) | ((u32)data[2] << 16) | ((u32)data[3] << 24);
    return g_cart.IsLoaded() && savedCrc == g_cart.GetRomCrc();
}

void SaveStateToSlot(int slot)
{
    if (!g_romLoaded) return;
    StateWriter w;
    g_bus.SaveState(w);
    if (!WriteZipStored(GetStateFilePath(slot), "state.bin", w.buf))
        ShowMsg("Failed to save state.", "Save State", MB_OK | MB_ICONERROR);
}

void LoadStateFromSlot(int slot)
{
    if (!g_romLoaded) return;
    std::vector<u8> data;
    if (!ReadZipStoredFirstEntry(GetStateFilePath(slot), data))
    {
        ShowMsg("No save state in that slot.", "Load State", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!StateBufferMatchesLoadedRom(data))
    {
        ShowMsg("This save state was made for a different ROM and cannot be loaded.", "Load State", MB_OK | MB_ICONWARNING);
        return;
    }

    StateWriter backup;
    g_bus.SaveState(backup);

    StateReader r(data.data(), data.size());
    g_bus.LoadState(r);
    if (!r.ok)
    {
        StateReader rb(backup.buf.data(), backup.buf.size());
        g_bus.LoadState(rb);
        ShowMsg("Save state is corrupt.", "Load State", MB_OK | MB_ICONERROR);
    }
}

void SaveStateToZipDialog(HWND hwnd)
{
    if (!g_romLoaded) return;
    wchar_t fileBuf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Save State (*.zip)\0*.zip\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"zip";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = L"Save State As";
    if (!GetSaveFileNameW(&ofn)) return;

    StateWriter w;
    g_bus.SaveState(w);
    if (!WriteZipStored(fileBuf, "state.bin", w.buf))
        ShowMsg("Failed to save state.", "Save State", MB_OK | MB_ICONERROR);
}

void LoadStateFromZipDialog(HWND hwnd)
{
    if (!g_romLoaded) return;
    wchar_t fileBuf[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Save State (*.zip)\0*.zip\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Load State";
    if (!GetOpenFileNameW(&ofn)) return;

    std::vector<u8> data;
    if (!ReadZipStoredFirstEntry(fileBuf, data))
    {
        ShowMsg("Could not read that file.", "Load State", MB_OK | MB_ICONERROR);
        return;
    }
    if (!StateBufferMatchesLoadedRom(data))
    {
        ShowMsg("This save state was made for a different ROM and cannot be loaded.", "Load State", MB_OK | MB_ICONWARNING);
        return;
    }

    StateWriter backup;
    g_bus.SaveState(backup);

    StateReader r(data.data(), data.size());
    g_bus.LoadState(r);
    if (!r.ok)
    {
        StateReader rb(backup.buf.data(), backup.buf.size());
        g_bus.LoadState(rb);
        ShowMsg("Save state is corrupt.", "Load State", MB_OK | MB_ICONERROR);
    }
}

static HWND g_hexEditorHwnd = nullptr;
static const wchar_t* kHexEditorClass = L"NesEmuHexEditorClass";
static HFONT g_hexFont = nullptr;
static int g_hexTopRow = 0;
static int g_hexCellW = 0, g_hexCellH = 0;
static int g_hexSelAddr = 0;
static int g_hexNibble = -1;
static const int ID_HEX_GOTO_EDIT = 300;
static const int ID_HEX_GOTO_BTN = 301;
static const int ID_HEX_TIMER = 2;
static const int kHexBytesPerRow = 16;
static const int kHexTotalRows = 65536 / kHexBytesPerRow;
static const int kHexHeaderY = 28;

void HexEditorEnsureFont(HDC hdc)
{
    if (g_hexFont) return;
    g_hexFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    HFONT old = (HFONT)SelectObject(hdc, g_hexFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    g_hexCellW = tm.tmAveCharWidth;
    g_hexCellH = tm.tmHeight + tm.tmExternalLeading + 2;
    SelectObject(hdc, old);
}

int HexRowsVisible(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int usable = (rc.bottom - rc.top) - kHexHeaderY;
    if (g_hexCellH <= 0) return 1;
    int rows = usable / g_hexCellH;
    return rows < 1 ? 1 : rows;
}

void HexEditorPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HexEditorEnsureFont(hdc);
    HFONT old = (HFONT)SelectObject(hdc, g_hexFont);
    SetBkMode(hdc, TRANSPARENT);

    RECT rc; GetClientRect(hwnd, &rc);
    HBRUSH bg = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(hdc, &rc, bg);

    int visibleRows = HexRowsVisible(hwnd);

    int addrX = 8;
    int hexX = addrX + 9 * g_hexCellW;
    int asciiX = hexX + kHexBytesPerRow * 3 * g_hexCellW + g_hexCellW;

    for (int r = 0; r < visibleRows; r++)
    {
        int row = g_hexTopRow + r;
        if (row >= kHexTotalRows) break;
        int y = kHexHeaderY + r * g_hexCellH;
        int baseAddr = row * kHexBytesPerRow;

        wchar_t addrBuf[16];
        wsprintfW(addrBuf, L"%04X:", baseAddr);
        TextOutW(hdc, addrX, y, addrBuf, (int)wcslen(addrBuf));

        wchar_t asciiBuf[kHexBytesPerRow + 1];
        for (int c = 0; c < kHexBytesPerRow; c++)
        {
            int addr = baseAddr + c;
            if (addr > 0xFFFF) { asciiBuf[c] = L' '; continue; }
            u8 val = g_bus.CpuRead((u16)addr);

            if (addr == g_hexSelAddr)
            {
                RECT sel{ hexX + c * 3 * g_hexCellW, y, hexX + c * 3 * g_hexCellW + 2 * g_hexCellW, y + g_hexCellH };
                HBRUSH hl = CreateSolidBrush(RGB(200, 220, 255));
                FillRect(hdc, &sel, hl);
                DeleteObject(hl);
            }
            wchar_t byteBuf[3];
            wsprintfW(byteBuf, L"%02X", val);
            TextOutW(hdc, hexX + c * 3 * g_hexCellW, y, byteBuf, 2);

            asciiBuf[c] = (val >= 0x20 && val < 0x7F) ? (wchar_t)val : L'.';
        }
        asciiBuf[kHexBytesPerRow] = 0;
        TextOutW(hdc, asciiX, y, asciiBuf, kHexBytesPerRow);
    }

    SelectObject(hdc, old);
    EndPaint(hwnd, &ps);
}

void HexEditorScrollTo(int addr)
{
    int row = addr / kHexBytesPerRow;
    g_hexTopRow = row - 4;
    if (g_hexTopRow < 0) g_hexTopRow = 0;
    if (g_hexTopRow > kHexTotalRows - 1) g_hexTopRow = kHexTotalRows - 1;
}

LRESULT CALLBACK HexEditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CreateWindowW(L"STATIC", L"Go to:", WS_CHILD | WS_VISIBLE, 8, 4, 50, 20, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"EDIT", L"0000", WS_CHILD | WS_VISIBLE | WS_BORDER, 60, 2, 60, 22, hwnd, (HMENU)(UINT_PTR)ID_HEX_GOTO_EDIT, nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 128, 2, 40, 22, hwnd, (HMENU)(UINT_PTR)ID_HEX_GOTO_BTN, nullptr, nullptr);

        SetScrollRange(hwnd, SB_VERT, 0, kHexTotalRows - 1, TRUE);
        SetTimer(hwnd, ID_HEX_TIMER, 200, nullptr);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_HEX_GOTO_BTN)
        {
            wchar_t buf[16];
            GetDlgItemTextW(hwnd, ID_HEX_GOTO_EDIT, buf, 16);
            int addr = (int)wcstoul(buf, nullptr, 16) & 0xFFFF;
            HexEditorScrollTo(addr);
            g_hexSelAddr = addr;
            SetScrollPos(hwnd, SB_VERT, g_hexTopRow, TRUE);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        int addrX = 8;
        int hexX = addrX + 9 * g_hexCellW;
        if (my >= kHexHeaderY && mx >= hexX && g_hexCellW > 0 && g_hexCellH > 0)
        {
            int r = (my - kHexHeaderY) / g_hexCellH;
            int c = (mx - hexX) / (3 * g_hexCellW);
            if (c >= 0 && c < kHexBytesPerRow)
            {
                int addr = (g_hexTopRow + r) * kHexBytesPerRow + c;
                if (addr >= 0 && addr <= 0xFFFF)
                {
                    g_hexSelAddr = addr;
                    g_hexNibble = -1;
                    SetFocus(hwnd);
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
        }
        return 0;
    }

    case WM_CHAR:
    {
        wchar_t ch = (wchar_t)wParam;
        int digit = -1;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;

        if (digit >= 0 && g_hexSelAddr >= 0 && g_hexSelAddr <= 0xFFFF)
        {
            if (g_hexNibble < 0)
            {
                g_hexNibble = digit;
            }
            else
            {
                u8 val = (u8)((g_hexNibble << 4) | digit);
                g_bus.CpuWrite((u16)g_hexSelAddr, val);
                g_hexNibble = -1;
                if (g_hexSelAddr < 0xFFFF) g_hexSelAddr++;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_UP && g_hexSelAddr >= kHexBytesPerRow) { g_hexSelAddr -= kHexBytesPerRow; g_hexNibble = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_DOWN && g_hexSelAddr <= 0xFFFF - kHexBytesPerRow) { g_hexSelAddr += kHexBytesPerRow; g_hexNibble = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_LEFT && g_hexSelAddr > 0) { g_hexSelAddr--; g_hexNibble = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_RIGHT && g_hexSelAddr < 0xFFFF) { g_hexSelAddr++; g_hexNibble = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;

    case WM_VSCROLL:
    {
        int pos = g_hexTopRow;
        switch (LOWORD(wParam))
        {
        case SB_LINEUP: pos--; break;
        case SB_LINEDOWN: pos++; break;
        case SB_PAGEUP: pos -= HexRowsVisible(hwnd); break;
        case SB_PAGEDOWN: pos += HexRowsVisible(hwnd); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: pos = HIWORD(wParam); break;
        default: break;
        }
        if (pos < 0) pos = 0;
        if (pos > kHexTotalRows - 1) pos = kHexTotalRows - 1;
        g_hexTopRow = pos;
        SetScrollPos(hwnd, SB_VERT, pos, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_HEX_TIMER) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_PAINT:
        HexEditorPaint(hwnd);
        return 0;

    case WM_SIZE:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_HEX_TIMER);
        if (g_hexFont) { DeleteObject(g_hexFont); g_hexFont = nullptr; }
        g_hexEditorHwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenHexEditor(HWND owner)
{
    if (g_hexEditorHwnd) { SetForegroundWindow(g_hexEditorHwnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = HexEditorWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = kHexEditorClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 560, 480 };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    g_hexEditorHwnd = CreateWindowW(kHexEditorClass, L"Hex Editor",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(g_hexEditorHwnd, SW_SHOW);
}

struct GgEntry
{
    std::wstring code;
    bool enabled = true;
    u16  addr = 0;
    u8   value = 0;
    u8   compare = 0;
    bool hasCompare = false;
};

static std::vector<GgEntry> g_ggList;
static HWND    g_ggHwnd = nullptr;
static WNDPROC g_ggEditOldProc = nullptr;
static const wchar_t* kGameGenieClass = L"NesEmuGameGenieClass";

static const int ID_GG_EDIT   = 400;
static const int ID_GG_ADD    = 401;
static const int ID_GG_LIST   = 402;
static const int ID_GG_TOGGLE = 403;
static const int ID_GG_REMOVE = 404;
static const int ID_GG_CLEAR  = 405;
static const int ID_GG_STATUS = 406;

static bool GgDecode(const std::wstring& text, GgEntry& out)
{
    static const wchar_t kLetters[] = L"APZLGITYEOXUKSVN";
    int n[8] = {};
    int len = 0;
    std::wstring canon;

    for (wchar_t ch : text)
    {
        if (ch == L' ' || ch == L'-') continue;
        if (ch >= L'a' && ch <= L'z') ch = (wchar_t)(ch - L'a' + L'A');

        int v = -1;
        for (int i = 0; i < 16; i++)
            if (kLetters[i] == ch) { v = i; break; }

        if (v < 0 || len >= 8) return false;
        n[len++] = v;
        canon += ch;
    }
    if (len != 6 && len != 8) return false;

    out.addr = (u16)(0x8000
        | ((n[3] & 7) << 12)
        | ((n[5] & 7) << 8) | ((n[4] & 8) << 8)
        | ((n[2] & 7) << 4) | ((n[1] & 8) << 4)
        |  (n[4] & 7)       |  (n[3] & 8));

    out.value = (u8)(((n[1] & 7) << 4) | ((n[0] & 8) << 4) | (n[0] & 7)
        | (len == 6 ? (n[5] & 8) : (n[7] & 8)));

    out.hasCompare = (len == 8);
    out.compare = 0;
    if (len == 8)
        out.compare = (u8)(((n[7] & 7) << 4) | ((n[6] & 8) << 4) | (n[6] & 7) | (n[5] & 8));

    out.code = canon;
    out.enabled = true;
    return true;
}

static void GgApply()
{
    g_bus.gameGenie.clear();
    for (const auto& e : g_ggList)
        if (e.enabled)
            g_bus.gameGenie.push_back({ e.addr, e.value, e.compare, e.hasCompare });
}

static void GgRefreshList(int select = -1)
{
    if (!g_ggHwnd) return;
    HWND list = GetDlgItem(g_ggHwnd, ID_GG_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);

    for (const auto& e : g_ggList)
    {
        wchar_t line[128];
        if (e.hasCompare)
            wsprintfW(line, L"[%s]  %s    %04X:%02X (if %02X)", e.enabled ? L"x" : L" ", e.code.c_str(), e.addr, e.value, e.compare);
        else
            wsprintfW(line, L"[%s]  %s    %04X:%02X", e.enabled ? L"x" : L" ", e.code.c_str(), e.addr, e.value);
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)line);
    }
    if (select >= 0 && select < (int)g_ggList.size())
        SendMessageW(list, LB_SETCURSEL, select, 0);
}

void GameGenieClearAll()
{
    g_ggList.clear();
    GgApply();
    GgRefreshList();
}

static LRESULT CALLBACK GgEditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_CHAR && w == VK_RETURN)
    {
        PostMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(ID_GG_ADD, BN_CLICKED), 0);
        return 0;
    }
    return CallWindowProcW(g_ggEditOldProc, h, m, w, l);
}

LRESULT CALLBACK GameGenieWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        g_ggHwnd = hwnd;
		HINSTANCE hi = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND ctl[8];
        ctl[0] = CreateWindowW(L"STATIC", L"Code:", WS_CHILD | WS_VISIBLE, 10, 14, 40, 18, hwnd, nullptr, hi, nullptr);
        ctl[1] = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL | ES_UPPERCASE,
                               55, 10, 180, 24, hwnd, (HMENU)(UINT_PTR)ID_GG_EDIT, hi, nullptr);
        ctl[2] = CreateWindowW(L"BUTTON", L"Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               245, 9, 70, 26, hwnd, (HMENU)(UINT_PTR)ID_GG_ADD, hi, nullptr);
        ctl[3] = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                               10, 44, 395, 200, hwnd, (HMENU)(UINT_PTR)ID_GG_LIST, hi, nullptr);
        ctl[4] = CreateWindowW(L"BUTTON", L"Enable / Disable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               10, 252, 120, 26, hwnd, (HMENU)(UINT_PTR)ID_GG_TOGGLE, hi, nullptr);
        ctl[5] = CreateWindowW(L"BUTTON", L"Remove", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               140, 252, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_GG_REMOVE, hi, nullptr);
        ctl[6] = CreateWindowW(L"BUTTON", L"Clear All", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               230, 252, 80, 26, hwnd, (HMENU)(UINT_PTR)ID_GG_CLEAR, hi, nullptr);
        ctl[7] = CreateWindowW(L"STATIC", L"Enter a 6 or 8 letter code. Double-click a code to toggle it.",
                               WS_CHILD | WS_VISIBLE, 10, 288, 395, 20, hwnd, (HMENU)(UINT_PTR)ID_GG_STATUS, hi, nullptr);

        for (HWND c : ctl) SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);

        SendMessageW(ctl[1], EM_LIMITTEXT, 16, 0);
        g_ggEditOldProc = (WNDPROC)SetWindowLongPtrW(ctl[1], GWLP_WNDPROC, (LONG_PTR)GgEditProc);

        GgRefreshList();
        SetFocus(ctl[1]);
        return 0;
    }

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        int code = HIWORD(wParam);
        HWND list = GetDlgItem(hwnd, ID_GG_LIST);

        if (id == ID_GG_ADD)
        {
            wchar_t buf[64] = L"";
            GetDlgItemTextW(hwnd, ID_GG_EDIT, buf, 64);

            GgEntry e;
            if (!GgDecode(buf, e))
            {
                SetDlgItemTextW(hwnd, ID_GG_STATUS, L"Invalid code. Use 6 or 8 letters from: A P Z L G I T Y E O X U K S V N");
                return 0;
            }
            for (const auto& x : g_ggList)
            {
                if (x.code == e.code)
                {
                    SetDlgItemTextW(hwnd, ID_GG_STATUS, L"That code is already in the list.");
                    return 0;
                }
            }
            g_ggList.push_back(e);
            GgApply();
            GgRefreshList((int)g_ggList.size() - 1);
            SetDlgItemTextW(hwnd, ID_GG_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_GG_STATUS, L"Code added.");
            SetFocus(GetDlgItem(hwnd, ID_GG_EDIT));
        }
        else if (id == ID_GG_TOGGLE || (id == ID_GG_LIST && code == LBN_DBLCLK))
        {
            int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_ggList.size())
            {
                g_ggList[sel].enabled = !g_ggList[sel].enabled;
                GgApply();
                GgRefreshList(sel);
            }
        }
        else if (id == ID_GG_REMOVE)
        {
            int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)g_ggList.size())
            {
                g_ggList.erase(g_ggList.begin() + sel);
                GgApply();
                GgRefreshList(sel < (int)g_ggList.size() ? sel : (int)g_ggList.size() - 1);
                SetDlgItemTextW(hwnd, ID_GG_STATUS, L"Code removed.");
            }
        }
        else if (id == ID_GG_CLEAR)
        {
            GameGenieClearAll();
            SetDlgItemTextW(hwnd, ID_GG_STATUS, L"All codes cleared.");
        }
        return 0;
    }

    case WM_DESTROY:
        g_ggHwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenGameGenie(HWND owner)
{
    if (g_ggHwnd) { SetForegroundWindow(g_ggHwnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = GameGenieWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = kGameGenieClass;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 415, 318 };
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    g_ggHwnd = CreateWindowW(kGameGenieClass, L"Game Genie",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(g_ggHwnd, SW_SHOW);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_ACTIVATE:
        g_windowActive = (LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH))
        {
            g_running = false;
            LoadRom(hwnd, path);
        }
        DragFinish(drop);
        return 0;
    }

    case WM_KEYDOWN:
    if (wParam == (WPARAM)g_toastKeys.Reset && g_romLoaded && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        g_bus.Reset();
        g_cpu.Reset();
    }
    else if (wParam == (WPARAM)g_toastKeys.OpenRom && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenFileDialog(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.InputConfig && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenInputConfig(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.ToastConfig && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenToastConfig(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.Options && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenOptions(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.HexEditor && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenHexEditor(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.GameGenie && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenGameGenie(hwnd);
    }
    else if (wParam == (WPARAM)g_toastKeys.PauseResume && g_romLoaded && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        g_running = !g_running;
    }
    return 0;

    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id == ID_FILE_OPEN) OpenFileDialog(hwnd);
        else if (id == ID_INPUT_CONFIG) OpenInputConfig(hwnd);
        else if (id == ID_TOAST_CONFIG) OpenToastConfig(hwnd);
        else if (id == ID_OPTIONS) OpenOptions(hwnd);
        else if (id == ID_HEX_EDITOR) OpenHexEditor(hwnd);
        else if (id == ID_GAME_GENIE) OpenGameGenie(hwnd);
        else if (id == ID_SAVE_STATE_ZIP) SaveStateToZipDialog(hwnd);
        else if (id == ID_LOAD_STATE_ZIP) LoadStateFromZipDialog(hwnd);
        else if (id >= ID_SAVE_SLOT_BASE && id < ID_SAVE_SLOT_BASE + 9) SaveStateToSlot(id - ID_SAVE_SLOT_BASE);
        else if (id >= ID_LOAD_SLOT_BASE && id < ID_LOAD_SLOT_BASE + 9) LoadStateFromSlot(id - ID_LOAD_SLOT_BASE);
        return 0;
    }

    case WM_SYSCOMMAND:
    if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        PaintFrame(hwnd);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    LoadOptions();

    g_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = kNesW;
    g_bmi.bmiHeader.biHeight = -kNesH;
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = BI_RGB;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = LoadIconW(hInstance, L"MAINICON");
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    g_fileMenu = fileMenu;
    AppendMenuW(fileMenu, MF_STRING, ID_FILE_OPEN, L"Open ROM...");
    AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);

    HMENU saveStateMenu = CreatePopupMenu();
    for (int i = 0; i < 9; i++)
    {
        wchar_t label[24];
        wsprintfW(label, L"Slot %d", i + 1);
        AppendMenuW(saveStateMenu, MF_STRING, ID_SAVE_SLOT_BASE + i, label);
    }
    AppendMenuW(saveStateMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(saveStateMenu, MF_STRING, ID_SAVE_STATE_ZIP, L"Save to File (.zip)...");
    AppendMenuW(fileMenu, MF_POPUP, (UINT_PTR)saveStateMenu, L"Save State");

    HMENU loadStateMenu = CreatePopupMenu();
    for (int i = 0; i < 9; i++)
    {
        wchar_t label[24];
        wsprintfW(label, L"Slot %d", i + 1);
        AppendMenuW(loadStateMenu, MF_STRING, ID_LOAD_SLOT_BASE + i, label);
    }
    AppendMenuW(loadStateMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(loadStateMenu, MF_STRING, ID_LOAD_STATE_ZIP, L"Load from File (.zip)...");
    AppendMenuW(fileMenu, MF_POPUP, (UINT_PTR)loadStateMenu, L"Load State");

    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)fileMenu, L"File");

    HMENU inputMenu = CreatePopupMenu();
    g_inputMenu = inputMenu;
    AppendMenuW(inputMenu, MF_STRING, ID_INPUT_CONFIG, L"Configure NES...");
    AppendMenuW(inputMenu, MF_STRING, ID_TOAST_CONFIG, L"Configure Toast...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)inputMenu, L"Input");

    HMENU optionsMenu = CreatePopupMenu();
    g_optionsMenu = optionsMenu;
    AppendMenuW(optionsMenu, MF_STRING, ID_OPTIONS, L"Preferences...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)optionsMenu, L"Options");

    HMENU toolsMenu = CreatePopupMenu();
    g_toolsMenu = toolsMenu;
    AppendMenuW(toolsMenu, MF_STRING, ID_HEX_EDITOR, L"Hex Editor...");
    AppendMenuW(toolsMenu, MF_STRING, ID_GAME_GENIE, L"Game Genie...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)toolsMenu, L"Tools");

    RECT wr{ 0, 0, kNesW * g_scale, kNesH * g_scale };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, TRUE);

    HWND hwnd = CreateWindowW(kWindowClass, L"Toast",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right - wr.left, wr.bottom - wr.top,
    nullptr, menuBar, hInstance, nullptr);

    g_mainHwnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    g_bus.cpu = &g_cpu;
    g_cpu.ConnectBus(&g_bus);

    StartDiscord();
    SetDiscordMenu();

    LoadKeyBindings();
    LoadToastBindings();
    RebuildMenuAccelerators();

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1) LoadRom(hwnd, argv[1]);
    if (argv) LocalFree(argv);

    LoadWinmmTimers();
    if (pTimeBeginPeriod) pTimeBeginPeriod(1);

    StartAudio();

    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    const double frameTime = 1.0 / 60.0;

    MSG msg{};
    bool quit = false;
    while (!quit)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) break;

        if (!g_windowActive && !g_runInBackground)
        {
            Sleep(20);
            QueryPerformanceCounter(&last);
            continue;
        }

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - last.QuadPart) / freq.QuadPart;

        if (elapsed >= frameTime)
        {
            if (elapsed > frameTime * 4.0) last = now;
            else last.QuadPart += (LONGLONG)(frameTime * freq.QuadPart);

            PollInput();
            RunOneFrame();
            PaintFrame(hwnd);
        }
        else
        {
            double remaining = frameTime - elapsed;
            if (remaining > 0.002)
                Sleep((DWORD)((remaining - 0.001) * 1000.0));
        }
    }

    StopAudio();
    StopDiscord();
    UnloadWinmmTimers();
    return 0;
}
