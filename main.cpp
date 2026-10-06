#if defined(_MSC_VER)
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
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

static const wchar_t* MAIN_CLASS = L"NesEmuWindowClass";
static const wchar_t* PAD_CLASS = L"NesEmuInputConfigClass";
static const int NES_W = 256, NES_H = 240;
static int winScale = 3;
static bool smoothScale = false;
static bool keepAspect = false;
static bool cropOverscan = true;

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

Cartridge emuCart;
Bus       emuBus;
Cpu6502   emuCpu;

void GameGenieClearAll();

bool romLoaded = false;
bool emuRunning = false;
bool winActive = true;
std::wstring loadedRom;
HWND mainWnd = nullptr;
BITMAPINFO frameBmi{};
std::vector<u32> frameBuf(NES_W * NES_H, 0xFF000000);

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

static KeyBindings padKeys[2] = {
    { 'Z', 'X', VK_RSHIFT, VK_RETURN, VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT },
    { 'K', 'J', 'U', 'I', 'W', 'S', 'A', 'D' }
};

static const wchar_t* buttonNames[8] = { L"A", L"B", L"Select", L"Start", L"Up", L"Down", L"Left", L"Right" };

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
    int r = MessageBoxA(mainWnd, text, title, flags);
    SendMessage(mainWnd, WM_CANCELMODE, 0, 0);
    SetFocus(mainWnd);
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
        f << prefix << buttonNames[i % 8] << L"=" << *BindingSlot(padKeys, i) << L"\n";
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
            wsprintfW(expected, L"P%d_%s", (i / 8) + 1, buttonNames[i % 8]);
            if (name == expected)
            {
                *BindingSlot(padKeys, i) = val;
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

static ToastBindings hotKeys;
static const wchar_t* hotkeyNames[8] = {
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
        f << hotkeyNames[i] << L"=" << *ToastBindingSlotOf(hotKeys, i) << L"\n";
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
            if (name == hotkeyNames[i]) { *ToastBindingSlotOf(hotKeys, i) = val; break; }
        }
    }
}

HMENU menuFile = nullptr;
HMENU menuInput = nullptr;
HMENU menuPrefs = nullptr;
HMENU menuTools = nullptr;

std::wstring MenuAccelText(int vk)
{
    return L"\tCtrl+" + KeyName(vk);
}

void RebuildMenuAccelerators()
{
    if (menuFile)
        ModifyMenuW(menuFile, ID_FILE_OPEN, MF_BYCOMMAND | MF_STRING, ID_FILE_OPEN,
            (L"Open ROM..." + MenuAccelText(hotKeys.OpenRom)).c_str());

    if (menuInput)
    {
        ModifyMenuW(menuInput, ID_INPUT_CONFIG, MF_BYCOMMAND | MF_STRING, ID_INPUT_CONFIG,
            (L"Configure NES..." + MenuAccelText(hotKeys.InputConfig)).c_str());
        ModifyMenuW(menuInput, ID_TOAST_CONFIG, MF_BYCOMMAND | MF_STRING, ID_TOAST_CONFIG,
            (L"Configure Toast..." + MenuAccelText(hotKeys.ToastConfig)).c_str());
    }

    if (menuPrefs)
        ModifyMenuW(menuPrefs, ID_OPTIONS, MF_BYCOMMAND | MF_STRING, ID_OPTIONS,
            (L"Preferences..." + MenuAccelText(hotKeys.Options)).c_str());

    if (menuTools)
    {
        ModifyMenuW(menuTools, ID_HEX_EDITOR, MF_BYCOMMAND | MF_STRING, ID_HEX_EDITOR,
            (L"Hex Editor..." + MenuAccelText(hotKeys.HexEditor)).c_str());
        ModifyMenuW(menuTools, ID_GAME_GENIE, MF_BYCOMMAND | MF_STRING, ID_GAME_GENIE,
            (L"Game Genie..." + MenuAccelText(hotKeys.GameGenie)).c_str());
    }

    if (mainWnd) DrawMenuBar(mainWnd);
}

static HWND hotCfgWnd = nullptr;
static ToastBindings hotKeysTemp;
static int hotListening = -1;
static bool hotConflict[8]{};
static const int ID_TOAST_REBIND_BASE = 500;
static const int ID_TOAST_TIMER_KEYPOLL = 3;
static const wchar_t* TOASTKEYS_CLASS = L"NesEmuToastConfigClass";
static const wchar_t* hotkeyLabels[8] = {
    L"Reset", L"Open ROM", L"Configure NES", L"Configure Toast",
    L"Preferences", L"Hex Editor", L"Game Genie", L"Pause / Resume"
};

void RefreshToastRebindButtonText(HWND hwnd, int index)
{
    HWND btn = GetDlgItem(hwnd, ID_TOAST_REBIND_BASE + index);
    std::wstring text = hotListening == index ? L"Press a key..." : KeyName(*ToastBindingSlotOf(hotKeysTemp, index));
    SetWindowTextW(btn, text.c_str());
}

void RecomputeToastConflicts(HWND hwnd)
{
    for (int i = 0; i < 8; i++) hotConflict[i] = false;
    for (int i = 0; i < 8; i++)
    {
        int vi = *ToastBindingSlotOf(hotKeysTemp, i);
        if (vi <= 0) continue;
        for (int j = i + 1; j < 8; j++)
        {
            int vj = *ToastBindingSlotOf(hotKeysTemp, j);
            if (vi == vj) { hotConflict[i] = true; hotConflict[j] = true; }
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
        hotCfgWnd = hwnd;
        hotKeysTemp = hotKeys;
        const int rowH = 32, labelW = 130, btnW = 150, padX = 16, padY = 16;
        for (int i = 0; i < 8; i++)
        {
            int y = padY + i * rowH;
            CreateWindowW(L"STATIC", hotkeyLabels[i], WS_CHILD | WS_VISIBLE | SS_LEFT,
                padX, y + 5, labelW, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                padX + labelW, y, btnW, 24, hwnd, (HMENU)(UINT_PTR)(ID_TOAST_REBIND_BASE + i), nullptr, nullptr);
        }
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
            hotListening = id - ID_TOAST_REBIND_BASE;
            for (int i = 0; i < 8; i++) RefreshToastRebindButtonText(hwnd, i);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TOAST_TIMER_KEYPOLL && hotListening >= 0 && GetForegroundWindow() == hwnd)
        {
            for (int vk = 0x08; vk <= 0xFE; vk++)
            {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
                if (!(GetAsyncKeyState(vk) & 0x8000)) continue;

                if (vk != VK_ESCAPE)
                {
                    *ToastBindingSlotOf(hotKeysTemp, hotListening) = vk;
                    hotKeys = hotKeysTemp;
                    SaveToastBindings();
                    RebuildMenuAccelerators();
                }

                hotListening = -1;
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
            HBRUSH bg = CreateSolidBrush(hotConflict[idx] ? RGB(220, 60, 60) : GetSysColor(COLOR_BTNFACE));
            FillRect(dis->hDC, &dis->rcItem, bg);
            DeleteObject(bg);

            DrawEdge(dis->hDC, &dis->rcItem, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

            wchar_t text[64];
            GetWindowTextW(dis->hwndItem, text, 64);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, hotConflict[idx] ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));
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
        hotCfgWnd = nullptr;
        hotListening = -1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenToastConfig(HWND owner)
{
    if (hotCfgWnd)
    {
        SetForegroundWindow(hotCfgWnd);
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = ToastConfigWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = TOASTKEYS_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 330, 288 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);

    HWND hwnd = CreateWindowW(TOASTKEYS_CLASS, L"Configure Toast",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);

    ShowWindow(hwnd, SW_SHOW);
}

static bool bgRun = false;
static bool discordOn = true;

std::wstring GetOptionsFilePath()
{
    return GetConfigDir() + L"\\options.cfg";
}

void SaveOptions()
{
    std::wofstream f(NarrowPath(GetOptionsFilePath()).c_str());
    if (!f.is_open()) return;
    f << L"Scale=" << winScale << L"\n";
    f << L"Discord=" << (discordOn ? 1 : 0) << L"\n";
    f << L"Background=" << (bgRun ? 1 : 0) << L"\n";
    f << L"Smooth=" << (smoothScale ? 1 : 0) << L"\n";
    f << L"KeepAspect=" << (keepAspect ? 1 : 0) << L"\n";
    f << L"CropOverscan=" << (cropOverscan ? 1 : 0) << L"\n";
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
        if (name == L"Scale" && val >= 1 && val <= 5) winScale = val;
        else if (name == L"Discord") discordOn = (val != 0);
        else if (name == L"Background") bgRun = (val != 0);
        else if (name == L"Smooth") smoothScale = (val != 0);
        else if (name == L"KeepAspect") keepAspect = (val != 0);
        else if (name == L"CropOverscan") cropOverscan = (val != 0);
    }
}

static const char* DISCORD_ID = "1550792213561352334";

static HANDLE dcPipe = INVALID_HANDLE_VALUE;
static volatile bool dcRunning = false;
static HANDLE dcThread = nullptr;
static CRITICAL_SECTION dcLock;
static bool dcLockReady = false;

static bool dcDirty = true;
static std::string dcDetails = "In the Menu";
static long long dcStart = 0;
static bool dcHasTime = false;

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
    if (dcPipe == INVALID_HANDLE_VALUE) return false;
    u32 op = (u32)opcode;
    u32 len = (u32)json.size();
    DWORD written = 0;
    if (!WriteFile(dcPipe, &op, 4, &written, nullptr) || written != 4) return false;
    if (!WriteFile(dcPipe, &len, 4, &written, nullptr) || written != 4) return false;
    if (len > 0)
    {
        if (!WriteFile(dcPipe, json.data(), len, &written, nullptr) || written != len) return false;
    }
    return true;
}

static bool DiscordReadFrame(std::string& outJson, DWORD waitMs)
{
    DWORD avail = 0;
    if (!PeekNamedPipe(dcPipe, nullptr, 0, nullptr, &avail, nullptr)) return false;
    if (avail < 8)
    {
        Sleep(waitMs);
        if (!PeekNamedPipe(dcPipe, nullptr, 0, nullptr, &avail, nullptr)) return false;
        if (avail < 8) return false;
    }
    u32 op = 0, len = 0;
    DWORD readBytes = 0;
    if (!ReadFile(dcPipe, &op, 4, &readBytes, nullptr) || readBytes != 4) return false;
    if (!ReadFile(dcPipe, &len, 4, &readBytes, nullptr) || readBytes != 4) return false;
    outJson.resize(len);
    if (len > 0)
    {
        if (!ReadFile(dcPipe, &outJson[0], len, &readBytes, nullptr) || readBytes != len) return false;
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
            dcPipe = h;
            return true;
        }
    }
    return false;
}

static void DiscordDisconnect()
{
    if (dcPipe != INVALID_HANDLE_VALUE)
    {
        CloseHandle(dcPipe);
        dcPipe = INVALID_HANDLE_VALUE;
    }
}

static bool DiscordHandshake()
{
    std::string payload = "{\"v\":1,\"client_id\":\"" + std::string(DISCORD_ID) + "\"}";
    if (!DiscordWriteFrame(0, payload)) return false;
    std::string resp;
    return DiscordReadFrame(resp, 300);
}

static std::string BuildActivityJson()
{
    static long long nonceCounter = 0;
    char nonceBuf[24];
    wsprintfA(nonceBuf, "%lld", ++nonceCounter);

    std::string details = JsonEscape(dcDetails);

    std::string json = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    json += std::to_string((long long)GetCurrentProcessId());
    json += ",\"activity\":{\"details\":\"" + details + "\"";

    if (dcHasTime)
    {
        json += ",\"timestamps\":{\"start\":";
        json += std::to_string(dcStart);
        json += "}";
    }

    json += ",\"assets\":{\"large_image\":\"icon\",\"large_text\":\"Toast\"}";
    json += "}},\"nonce\":\"" + std::string(nonceBuf) + "\"}";
    return json;
}

DWORD WINAPI DiscordThreadProc(LPVOID)
{
    while (dcRunning)
    {
        if (!discordOn)
        {
            if (dcPipe != INVALID_HANDLE_VALUE) DiscordDisconnect();
            Sleep(500);
            continue;
        }

        if (dcPipe == INVALID_HANDLE_VALUE)
        {
            if (!DiscordConnect()) { Sleep(2000); continue; }
            if (!DiscordHandshake()) { DiscordDisconnect(); Sleep(2000); continue; }
            EnterCriticalSection(&dcLock);
            dcDirty = true;
            LeaveCriticalSection(&dcLock);
        }

        bool dirty = false;
        std::string json;
        EnterCriticalSection(&dcLock);
        dirty = dcDirty;
        if (dirty) { json = BuildActivityJson(); dcDirty = false; }
        LeaveCriticalSection(&dcLock);

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
    if (!dcLockReady) { InitializeCriticalSection(&dcLock); dcLockReady = true; }
    dcRunning = true;
    dcThread = CreateThread(nullptr, 0, DiscordThreadProc, nullptr, 0, nullptr);
}

void StopDiscord()
{
    dcRunning = false;
    if (dcThread)
    {
        WaitForSingleObject(dcThread, 2000);
        CloseHandle(dcThread);
        dcThread = nullptr;
    }
    if (dcLockReady) { DeleteCriticalSection(&dcLock); dcLockReady = false; }
}

void SetDiscordMenu()
{
    if (!dcLockReady) return;
    EnterCriticalSection(&dcLock);
    dcDetails = "In the Menu";
    dcHasTime = false;
    dcDirty = true;
    LeaveCriticalSection(&dcLock);
}

void SetDiscordPlaying(const std::wstring& romPath)
{
    if (!dcLockReady) return;
    size_t slash = romPath.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? romPath : romPath.substr(slash + 1);
    std::string narrowName = NarrowPath(name);

    EnterCriticalSection(&dcLock);
    dcDetails = "Playing - " + narrowName;
    dcStart = (long long)time(nullptr);
    dcHasTime = true;
    dcDirty = true;
    LeaveCriticalSection(&dcLock);
}

typedef UINT(WINAPI* TimeBeginPeriodFn)(UINT);
typedef UINT(WINAPI* TimeEndPeriodFn)(UINT);
static HMODULE winmmLib = nullptr;
static TimeBeginPeriodFn pTimeBeginPeriod = nullptr;
static TimeEndPeriodFn   pTimeEndPeriod = nullptr;

void LoadWinmmTimers()
{
    winmmLib = LoadLibraryW(L"winmm.dll");
    if (!winmmLib) return;
    pTimeBeginPeriod = (TimeBeginPeriodFn)GetProcAddress(winmmLib, "timeBeginPeriod");
    pTimeEndPeriod = (TimeEndPeriodFn)GetProcAddress(winmmLib, "timeEndPeriod");
}

void UnloadWinmmTimers()
{
    if (pTimeEndPeriod) pTimeEndPeriod(1);
    if (winmmLib) FreeLibrary(winmmLib);
    winmmLib = nullptr;
    pTimeBeginPeriod = nullptr;
    pTimeEndPeriod = nullptr;
}

typedef HANDLE(WINAPI* AvSetMmThreadCharacteristicsWFn)(LPCWSTR, LPDWORD);
typedef BOOL(WINAPI* AvRevertMmThreadCharacteristicsFn)(HANDLE);

static volatile bool audioRunning = false;
static HANDLE audioThread = nullptr;

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

struct AudioFader
{
    float gain = 0.0f;
    float held = 0.0f;

    static float RampStep() { return 1.0f / (float)(Apu2A03::kSampleRate * 0.010); }

    int16_t Process(bool hasData, int16_t sample)
    {
        const float step = RampStep();
        if (hasData)
        {
            held = (float)sample;
            gain += step;
            if (gain > 1.0f) gain = 1.0f;
        }
        else
        {
            gain -= step;
            if (gain < 0.0f) gain = 0.0f;
        }
        if (gain >= 1.0f && hasData) return sample;
        float shaped = gain * gain * (3.0f - 2.0f * gain);
        return (int16_t)(held * shaped);
    }
};

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

    while (audioRunning && enumr)
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

        AudioFader fader;
        DWORD lastDeviceCheck = GetTickCount();

        for (;;)
        {
            if (!audioRunning) break;

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
            for (UINT32 i = 0; i < framesAvailable; i++)
            {
                size_t r = emuBus.apu.ringRead.load(std::memory_order_relaxed);
                if (r == emuBus.apu.ringWrite.load(std::memory_order_acquire))
                {
                    out[i] = fader.Process(false, 0);
                }
                else
                {
                    out[i] = fader.Process(true, emuBus.apu.ringBuffer[r]);
                    emuBus.apu.ringRead.store((r + 1) & (Apu2A03::kRingSize - 1), std::memory_order_release);
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
    audioRunning = true;
    audioThread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);
}

void StopAudio()
{
    audioRunning = false;
    if (audioThread)
    {
        WaitForSingleObject(audioThread, 2000);
        CloseHandle(audioThread);
        audioThread = nullptr;
    }
}

void UpdateWindowTitle(HWND hwnd)
{
    std::wstring title = L"Toast";
    if (romLoaded)
    {
        size_t slash = loadedRom.find_last_of(L"\\/");
        std::wstring name = (slash == std::wstring::npos) ? loadedRom : loadedRom.substr(slash + 1);
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

    emuCart = std::move(newCart);
    emuBus.ConnectCartridge(&emuCart);
    emuBus.Reset();
    emuCpu.Reset();
    emuBus.totalCycles = 0;
    GameGenieClearAll();

    if (!emuCart.lastError.empty())
        ShowMsg(emuCart.lastError.c_str(), "Notice", MB_OK | MB_ICONWARNING);

    romLoaded = true;
    emuRunning = true;
    loadedRom = path;
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
        emuRunning = false;
        LoadRom(hwnd, fileBuf);
    }
}

static bool ToastWindowIsFocused()
{
    HWND foreground = GetForegroundWindow();
    return foreground != nullptr && foreground == mainWnd;
}

void PollInput()
{
    if (!romLoaded) return;

    if (!ToastWindowIsFocused())
    {
        emuBus.controllerState[0] = 0;
        emuBus.controllerState[1] = 0;
        return;
    }

    for (int p = 0; p < 2; p++)
    {
        u8 s = 0;
        if (GetAsyncKeyState(padKeys[p].A) & 0x8000)      s |= 0x01;
        if (GetAsyncKeyState(padKeys[p].B) & 0x8000)      s |= 0x02;
        if (GetAsyncKeyState(padKeys[p].Select) & 0x8000) s |= 0x04;
        if (GetAsyncKeyState(padKeys[p].Start) & 0x8000)  s |= 0x08;
        if (GetAsyncKeyState(padKeys[p].Up) & 0x8000)     s |= 0x10;
        if (GetAsyncKeyState(padKeys[p].Down) & 0x8000)   s |= 0x20;
        if (GetAsyncKeyState(padKeys[p].Left) & 0x8000)   s |= 0x40;
        if (GetAsyncKeyState(padKeys[p].Right) & 0x8000)  s |= 0x80;
        emuBus.controllerState[p] = s;
    }
}

static bool cropEdges = false;

void RunOneFrame()
{
    if (!emuRunning) return;
    emuBus.ppu.frameComplete = false;
    int guard = 400000;
    while (!emuBus.ppu.frameComplete && guard-- > 0)
    {
        emuBus.Clock();
    }
    for (int i = 0; i < NES_W * NES_H; i++)
        frameBuf[i] = emuBus.ppu.screen[i];

    cropEdges = emuBus.ppu.LeftColumnHidden();
}

void PaintFrame(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    RECT rc; GetClientRect(hwnd, &rc);
    int destW = rc.right - rc.left;
    int destH = rc.bottom - rc.top;

    bool crop = cropEdges && cropOverscan;
    int cropX = crop ? 8 : 0;
    int cropTop = crop ? 8 : 0;
    int cropBottom = crop ? 7 : 0;
    int srcW = NES_W - 2 * cropX;
    int srcH = NES_H - cropTop - cropBottom;

    int dx = 0, dy = 0, dw = destW, dh = destH;
    if (keepAspect && destW > 0 && destH > 0)
    {
        double sx = (double)destW / srcW, sy = (double)destH / srcH;
        double s = sx < sy ? sx : sy;
        dw = (int)(srcW * s);
        dh = (int)(srcH * s);
        dx = (destW - dw) / 2;
        dy = (destH - dh) / 2;

        HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RECT bars[4] = { { 0, 0, destW, dy }, { 0, dy + dh, destW, destH }, { 0, dy, dx, dy + dh }, { dx + dw, dy, destW, dy + dh } };
        for (int i = 0; i < 4; i++)
            if (bars[i].right > bars[i].left && bars[i].bottom > bars[i].top) FillRect(hdc, &bars[i], black);
    }

    if (smoothScale)
    {
        SetStretchBltMode(hdc, HALFTONE);
        SetBrushOrgEx(hdc, 0, 0, nullptr);
    }
    else
    {
        SetStretchBltMode(hdc, COLORONCOLOR);
    }

    StretchDIBits(hdc,
        dx, dy, dw, dh,
        cropX, cropTop, srcW, srcH,
        frameBuf.data(), &frameBmi,
        DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(hwnd, hdc);
}

static HWND padCfgWnd = nullptr;
static KeyBindings padKeysTemp[2];
static int padListening = -1;
static bool padConflict[16]{};
static const int ID_REBIND_BASE = 100;
static const int ID_TIMER_KEYPOLL = 1;

void RefreshRebindButtonText(HWND hwnd, int flatIndex)
{
    HWND btn = GetDlgItem(hwnd, ID_REBIND_BASE + flatIndex);
    std::wstring text = padListening == flatIndex ? L"Press a key..." : KeyName(*BindingSlot(padKeysTemp, flatIndex));
    SetWindowTextW(btn, text.c_str());
}

void RecomputeInputConflicts(HWND hwnd)
{
    for (int i = 0; i < 16; i++) padConflict[i] = false;
    for (int i = 0; i < 16; i++)
    {
        int vi = *BindingSlot(padKeysTemp, i);
        if (vi <= 0) continue;
        for (int j = i + 1; j < 16; j++)
        {
            int vj = *BindingSlot(padKeysTemp, j);
            if (vi == vj) { padConflict[i] = true; padConflict[j] = true; }
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
        padKeysTemp[0] = padKeys[0];
        padKeysTemp[1] = padKeys[1];

        const int rowH = 32, labelW = 90, btnW = 150, padX = 16, padY = 32;

        CreateWindowW(L"STATIC", L"Player 1", WS_CHILD | WS_VISIBLE | SS_LEFT, padX, 8, labelW, 20, hwnd, nullptr, nullptr, nullptr);
        CreateWindowW(L"STATIC", L"Player 2", WS_CHILD | WS_VISIBLE | SS_LEFT, padX + 260, 8, labelW, 20, hwnd, nullptr, nullptr, nullptr);

        for (int i = 0; i < 16; i++)
        {
            int p = i / 8;
            int b = i % 8;
            int offsetX = p * 260;
            int y = padY + b * rowH;

            CreateWindowW(L"STATIC", buttonNames[b], WS_CHILD | WS_VISIBLE | SS_LEFT,
                padX + offsetX, y + 5, labelW, 20, hwnd, nullptr, nullptr, nullptr);
            HWND btn = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
                padX + labelW + offsetX, y, btnW, 24, hwnd, (HMENU)(UINT_PTR)(ID_REBIND_BASE + i), nullptr, nullptr);
            (void)btn;
        }

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
            padListening = id - ID_REBIND_BASE;
            SetFocus(hwnd);
            for (int i = 0; i < 16; i++) RefreshRebindButtonText(hwnd, i);
        }
        return 0;
    }

    case WM_TIMER:
        if (wParam == ID_TIMER_KEYPOLL && padListening >= 0 && GetForegroundWindow() == hwnd)
        {
            for (int vk = 0x08; vk <= 0xFE; vk++)
            {
                if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) continue;
                if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU) continue;
                if (!(GetAsyncKeyState(vk) & 0x8000)) continue;

                if (vk != VK_ESCAPE)
                {
                    *BindingSlot(padKeysTemp, padListening) = vk;
                    padKeys[0] = padKeysTemp[0];
                    padKeys[1] = padKeysTemp[1];
                    SaveKeyBindings();
                }

                padListening = -1;
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
            HBRUSH bg = CreateSolidBrush(padConflict[idx] ? RGB(220, 60, 60) : GetSysColor(COLOR_BTNFACE));
            FillRect(dis->hDC, &dis->rcItem, bg);
            DeleteObject(bg);

            DrawEdge(dis->hDC, &dis->rcItem, pressed ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

            wchar_t text[64];
            GetWindowTextW(dis->hwndItem, text, 64);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, padConflict[idx] ? RGB(255, 255, 255) : GetSysColor(COLOR_BTNTEXT));
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
        padCfgWnd = nullptr;
        padListening = -1;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenInputConfig(HWND owner)
{
    if (padCfgWnd)
    {
        SetForegroundWindow(padCfgWnd);
        return;
    }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = InputConfigWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = PAD_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 540, 300 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);

    padCfgWnd = CreateWindowW(PAD_CLASS, L"Configure Controls",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);

    ShowWindow(padCfgWnd, SW_SHOW);
}

static HWND prefsWnd = nullptr;
static const wchar_t* PREFS_CLASS = L"NesEmuOptionsClass";
static const int ID_OPT_SCALE_BASE = 400;
static const int ID_OPT_DISCORD = 410;
static const int ID_OPT_BACKGROUND = 411;
static const int ID_OPT_VIDEO_LABEL = 413;
static const int ID_OPT_SMOOTH = 414;
static const int ID_OPT_ASPECT = 415;
static const int ID_OPT_CROP = 416;
static const int ID_OPT_TAB = 420;

static void EnsureTabControlClass()
{
    static bool done = false;
    if (done) return;
    done = true;
    HMODULE lib = LoadLibraryW(L"comctl32.dll");
    if (!lib) return;
    typedef BOOL (WINAPI *InitCommonControlsExFn)(const INITCOMMONCONTROLSEX*);
    InitCommonControlsExFn init = (InitCommonControlsExFn)GetProcAddress(lib, "InitCommonControlsEx");
    if (init)
    {
        INITCOMMONCONTROLSEX icc;
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_TAB_CLASSES;
        init(&icc);
    }
}

static void ApplyWindowScale(HWND prefsHwnd)
{
    HWND owner = GetWindow(prefsHwnd, GW_OWNER);
    if (!owner) return;
    RECT wr{ 0, 0, NES_W * winScale, NES_H * winScale };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, TRUE);
    SetWindowPos(owner, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top, SWP_NOMOVE | SWP_NOZORDER);
}

static void ShowPreferencesTab(HWND hwnd, int tab)
{
    const int generalIds[] = { ID_OPT_DISCORD, ID_OPT_BACKGROUND };
    for (size_t i = 0; i < sizeof(generalIds) / sizeof(generalIds[0]); i++)
        ShowWindow(GetDlgItem(hwnd, generalIds[i]), tab == 0 ? SW_SHOW : SW_HIDE);

    int videoIds[] = { ID_OPT_VIDEO_LABEL, ID_OPT_SMOOTH, ID_OPT_ASPECT, ID_OPT_CROP,
                       ID_OPT_SCALE_BASE, ID_OPT_SCALE_BASE + 1, ID_OPT_SCALE_BASE + 2, ID_OPT_SCALE_BASE + 3, ID_OPT_SCALE_BASE + 4 };
    for (size_t i = 0; i < sizeof(videoIds) / sizeof(videoIds[0]); i++)
        ShowWindow(GetDlgItem(hwnd, videoIds[i]), tab == 1 ? SW_SHOW : SW_HIDE);
}

static bool IsChecked(HWND hwnd, int id)
{
    return SendMessageW(GetDlgItem(hwnd, id), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

LRESULT CALLBACK OptionsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        EnsureTabControlClass();

        HWND tab = CreateWindowExW(0, L"SysTabControl32", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_FOCUSNEVER,
            8, 8, 384, 28, hwnd, (HMENU)(UINT_PTR)ID_OPT_TAB, nullptr, nullptr);
        const wchar_t* tabNames[2] = { L"General", L"Video" };
        for (int i = 0; i < 2; i++)
        {
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = (LPWSTR)tabNames[i];
            SendMessageW(tab, TCM_INSERTITEMW, (WPARAM)i, (LPARAM)&item);
        }

        HWND discordCb = CreateWindowW(L"BUTTON", L"Enable Discord Rich Presence", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 56, 360, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_DISCORD, nullptr, nullptr);
        SendMessageW(discordCb, BM_SETCHECK, discordOn ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND bgCb = CreateWindowW(L"BUTTON", L"Continue emulating when window is not focused", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 84, 360, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_BACKGROUND, nullptr, nullptr);
        SendMessageW(bgCb, BM_SETCHECK, bgRun ? BST_CHECKED : BST_UNCHECKED, 0);

        CreateWindowW(L"STATIC", L"Window Size", WS_CHILD, 20, 52, 120, 20, hwnd, (HMENU)(UINT_PTR)ID_OPT_VIDEO_LABEL, nullptr, nullptr);
        const wchar_t* labels[5] = { L"1x", L"2x", L"3x", L"4x", L"5x" };
        for (int i = 0; i < 5; i++)
        {
            DWORD style = WS_CHILD | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0);
            HWND rb = CreateWindowW(L"BUTTON", labels[i], style, 20 + i * 62, 76, 58, 22, hwnd, (HMENU)(UINT_PTR)(ID_OPT_SCALE_BASE + i), nullptr, nullptr);
            if (winScale == i + 1) SendMessageW(rb, BM_SETCHECK, BST_CHECKED, 0);
        }

        HWND smoothCb = CreateWindowW(L"BUTTON", L"Smooth scaling", WS_CHILD | BS_AUTOCHECKBOX,
            20, 110, 360, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_SMOOTH, nullptr, nullptr);
        SendMessageW(smoothCb, BM_SETCHECK, smoothScale ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND aspectCb = CreateWindowW(L"BUTTON", L"Keep aspect ratio when resizing", WS_CHILD | BS_AUTOCHECKBOX,
            20, 136, 360, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_ASPECT, nullptr, nullptr);
        SendMessageW(aspectCb, BM_SETCHECK, keepAspect ? BST_CHECKED : BST_UNCHECKED, 0);

        HWND cropCb = CreateWindowW(L"BUTTON", L"Crop hidden edges", WS_CHILD | BS_AUTOCHECKBOX,
            20, 162, 360, 22, hwnd, (HMENU)(UINT_PTR)ID_OPT_CROP, nullptr, nullptr);
        SendMessageW(cropCb, BM_SETCHECK, cropOverscan ? BST_CHECKED : BST_UNCHECKED, 0);

        ShowPreferencesTab(hwnd, 0);
        return 0;
    }

    case WM_NOTIFY:
    {
        NMHDR* nm = (NMHDR*)lParam;
        if (nm && nm->idFrom == ID_OPT_TAB && nm->code == TCN_SELCHANGE)
        {
            int sel = (int)SendMessageW(nm->hwndFrom, TCM_GETCURSEL, 0, 0);
            ShowPreferencesTab(hwnd, sel);
        }
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) != BN_CLICKED) return 0;
        int id = LOWORD(wParam);
        HWND owner = GetWindow(hwnd, GW_OWNER);

        if (id >= ID_OPT_SCALE_BASE && id < ID_OPT_SCALE_BASE + 5)
        {
            winScale = id - ID_OPT_SCALE_BASE + 1;
            SaveOptions();
            ApplyWindowScale(hwnd);
        }
        else if (id == ID_OPT_DISCORD)
        {
            discordOn = IsChecked(hwnd, ID_OPT_DISCORD);
            SaveOptions();
        }
        else if (id == ID_OPT_BACKGROUND)
        {
            bgRun = IsChecked(hwnd, ID_OPT_BACKGROUND);
            SaveOptions();
        }
        else if (id == ID_OPT_SMOOTH || id == ID_OPT_ASPECT || id == ID_OPT_CROP)
        {
            smoothScale = IsChecked(hwnd, ID_OPT_SMOOTH);
            keepAspect = IsChecked(hwnd, ID_OPT_ASPECT);
            cropOverscan = IsChecked(hwnd, ID_OPT_CROP);
            SaveOptions();
            if (owner) InvalidateRect(owner, nullptr, FALSE);
        }
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        prefsWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenOptions(HWND owner)
{
    if (prefsWnd) { SetForegroundWindow(prefsWnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = OptionsWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = PREFS_CLASS;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hIcon = LoadIconW(wc.hInstance, L"MAINICON");
        if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        classRegistered = true;
    }

    RECT wr{ 0, 0, 400, 200 };
    AdjustWindowRect(&wr, WS_CAPTION | WS_SYSMENU, FALSE);
    prefsWnd = CreateWindowW(PREFS_CLASS, L"Preferences",
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(prefsWnd, SW_SHOW);
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

    f.flush();
    return f.good();
}

static u16 Rd16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }
static u32 Rd32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

static bool ReadZipStoredFirstEntry(const std::wstring& path, std::vector<u8>& outData,
                                    const char** error = nullptr, bool* notFound = nullptr)
{
    const char* dummy = nullptr;
    if (!error) error = &dummy;
    if (notFound) *notFound = false;
    outData.clear();

    std::ifstream f(NarrowPath(path).c_str(), std::ios::binary);
    if (!f.is_open()) { if (notFound) *notFound = true; *error = "the file could not be opened"; return false; }

    f.seekg(0, std::ios::end);
    std::streamoff fileSize = f.tellg();
    f.seekg(0, std::ios::beg);
    if (fileSize < 30 + 22) { *error = "the file is too small to be a save state (truncated?)"; return false; }
    if (fileSize > (std::streamoff)(256u * 1024u * 1024u)) { *error = "the file is too large to be a save state"; return false; }

    std::vector<u8> file((size_t)fileSize);
    f.read((char*)file.data(), (std::streamsize)file.size());
    if ((std::streamoff)f.gcount() != fileSize) { *error = "the file could not be read completely"; return false; }

    const u8* d = file.data();
    size_t n = file.size();

    if (Rd32(d) != 0x04034b50) { *error = "this is not a Toast save state (bad zip header)"; return false; }
    u16 flags = Rd16(d + 6), method = Rd16(d + 8);
    u32 crc = Rd32(d + 14), compSize = Rd32(d + 18), uncompSize = Rd32(d + 22);
    u16 nameLen = Rd16(d + 26), extraLen = Rd16(d + 28);
    if (flags & 0x0001) { *error = "encrypted zip files are not supported"; return false; }
    if (flags & 0x0008) { *error = "zip uses a data descriptor, which Toast save states never do"; return false; }
    if (method != 0)    { *error = "the zip is compressed; Toast save states are stored uncompressed"; return false; }
    if (compSize != uncompSize) { *error = "entry sizes do not match (corrupted)"; return false; }

    size_t dataStart = 30 + (size_t)nameLen + (size_t)extraLen;
    if (dataStart > n || (size_t)compSize > n - dataStart) { *error = "the file is truncated (entry data is incomplete)"; return false; }
    size_t dataEnd = dataStart + compSize;

    size_t eocd = std::string::npos;
    size_t searchFrom = (n > 22 + 65535) ? n - (22 + 65535) : 0;
    for (size_t i = n - 22 + 1; i-- > searchFrom; )
    {
        if (Rd32(d + i) == 0x06054b50) { eocd = i; break; }
    }
    if (eocd == std::string::npos || eocd < dataEnd) { *error = "the end of the zip is missing (truncated?)"; return false; }
    u16 entries = Rd16(d + eocd + 10);
    u32 cdSize = Rd32(d + eocd + 12), cdOffset = Rd32(d + eocd + 16);
    if (entries < 1) { *error = "the zip contains no entries"; return false; }
    if (cdOffset < dataEnd || (size_t)cdOffset > n || (size_t)cdSize > n - (size_t)cdOffset || cdSize < 46)
    { *error = "the zip directory is damaged"; return false; }

    const u8* c = d + cdOffset;
    if (Rd32(c) != 0x02014b50) { *error = "the zip directory is damaged"; return false; }
    if (Rd32(c + 16) != crc || Rd32(c + 20) != compSize || Rd32(c + 24) != uncompSize || Rd32(c + 42) != 0)
    { *error = "the zip directory does not match the saved data (corrupted)"; return false; }

    if (Crc32(d + dataStart, compSize) != crc) { *error = "the data failed its CRC check (the file is corrupted)"; return false; }

    outData.assign(d + dataStart, d + dataEnd);
    return true;
}

static bool SaveZipVerified(const std::wstring& path, const std::vector<u8>& data, const char** error)
{
    std::wstring tmp = path + L".tmp";
    if (!WriteZipStored(tmp, "state.bin", data))
    {
        DeleteFileA(NarrowPath(tmp).c_str());
        *error = "the file could not be written (disk full or no permission?)";
        return false;
    }
    std::vector<u8> check;
    const char* why = nullptr;
    if (!ReadZipStoredFirstEntry(tmp, check, &why) || check != data)
    {
        DeleteFileA(NarrowPath(tmp).c_str());
        *error = "the saved file failed verification, so the old file was kept";
        return false;
    }
    if (!MoveFileExA(NarrowPath(tmp).c_str(), NarrowPath(path).c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        DeleteFileA(NarrowPath(tmp).c_str());
        *error = "the existing file could not be replaced";
        return false;
    }
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
    size_t slash = loadedRom.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? loadedRom : loadedRom.substr(slash + 1);
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
    return emuCart.IsLoaded() && savedCrc == emuCart.GetRomCrc();
}

static void SaveStateToPath(const std::wstring& path)
{
    StateWriter w;
    emuBus.SaveState(w);
    const char* why = nullptr;
    if (!SaveZipVerified(path, w.buf, &why))
    {
        std::string msg = std::string("Failed to save state: ") + why + ".";
        ShowMsg(msg.c_str(), "Save State", MB_OK | MB_ICONERROR);
    }
}

static void ApplyStateBuffer(const std::vector<u8>& data)
{
    if (!StateBufferMatchesLoadedRom(data))
    {
        ShowMsg("This save state was made for a different ROM and cannot be loaded.", "Load State", MB_OK | MB_ICONWARNING);
        return;
    }

    StateWriter backup;
    emuBus.SaveState(backup);

    StateReader r(data.data(), data.size());
    emuBus.LoadState(r);
    if (!r.ok || r.pos != r.len)
    {
        StateReader rb(backup.buf.data(), backup.buf.size());
        emuBus.LoadState(rb);
        ShowMsg(r.ok ? "This save state does not match this version of Toast (unexpected size), so it was not loaded."
                     : "This save state is corrupted, so it was not loaded.",
                "Load State", MB_OK | MB_ICONERROR);
    }
}

static void LoadStateFromPath(const std::wstring& path, bool isSlot)
{
    std::vector<u8> data;
    const char* why = nullptr;
    bool notFound = false;
    if (!ReadZipStoredFirstEntry(path, data, &why, &notFound))
    {
        if (notFound && isSlot)
        {
            ShowMsg("No save state in that slot.", "Load State", MB_OK | MB_ICONWARNING);
        }
        else
        {
            std::string msg = std::string("This save state was not loaded: ") + why + ".";
            ShowMsg(msg.c_str(), "Load State", MB_OK | MB_ICONERROR);
        }
        return;
    }
    ApplyStateBuffer(data);
}

void SaveStateToSlot(int slot)
{
    if (!romLoaded) return;
    SaveStateToPath(GetStateFilePath(slot));
}

void LoadStateFromSlot(int slot)
{
    if (!romLoaded) return;
    LoadStateFromPath(GetStateFilePath(slot), true);
}

void SaveStateToZipDialog(HWND hwnd)
{
    if (!romLoaded) return;
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

    SaveStateToPath(fileBuf);
}

void LoadStateFromZipDialog(HWND hwnd)
{
    if (!romLoaded) return;
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

    LoadStateFromPath(fileBuf, false);
}

static HWND hexWnd = nullptr;
static const wchar_t* HEX_CLASS = L"NesEmuHexEditorClass";
static HFONT hexFont = nullptr;
static int hexTop = 0;
static int hexCW = 0, hexCH = 0;
static int hexSel = 0;
static int hexNib = -1;
static const int ID_HEX_GOTO_EDIT = 300;
static const int ID_HEX_GOTO_BTN = 301;
static const int ID_HEX_TIMER = 2;
static const int HEX_COLS = 16;
static const int HEX_ROWS = 65536 / HEX_COLS;
static const int HEX_HDR_Y = 28;

void HexEditorEnsureFont(HDC hdc)
{
    if (hexFont) return;
    hexFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    HFONT old = (HFONT)SelectObject(hdc, hexFont);
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    hexCW = tm.tmAveCharWidth;
    hexCH = tm.tmHeight + tm.tmExternalLeading + 2;
    SelectObject(hdc, old);
}

int HexRowsVisible(HWND hwnd)
{
    RECT rc; GetClientRect(hwnd, &rc);
    int usable = (rc.bottom - rc.top) - HEX_HDR_Y;
    if (hexCH <= 0) return 1;
    int rows = usable / hexCH;
    return rows < 1 ? 1 : rows;
}

void HexEditorPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HexEditorEnsureFont(hdc);
    HFONT old = (HFONT)SelectObject(hdc, hexFont);
    SetBkMode(hdc, TRANSPARENT);

    RECT rc; GetClientRect(hwnd, &rc);
    HBRUSH bg = (HBRUSH)GetStockObject(WHITE_BRUSH);
    FillRect(hdc, &rc, bg);

    int visibleRows = HexRowsVisible(hwnd);

    int addrX = 8;
    int hexX = addrX + 9 * hexCW;
    int asciiX = hexX + HEX_COLS * 3 * hexCW + hexCW;

    for (int r = 0; r < visibleRows; r++)
    {
        int row = hexTop + r;
        if (row >= HEX_ROWS) break;
        int y = HEX_HDR_Y + r * hexCH;
        int baseAddr = row * HEX_COLS;

        wchar_t addrBuf[16];
        wsprintfW(addrBuf, L"%04X:", baseAddr);
        TextOutW(hdc, addrX, y, addrBuf, (int)wcslen(addrBuf));

        wchar_t asciiBuf[HEX_COLS + 1];
        for (int c = 0; c < HEX_COLS; c++)
        {
            int addr = baseAddr + c;
            if (addr > 0xFFFF) { asciiBuf[c] = L' '; continue; }
            u8 val = emuBus.CpuRead((u16)addr);

            if (addr == hexSel)
            {
                RECT sel{ hexX + c * 3 * hexCW, y, hexX + c * 3 * hexCW + 2 * hexCW, y + hexCH };
                HBRUSH hl = CreateSolidBrush(RGB(200, 220, 255));
                FillRect(hdc, &sel, hl);
                DeleteObject(hl);
            }
            wchar_t byteBuf[3];
            wsprintfW(byteBuf, L"%02X", val);
            TextOutW(hdc, hexX + c * 3 * hexCW, y, byteBuf, 2);

            asciiBuf[c] = (val >= 0x20 && val < 0x7F) ? (wchar_t)val : L'.';
        }
        asciiBuf[HEX_COLS] = 0;
        TextOutW(hdc, asciiX, y, asciiBuf, HEX_COLS);
    }

    SelectObject(hdc, old);
    EndPaint(hwnd, &ps);
}

void HexEditorScrollTo(int addr)
{
    int row = addr / HEX_COLS;
    hexTop = row - 4;
    if (hexTop < 0) hexTop = 0;
    if (hexTop > HEX_ROWS - 1) hexTop = HEX_ROWS - 1;
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

        SetScrollRange(hwnd, SB_VERT, 0, HEX_ROWS - 1, TRUE);
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
            hexSel = addr;
            SetScrollPos(hwnd, SB_VERT, hexTop, TRUE);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam);
        int my = GET_Y_LPARAM(lParam);
        int addrX = 8;
        int hexX = addrX + 9 * hexCW;
        if (my >= HEX_HDR_Y && mx >= hexX && hexCW > 0 && hexCH > 0)
        {
            int r = (my - HEX_HDR_Y) / hexCH;
            int c = (mx - hexX) / (3 * hexCW);
            if (c >= 0 && c < HEX_COLS)
            {
                int addr = (hexTop + r) * HEX_COLS + c;
                if (addr >= 0 && addr <= 0xFFFF)
                {
                    hexSel = addr;
                    hexNib = -1;
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

        if (digit >= 0 && hexSel >= 0 && hexSel <= 0xFFFF)
        {
            if (hexNib < 0)
            {
                hexNib = digit;
            }
            else
            {
                u8 val = (u8)((hexNib << 4) | digit);
                emuBus.CpuWrite((u16)hexSel, val);
                hexNib = -1;
                if (hexSel < 0xFFFF) hexSel++;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wParam == VK_UP && hexSel >= HEX_COLS) { hexSel -= HEX_COLS; hexNib = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_DOWN && hexSel <= 0xFFFF - HEX_COLS) { hexSel += HEX_COLS; hexNib = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_LEFT && hexSel > 0) { hexSel--; hexNib = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        else if (wParam == VK_RIGHT && hexSel < 0xFFFF) { hexSel++; hexNib = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;

    case WM_VSCROLL:
    {
        int pos = hexTop;
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
        if (pos > HEX_ROWS - 1) pos = HEX_ROWS - 1;
        hexTop = pos;
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
        if (hexFont) { DeleteObject(hexFont); hexFont = nullptr; }
        hexWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenHexEditor(HWND owner)
{
    if (hexWnd) { SetForegroundWindow(hexWnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = HexEditorWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = HEX_CLASS;
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
    hexWnd = CreateWindowW(HEX_CLASS, L"Hex Editor",
        WS_OVERLAPPEDWINDOW | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(hexWnd, SW_SHOW);
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

static std::vector<GgEntry> ggCodes;
static HWND    ggWnd = nullptr;
static WNDPROC ggOldProc = nullptr;
static const wchar_t* GG_CLASS = L"NesEmuGameGenieClass";

static const int ID_GG_EDIT   = 400;
static const int ID_GG_ADD    = 401;
static const int ID_GG_LIST   = 402;
static const int ID_GG_TOGGLE = 403;
static const int ID_GG_REMOVE = 404;
static const int ID_GG_CLEAR  = 405;
static const int ID_GG_STATUS = 406;

static bool GgDecode(const std::wstring& text, GgEntry& out)
{
    static const wchar_t ggLetters[] = L"APZLGITYEOXUKSVN";
    int n[8] = {};
    int len = 0;
    std::wstring canon;

    for (wchar_t ch : text)
    {
        if (ch == L' ' || ch == L'-') continue;
        if (ch >= L'a' && ch <= L'z') ch = (wchar_t)(ch - L'a' + L'A');

        int v = -1;
        for (int i = 0; i < 16; i++)
            if (ggLetters[i] == ch) { v = i; break; }

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
    emuBus.gameGenie.clear();
    for (const auto& e : ggCodes)
        if (e.enabled)
            emuBus.gameGenie.push_back({ e.addr, e.value, e.compare, e.hasCompare });
}

static void GgRefreshList(int select = -1)
{
    if (!ggWnd) return;
    HWND list = GetDlgItem(ggWnd, ID_GG_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);

    for (const auto& e : ggCodes)
    {
        wchar_t line[128];
        if (e.hasCompare)
            wsprintfW(line, L"[%s]  %s    %04X:%02X (if %02X)", e.enabled ? L"x" : L" ", e.code.c_str(), e.addr, e.value, e.compare);
        else
            wsprintfW(line, L"[%s]  %s    %04X:%02X", e.enabled ? L"x" : L" ", e.code.c_str(), e.addr, e.value);
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)line);
    }
    if (select >= 0 && select < (int)ggCodes.size())
        SendMessageW(list, LB_SETCURSEL, select, 0);
}

void GameGenieClearAll()
{
    ggCodes.clear();
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
    return CallWindowProcW(ggOldProc, h, m, w, l);
}

LRESULT CALLBACK GameGenieWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        ggWnd = hwnd;
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
        ggOldProc = (WNDPROC)SetWindowLongPtrW(ctl[1], GWLP_WNDPROC, (LONG_PTR)GgEditProc);

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
            for (const auto& x : ggCodes)
            {
                if (x.code == e.code)
                {
                    SetDlgItemTextW(hwnd, ID_GG_STATUS, L"That code is already in the list.");
                    return 0;
                }
            }
            ggCodes.push_back(e);
            GgApply();
            GgRefreshList((int)ggCodes.size() - 1);
            SetDlgItemTextW(hwnd, ID_GG_EDIT, L"");
            SetDlgItemTextW(hwnd, ID_GG_STATUS, L"Code added.");
            SetFocus(GetDlgItem(hwnd, ID_GG_EDIT));
        }
        else if (id == ID_GG_TOGGLE || (id == ID_GG_LIST && code == LBN_DBLCLK))
        {
            int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)ggCodes.size())
            {
                ggCodes[sel].enabled = !ggCodes[sel].enabled;
                GgApply();
                GgRefreshList(sel);
            }
        }
        else if (id == ID_GG_REMOVE)
        {
            int sel = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < (int)ggCodes.size())
            {
                ggCodes.erase(ggCodes.begin() + sel);
                GgApply();
                GgRefreshList(sel < (int)ggCodes.size() ? sel : (int)ggCodes.size() - 1);
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
        ggWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void OpenGameGenie(HWND owner)
{
    if (ggWnd) { SetForegroundWindow(ggWnd); return; }

    static bool classRegistered = false;
    if (!classRegistered)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = GameGenieWndProc;
        wc.hInstance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        wc.lpszClassName = GG_CLASS;
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
    ggWnd = CreateWindowW(GG_CLASS, L"Game Genie",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE), nullptr);
    ShowWindow(ggWnd, SW_SHOW);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_ACTIVATE:
        winActive = (LOWORD(wParam) != WA_INACTIVE);
        return 0;

    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(drop, 0, path, MAX_PATH))
        {
            emuRunning = false;
            LoadRom(hwnd, path);
        }
        DragFinish(drop);
        return 0;
    }

    case WM_KEYDOWN:
    if (wParam == (WPARAM)hotKeys.Reset && romLoaded && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        emuBus.Reset();
        emuCpu.Reset();
    }
    else if (wParam == (WPARAM)hotKeys.OpenRom && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenFileDialog(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.InputConfig && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenInputConfig(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.ToastConfig && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenToastConfig(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.Options && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenOptions(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.HexEditor && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenHexEditor(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.GameGenie && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        OpenGameGenie(hwnd);
    }
    else if (wParam == (WPARAM)hotKeys.PauseResume && romLoaded && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        emuRunning = !emuRunning;
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

    frameBmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    frameBmi.bmiHeader.biWidth = NES_W;
    frameBmi.bmiHeader.biHeight = -NES_H;
    frameBmi.bmiHeader.biPlanes = 1;
    frameBmi.bmiHeader.biBitCount = 32;
    frameBmi.bmiHeader.biCompression = BI_RGB;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = LoadIconW(hInstance, L"MAINICON");
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    HMENU menuBar = CreateMenu();
    HMENU fileMenu = CreatePopupMenu();
    menuFile = fileMenu;
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
    menuInput = inputMenu;
    AppendMenuW(inputMenu, MF_STRING, ID_INPUT_CONFIG, L"Configure NES...");
    AppendMenuW(inputMenu, MF_STRING, ID_TOAST_CONFIG, L"Configure Toast...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)inputMenu, L"Input");

    HMENU optionsMenu = CreatePopupMenu();
    menuPrefs = optionsMenu;
    AppendMenuW(optionsMenu, MF_STRING, ID_OPTIONS, L"Preferences...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)optionsMenu, L"Options");

    HMENU toolsMenu = CreatePopupMenu();
    menuTools = toolsMenu;
    AppendMenuW(toolsMenu, MF_STRING, ID_HEX_EDITOR, L"Hex Editor...");
    AppendMenuW(toolsMenu, MF_STRING, ID_GAME_GENIE, L"Game Genie...");
    AppendMenuW(menuBar, MF_POPUP, (UINT_PTR)toolsMenu, L"Tools");

    RECT wr{ 0, 0, NES_W * winScale, NES_H * winScale };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, TRUE);

    HWND hwnd = CreateWindowW(MAIN_CLASS, L"Toast",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT,
    wr.right - wr.left, wr.bottom - wr.top,
    nullptr, menuBar, hInstance, nullptr);

    mainWnd = hwnd;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    emuBus.cpu = &emuCpu;
    emuCpu.ConnectBus(&emuBus);

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

        if (!winActive && !bgRun)
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
