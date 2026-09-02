#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>
#include "include/json.hpp"

#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Msimg32.lib")

#pragma comment(linker, \
    "\"/manifestdependency:type='win32' "\
    "name='Microsoft.Windows.Common-Controls' "\
    "version='6.0.0.0' "\
    "processorArchitecture='*' "\
    "publicKeyToken='6595b64144ccf1df' "\
    "language='*'\"")

using json = nlohmann::json;

#define ID_BUTTON_OPEN 1001
#define ID_STATIC_INFO 1002
#define ID_LISTVIEW    1003
#define ID_STATIC_STATUS 1004
#define ID_COMBO_PLUGIN  1005

#define WM_APP_VOL_DONE (WM_APP + 1)

struct PluginOption
{
    const wchar_t* label;
    const wchar_t* plugin;
};

const PluginOption PLUGIN_OPTIONS[] = {
    { L"System Info (info)",              L"windows.info"                  },
    { L"Process List (pslist)",           L"windows.pslist"                },
    { L"Process Scan (psscan)",           L"windows.psscan"                },
    { L"Process Tree (pstree)",           L"windows.pstree"                },
    { L"Command Lines (cmdline)",         L"windows.cmdline"               },
    { L"Environment Vars (envars)",       L"windows.envars"                },
    { L"Sessions (sessions)",             L"windows.sessions"              },
    { L"Privileges (privileges)",         L"windows.privileges"            },
    { L"User SIDs (getsids)",             L"windows.getsids"               },
    { L"Threads (threads)",               L"windows.threads"               },
    { L"Thread Scan (thrdscan)",          L"windows.thrdscan"              },

    { L"Loaded DLLs (dlllist)",           L"windows.dlllist"               },
    { L"LDR Modules (ldrmodules)",        L"windows.ldrmodules"            },
    { L"Suspicious Memory (malfind)",     L"windows.malfind"               },
    { L"VAD Info (vadinfo)",              L"windows.vadinfo"               },
    { L"VAD Walk (vadwalk)",              L"windows.vadwalk"               },
    { L"Virtual Memory Map (virtmap)",    L"windows.virtmap"               },
    { L"Handles (handles)",               L"windows.handles"               },
    { L"Kernel Modules (modules)",        L"windows.modules"               },
    { L"Module Scan (modscan)",           L"windows.modscan"               },

    { L"Network Connections (netscan)",   L"windows.netscan"               },
    { L"Network Stat (netstat)",          L"windows.netstat"               },

    { L"Services (svcscan)",              L"windows.svcscan"               },
    { L"Driver Scan (driverscan)",        L"windows.driverscan"            },
    { L"Device Tree (devicetree)",        L"windows.devicetree"            },
    { L"Callbacks (callbacks)",           L"windows.callbacks"             },
    { L"Unloaded Modules (unloadedmodules)", L"windows.unloadedmodules"    },

    { L"File Scan (filescan)",            L"windows.filescan"              },
    { L"Mutant Scan (mutantscan)",        L"windows.mutantscan"            },
    { L"Symlink Scan (symlinkscan)",      L"windows.symlinkscan"           },
    { L"Big Pools (bigpools)",            L"windows.bigpools"              },

    { L"Registry Hives (hivelist)",       L"windows.registry.hivelist"     },
    { L"Registry Certificates",           L"windows.registry.certificates" },
    { L"UserAssist (userassist)",         L"windows.registry.userassist"   },

    { L"Shimcache Memory (shimcachemem)", L"windows.shimcachemem"          },
    { L"Skeleton Key Check",              L"windows.skeleton_key_check"    },
    { L"Timeliner (timeliner)",           L"windows.timeliner"             },
    { L"Version Info (verinfo)",          L"windows.verinfo"               },
    { L"Statistics (statistics)",         L"windows.statistics"            },
    { L"Crash Info (crashinfo)",          L"windows.crashinfo"             },
};
const int PLUGIN_OPTIONS_COUNT = sizeof(PLUGIN_OPTIONS) / sizeof(PLUGIN_OPTIONS[0]);

HWND hInfo = nullptr;
HWND hListView = nullptr;
HWND hStatus = nullptr;
HWND hHeaderPanel = nullptr;
HWND hButtonOpen = nullptr;
HWND hComboPlugin = nullptr;
HWND hGroupList = nullptr;

std::wstring g_dumpPath;

const COLORREF COLOR_ACCENT = RGB(37, 99, 235);
const COLORREF COLOR_ACCENT_DARK = RGB(29, 78, 216);
const COLORREF COLOR_BG = RGB(244, 246, 249);
const COLORREF COLOR_PANEL_BG = RGB(255, 255, 255);
const COLORREF COLOR_TEXT_DARK = RGB(30, 34, 40);
const COLORREF COLOR_TEXT_MUTED = RGB(110, 118, 130);
const COLORREF COLOR_STATUS_IDLE = RGB(110, 118, 130);
const COLORREF COLOR_STATUS_BUSY = RGB(217, 119, 6);
const COLORREF COLOR_STATUS_OK = RGB(22, 163, 74);
const COLORREF COLOR_STATUS_ERR = RGB(220, 38, 38);
const COLORREF COLOR_ROW_ALT = RGB(240, 244, 250);

HFONT hFontTitle = nullptr;
HFONT hFontSubtitle = nullptr;
HFONT hFontUI = nullptr;
HFONT hFontMono = nullptr;
HBRUSH hBrushBg = nullptr;
HBRUSH hBrushPanel = nullptr;

std::wstring g_statusText = L"Idle";
COLORREF g_statusColor = COLOR_STATUS_IDLE;
bool g_buttonHover = false;

std::string g_volOutput;
std::mutex g_volOutputMutex;

void SetStatus(const std::wstring& text, COLORREF color)
{
    g_statusText = text;
    g_statusColor = color;

    if (hStatus)
    {
        SetWindowTextW(hStatus, text.c_str());
        InvalidateRect(hStatus, nullptr, TRUE);
    }
}

std::wstring FormatFileSize(ULONGLONG size)
{
    std::wstringstream ss;

    if (size >= 1024ULL * 1024ULL * 1024ULL)
    {
        double gb = static_cast<double>(size) /
            (1024.0 * 1024.0 * 1024.0);
        ss.precision(2);
        ss << std::fixed << gb << L" GB";
    }
    else if (size >= 1024ULL * 1024ULL)
    {
        double mb = static_cast<double>(size) /
            (1024.0 * 1024.0);
        ss.precision(2);
        ss << std::fixed << mb << L" MB";
    }
    else if (size >= 1024ULL)
    {
        double kb = static_cast<double>(size) /
            1024.0;
        ss.precision(2);
        ss << std::fixed << kb << L" KB";
    }
    else
    {
        ss << size << L" Bytes";
    }

    return ss.str();
}

std::string RunProcessAndCaptureOutput(const std::wstring& commandLine)
{
    SECURITY_ATTRIBUTES saAttr{};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = nullptr;

    HANDLE hReadPipe = nullptr;
    HANDLE hWritePipe = nullptr;

    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0))
        return "Error: could not create pipe.";

    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    SECURITY_ATTRIBUTES saNull{};
    saNull.nLength = sizeof(saNull);
    saNull.bInheritHandle = TRUE;
    saNull.lpSecurityDescriptor = nullptr;

    HANDLE hNul = CreateFileW(
        L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE,
        &saNull, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = (hNul != INVALID_HANDLE_VALUE) ? hNul : hWritePipe;

    PROCESS_INFORMATION pi{};

    std::wstring cmd = commandLine;

    BOOL success = CreateProcessW(
        nullptr,
        &cmd[0],
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(hWritePipe);
    if (hNul != INVALID_HANDLE_VALUE)
        CloseHandle(hNul);

    if (!success)
    {
        CloseHandle(hReadPipe);
        return "Error: could not start Volatility process. "
            "Make sure Python and volatility3 are installed and in PATH.";
    }

    std::string output;
    char buffer[4096];
    DWORD bytesRead = 0;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr)
        && bytesRead > 0)
    {
        buffer[bytesRead] = '\0';
        output.append(buffer, bytesRead);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return output;
}

const std::wstring VOLATILITY_SCRIPT_PATH =
L"C:\\Users\\Us3r\\volatility3\\vol.py";

void RunVolatilityAsync(HWND hwnd, std::wstring dumpPath, std::wstring pluginName)
{
    std::thread([hwnd, dumpPath, pluginName]()
        {
            std::wstring cmd =
                L"python3 \"" + VOLATILITY_SCRIPT_PATH + L"\" -q -f \"" +
                dumpPath + L"\" -r json " + pluginName;

            std::string result = RunProcessAndCaptureOutput(cmd);

            {
                std::lock_guard<std::mutex> lock(g_volOutputMutex);
                g_volOutput = result;
            }

            PostMessageW(hwnd, WM_APP_VOL_DONE, 0, 0);

        }).detach();
}

std::wstring GetSelectedPluginArg()
{
    int sel = (int)SendMessageW(hComboPlugin, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= PLUGIN_OPTIONS_COUNT)
        sel = 0;
    return PLUGIN_OPTIONS[sel].plugin;
}

void RunSelectedPlugin(HWND hwnd)
{
    if (g_dumpPath.empty())
        return;

    std::wstring pluginArg = GetSelectedPluginArg();

    std::wstring status = L"● Running Volatility3 (" + pluginArg + L")...";
    SetStatus(status.c_str(), COLOR_STATUS_BUSY);
    ListView_DeleteAllItems(hListView);

    RunVolatilityAsync(hwnd, g_dumpPath, pluginArg);
}

void OpenDumpFile(HWND hwnd)
{
    wchar_t fileName[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;

    ofn.lpstrFilter =
        L"Memory Dump (*.dmp;*.raw;*.mem;*.vmem)\0*.dmp;*.raw;*.mem;*.vmem\0"
        L"All Files (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST |
        OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn))
        return;

    SetStatus(L"● Reading file...", COLOR_STATUS_BUSY);

    HANDLE hFile = CreateFileW(
        fileName,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE)
    {
        SetStatus(L"● Error opening file", COLOR_STATUS_ERR);
        MessageBoxW(
            hwnd,
            L"Could not open the dump file.",
            L"Error",
            MB_ICONERROR
        );
        return;
    }

    LARGE_INTEGER fileSize{};

    if (!GetFileSizeEx(hFile, &fileSize))
    {
        CloseHandle(hFile);

        SetStatus(L"● Error reading file size", COLOR_STATUS_ERR);
        MessageBoxW(
            hwnd,
            L"Could not get the file size.",
            L"Error",
            MB_ICONERROR
        );
        return;
    }

    CloseHandle(hFile);

    std::wstring path(fileName);

    size_t pos = path.find_last_of(L"\\/");

    std::wstring name =
        (pos == std::wstring::npos)
        ? path
        : path.substr(pos + 1);

    std::wstring info =
        L"File name:  " + name +
        L"\r\nPath:  " + path +
        L"\r\nSize:  " + FormatFileSize(static_cast<ULONGLONG>(fileSize.QuadPart));

    SetWindowTextW(hInfo, info.c_str());
    g_dumpPath = path;

    RunSelectedPlugin(hwnd);
}

std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty())
        return std::wstring();

    int sizeNeeded = MultiByteToWideChar(
        CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);

    std::wstring result(sizeNeeded, 0);

    MultiByteToWideChar(
        CP_UTF8, 0, str.data(), (int)str.size(), &result[0], sizeNeeded);

    return result;
}

std::wstring JsonFieldToWString(const json& item, const std::string& key)
{
    if (!item.contains(key) || item.at(key).is_null())
        return L"-";

    const json& value = item.at(key);

    if (value.is_string())
    {
        std::string s = value.get<std::string>();
        return Utf8ToWide(s);
    }
    else
    {
        std::string s = value.dump();
        return Utf8ToWide(s);
    }
}

std::vector<std::string> CollectColumnKeys(const json& parsedArray)
{
    std::vector<std::string> keys;

    static const std::vector<std::string> IGNORED_KEYS = {
        "__children"
    };

    for (const auto& item : parsedArray)
    {
        if (!item.is_object())
            continue;

        for (auto it = item.begin(); it != item.end(); ++it)
        {
            const std::string& key = it.key();

            bool ignored = false;
            for (auto& ig : IGNORED_KEYS)
            {
                if (key == ig) { ignored = true; break; }
            }
            if (ignored)
                continue;

            bool alreadyAdded = false;
            for (auto& k : keys)
            {
                if (k == key) { alreadyAdded = true; break; }
            }
            if (!alreadyAdded)
                keys.push_back(key);
        }
    }

    return keys;
}

void SetupDynamicColumns(HWND listView, const std::vector<std::string>& keys)
{
    while (ListView_DeleteColumn(listView, 0)) {}

    int index = 0;
    for (const auto& key : keys)
    {
        std::wstring wideKey = Utf8ToWide(key);

        int width = 120;
        if (key == "PID" || key == "PPID" || key == "Threads" || key == "Handles")
            width = 70;
        else if (key == "ImageFileName" || key == "Process" || key == "Name")
            width = 200;
        else if (key == "Hexdump" || key == "Disasm")
            width = 300;

        LVCOLUMNW lvc{};
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvc.pszText = (LPWSTR)wideKey.c_str();
        lvc.cx = width;
        lvc.iSubItem = index;
        ListView_InsertColumn(listView, index, &lvc);
        index++;
    }
}

bool PopulateListViewFromJson(HWND listView, const std::string& jsonText, int& itemCount)
{
    itemCount = 0;
    json parsed;

    try
    {
        parsed = json::parse(jsonText);
    }
    catch (const std::exception&)
    {
        return false;
    }

    if (!parsed.is_array())
        return false;

    std::vector<std::string> keys = CollectColumnKeys(parsed);

    ListView_DeleteAllItems(listView);
    SetupDynamicColumns(listView, keys);

    int row = 0;
    for (const auto& item : parsed)
    {
        if (!item.is_object())
            continue;

        LVITEMW lvi{};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = row;
        lvi.iSubItem = 0;

        std::wstring firstValue = keys.empty() ? L"" : JsonFieldToWString(item, keys[0]);
        lvi.pszText = (LPWSTR)firstValue.c_str();
        int insertedIndex = ListView_InsertItem(listView, &lvi);

        for (size_t col = 1; col < keys.size(); col++)
        {
            std::wstring value = JsonFieldToWString(item, keys[col]);
            ListView_SetItemText(listView, insertedIndex, (int)col, (LPWSTR)value.c_str());
        }

        row++;
    }

    itemCount = row;
    return true;
}

LRESULT CALLBACK ButtonSubclassProc(
    HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
    UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    switch (uMsg)
    {
    case WM_MOUSEMOVE:
    {
        if (!g_buttonHover)
        {
            g_buttonHover = true;
            InvalidateRect(hwnd, nullptr, FALSE);

            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
        }
        break;
    }
    case WM_MOUSELEAVE:
    {
        g_buttonHover = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    }
    case WM_NCDESTROY:
        RemoveWindowSubclass(hwnd, ButtonSubclassProc, uIdSubclass);
        break;
    }

    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProc(
    HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {

        hFontTitle = CreateFontW(
            -22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        hFontSubtitle = CreateFontW(
            -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        hFontUI = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        hFontMono = CreateFontW(
            -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

        hBrushBg = CreateSolidBrush(COLOR_BG);
        hBrushPanel = CreateSolidBrush(COLOR_PANEL_BG);

        hHeaderPanel = CreateWindowW(
            L"STATIC", L"",
            WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
            0, 0, 780, 72,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );

        HWND hButton = CreateWindowW(
            L"BUTTON",
            L"📂  Open Memory Dump",
            WS_TABSTOP | WS_VISIBLE |
            WS_CHILD | BS_OWNERDRAW,
            24, 90,
            210, 38,
            hwnd,
            (HMENU)ID_BUTTON_OPEN,
            GetModuleHandleW(nullptr),
            nullptr
        );
        SendMessageW(hButton, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        hButtonOpen = hButton;
        SetWindowSubclass(hButtonOpen, ButtonSubclassProc, 1, 0);

        hComboPlugin = CreateWindowW(
            L"COMBOBOX",
            L"",
            WS_VISIBLE | WS_CHILD | WS_TABSTOP |
            CBS_DROPDOWNLIST | WS_VSCROLL,
            250, 90,
            280, 450,
            hwnd,
            (HMENU)ID_COMBO_PLUGIN,
            GetModuleHandleW(nullptr),
            nullptr
        );
        SendMessageW(hComboPlugin, WM_SETFONT, (WPARAM)hFontUI, TRUE);

        for (int i = 0; i < PLUGIN_OPTIONS_COUNT; i++)
        {
            SendMessageW(hComboPlugin, CB_ADDSTRING, 0, (LPARAM)PLUGIN_OPTIONS[i].label);
        }
        SendMessageW(hComboPlugin, CB_SETCURSEL, 0, 0);

        hStatus = CreateWindowW(
            L"STATIC", L"● Idle",
            WS_VISIBLE | WS_CHILD | SS_OWNERDRAW,
            542, 90, 210, 38,
            hwnd, (HMENU)ID_STATIC_STATUS, GetModuleHandleW(nullptr), nullptr
        );

        HWND hGroupFile = CreateWindowW(
            L"BUTTON", L"File Information",
            WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            24, 142, 732, 90,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        SendMessageW(hGroupFile, WM_SETFONT, (WPARAM)hFontUI, TRUE);

        hInfo = CreateWindowW(
            L"EDIT",
            L"Select a memory dump file to begin...",
            WS_VISIBLE | WS_CHILD | WS_BORDER |
            WS_VSCROLL | WS_HSCROLL |
            ES_LEFT | ES_MULTILINE |
            ES_READONLY | ES_AUTOVSCROLL,
            36, 166,
            708, 56,
            hwnd,
            (HMENU)ID_STATIC_INFO,
            GetModuleHandleW(nullptr),
            nullptr
        );
        SendMessageW(hInfo, WM_SETFONT, (WPARAM)hFontMono, TRUE);

        HWND hGroupList = CreateWindowW(
            L"BUTTON", L"Results",
            WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            24, 246, 732, 344,
            hwnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        SendMessageW(hGroupList, WM_SETFONT, (WPARAM)hFontUI, TRUE);
        ::hGroupList = hGroupList;

        hListView = CreateWindowW(
            WC_LISTVIEWW,
            L"",
            WS_VISIBLE | WS_CHILD | WS_BORDER |
            LVS_REPORT | LVS_SHOWSELALWAYS,
            36, 270,
            708, 306,
            hwnd,
            (HMENU)ID_LISTVIEW,
            GetModuleHandleW(nullptr),
            nullptr
        );

        ListView_SetExtendedListViewStyle(
            hListView,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER
        );

        SendMessageW(hListView, WM_SETFONT, (WPARAM)hFontUI, TRUE);

        RECT rcInfo, rcList;
        GetWindowRect(hInfo, &rcInfo);
        GetWindowRect(hListView, &rcList);

        HRGN hRgnInfo = CreateRoundRectRgn(
            0, 0, rcInfo.right - rcInfo.left, rcInfo.bottom - rcInfo.top, 8, 8);
        SetWindowRgn(hInfo, hRgnInfo, TRUE);

        HRGN hRgnList = CreateRoundRectRgn(
            0, 0, rcList.right - rcList.left, rcList.bottom - rcList.top, 8, 8);
        SetWindowRgn(hListView, hRgnList, TRUE);

        return 0;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;

        SetBkMode(hdcStatic, TRANSPARENT);

        if (hCtrl == hInfo)
        {
            SetTextColor(hdcStatic, COLOR_TEXT_DARK);
            SetBkColor(hdcStatic, COLOR_PANEL_BG);
            return (LRESULT)hBrushPanel;
        }

        SetTextColor(hdcStatic, COLOR_TEXT_DARK);
        return (LRESULT)hBrushBg;
    }

    case WM_CTLCOLOREDIT:
    {
        HDC hdcEdit = (HDC)wParam;
        SetTextColor(hdcEdit, COLOR_TEXT_DARK);
        SetBkColor(hdcEdit, COLOR_PANEL_BG);
        return (LRESULT)hBrushPanel;
    }

    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, hBrushBg);
        return 1;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;

        if (dis->hwndItem == hButtonOpen)
        {
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;

            COLORREF top, bottom;
            if (pressed)
            {
                top = COLOR_ACCENT_DARK;
                bottom = RGB(14, 40, 105);
            }
            else if (g_buttonHover)
            {
                top = RGB(65, 130, 255);
                bottom = COLOR_ACCENT;
            }
            else
            {
                top = COLOR_ACCENT;
                bottom = COLOR_ACCENT_DARK;
            }

            TRIVERTEX vert[2];
            vert[0].x = dis->rcItem.left;
            vert[0].y = dis->rcItem.top;
            vert[0].Red = GetRValue(top) << 8;
            vert[0].Green = GetGValue(top) << 8;
            vert[0].Blue = GetBValue(top) << 8;
            vert[0].Alpha = 0;

            vert[1].x = dis->rcItem.right;
            vert[1].y = dis->rcItem.bottom;
            vert[1].Red = GetRValue(bottom) << 8;
            vert[1].Green = GetGValue(bottom) << 8;
            vert[1].Blue = GetBValue(bottom) << 8;
            vert[1].Alpha = 0;

            GRADIENT_RECT gRect = { 0, 1 };

            HRGN hRoundRgn = CreateRoundRectRgn(
                dis->rcItem.left, dis->rcItem.top,
                dis->rcItem.right + 1, dis->rcItem.bottom + 1,
                10, 10);
            HRGN hOldClip = CreateRectRgn(0, 0, 0, 0);
            GetClipRgn(dis->hDC, hOldClip);
            SelectClipRgn(dis->hDC, hRoundRgn);

            GradientFill(dis->hDC, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_V);

            SelectClipRgn(dis->hDC, hOldClip);
            DeleteObject(hOldClip);
            DeleteObject(hRoundRgn);

            COLORREF borderColor = g_buttonHover ? RGB(150, 190, 255) : RGB(0, 0, 0);
            HPEN hPen = CreatePen(PS_SOLID, g_buttonHover ? 2 : 1, borderColor);
            HPEN hOldPen = (HPEN)SelectObject(dis->hDC, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
            RoundRect(dis->hDC, dis->rcItem.left, dis->rcItem.top,
                dis->rcItem.right, dis->rcItem.bottom, 10, 10);
            SelectObject(dis->hDC, hOldPen);
            SelectObject(dis->hDC, hOldBrush);
            DeleteObject(hPen);

            wchar_t text[128];
            GetWindowTextW(dis->hwndItem, text, 128);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            SelectObject(dis->hDC, hFontUI);
            RECT rcText = dis->rcItem;
            if (pressed) OffsetRect(&rcText, 0, 1);
            DrawTextW(dis->hDC, text, -1, &rcText,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            return TRUE;
        }

        if (dis->hwndItem == hHeaderPanel)
        {

            TRIVERTEX vert[2];
            vert[0].x = dis->rcItem.left;
            vert[0].y = dis->rcItem.top;
            vert[0].Red = GetRValue(COLOR_ACCENT) << 8;
            vert[0].Green = GetGValue(COLOR_ACCENT) << 8;
            vert[0].Blue = GetBValue(COLOR_ACCENT) << 8;
            vert[0].Alpha = 0;

            vert[1].x = dis->rcItem.right;
            vert[1].y = dis->rcItem.bottom;
            vert[1].Red = GetRValue(COLOR_ACCENT_DARK) << 8;
            vert[1].Green = GetGValue(COLOR_ACCENT_DARK) << 8;
            vert[1].Blue = GetBValue(COLOR_ACCENT_DARK) << 8;
            vert[1].Alpha = 0;

            GRADIENT_RECT gRect = { 0, 1 };
            GradientFill(dis->hDC, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_H);

            RECT rcAccentLine = dis->rcItem;
            rcAccentLine.top = rcAccentLine.bottom - 3;
            HBRUSH hAccentLine = CreateSolidBrush(COLOR_ACCENT);
            FillRect(dis->hDC, &rcAccentLine, hAccentLine);
            DeleteObject(hAccentLine);

            SetBkMode(dis->hDC, TRANSPARENT);

            RECT rcTitle = dis->rcItem;
            rcTitle.left += 24;
            rcTitle.top += 12;
            SetTextColor(dis->hDC, RGB(255, 255, 255));
            SelectObject(dis->hDC, hFontTitle);
            DrawTextW(dis->hDC, L"🧠 Memory Dump Analyzer", -1, &rcTitle,
                DT_LEFT | DT_TOP | DT_SINGLELINE);

            RECT rcSub = dis->rcItem;
            rcSub.left += 26;
            rcSub.top += 42;
            SetTextColor(dis->hDC, RGB(219, 234, 254));
            SelectObject(dis->hDC, hFontSubtitle);
            DrawTextW(dis->hDC, L"Volatility3 forensic triage · ML-ready output",
                -1, &rcSub, DT_LEFT | DT_TOP | DT_SINGLELINE);

            return TRUE;
        }

        if (dis->hwndItem == hStatus)
        {
            FillRect(dis->hDC, &dis->rcItem, hBrushBg);
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, g_statusColor);
            SelectObject(dis->hDC, hFontUI);

            RECT rc = dis->rcItem;
            DrawTextW(dis->hDC, g_statusText.c_str(), -1, &rc,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            return TRUE;
        }

        return FALSE;
    }

    case WM_NOTIFY:
    {
        LPNMHDR nmhdr = (LPNMHDR)lParam;

        if (nmhdr->hwndFrom == hListView && nmhdr->code == NM_CUSTOMDRAW)
        {
            LPNMLVCUSTOMDRAW lvcd = (LPNMLVCUSTOMDRAW)lParam;

            switch (lvcd->nmcd.dwDrawStage)
            {
            case CDDS_PREPAINT:
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                return TRUE;

            case CDDS_ITEMPREPAINT:
            {
                int row = (int)lvcd->nmcd.dwItemSpec;
                lvcd->clrTextBk = (row % 2 == 0) ? COLOR_PANEL_BG : COLOR_ROW_ALT;
                lvcd->clrText = COLOR_TEXT_DARK;
                SetWindowLongPtr(hwnd, DWLP_MSGRESULT, CDRF_DODEFAULT);
                return TRUE;
            }
            }
        }

        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BUTTON_OPEN)
        {
            OpenDumpFile(hwnd);
        }
        else if (LOWORD(wParam) == ID_COMBO_PLUGIN && HIWORD(wParam) == CBN_SELCHANGE)
        {
            RunSelectedPlugin(hwnd);
        }

        return 0;
    }

    case WM_APP_VOL_DONE:
    {
        std::string resultCopy;
        {
            std::lock_guard<std::mutex> lock(g_volOutputMutex);
            resultCopy = g_volOutput;
        }

        {
            wchar_t tempPath[MAX_PATH];
            GetTempPathW(MAX_PATH, tempPath);
            std::wstring debugFile = std::wstring(tempPath) + L"volatility_last_output.json";
            HANDLE hDbg = CreateFileW(debugFile.c_str(), GENERIC_WRITE, 0,
                nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hDbg != INVALID_HANDLE_VALUE)
            {
                DWORD written = 0;
                WriteFile(hDbg, resultCopy.data(), (DWORD)resultCopy.size(), &written, nullptr);
                CloseHandle(hDbg);
            }
        }

        if (resultCopy.empty())
        {
            SetWindowTextW(
                hInfo,
                L"Volatility returned no output.\r\n"
                L"Check that Python and volatility3 are installed "
                L"and available in PATH."
            );
            SetStatus(L"● No output from Volatility", COLOR_STATUS_ERR);
            ListView_DeleteAllItems(hListView);
        }
        else
        {
            std::wstring pluginArg = GetSelectedPluginArg();
            std::wstring groupTitle = L"Results  (" + pluginArg + L")";
            SetWindowTextW(hGroupList, groupTitle.c_str());

            int itemCount = 0;
            bool parsedOk = PopulateListViewFromJson(hListView, resultCopy, itemCount);

            if (parsedOk && itemCount > 0)
            {
                std::wstringstream ss;
                ss << L"Loaded successfully — " << itemCount << L" items found.";
                SetWindowTextW(hInfo, ss.str().c_str());
                SetStatus(L"● Done", COLOR_STATUS_OK);
            }
            else if (parsedOk && itemCount == 0)
            {

                std::wstring wideResult = Utf8ToWide(resultCopy);
                std::wstring msg =
                    L"Parsed OK but 0 processes were found in the JSON array.\r\n"
                    L"This usually means the dump could not be profiled correctly, "
                    L"or the wrong plugin/format was used.\r\n\r\n"
                    L"Raw output:\r\n" + wideResult;
                SetWindowTextW(hInfo, msg.c_str());
                SetStatus(L"● Empty result", COLOR_STATUS_ERR);
            }
            else
            {

                std::wstring wideResult = Utf8ToWide(resultCopy);
                SetWindowTextW(hInfo, wideResult.c_str());
                SetStatus(L"● Volatility error — see details above", COLOR_STATUS_ERR);
                ListView_DeleteAllItems(hListView);
            }
        }

        return 0;
    }

    case WM_DESTROY:
        if (hFontTitle) DeleteObject(hFontTitle);
        if (hFontSubtitle) DeleteObject(hFontSubtitle);
        if (hFontUI) DeleteObject(hFontUI);
        if (hFontMono) DeleteObject(hFontMono);
        if (hBrushBg) DeleteObject(hBrushBg);
        if (hBrushPanel) DeleteObject(hBrushPanel);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        uMsg,
        wParam,
        lParam
    );
}

int WINAPI wWinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    PWSTR,
    int nCmdShow)
{

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] =
        L"DumpReceiverWindowClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(
        nullptr,
        IDC_ARROW
    );
    wc.hbrBackground =
        (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc))
    {
        MessageBoxW(
            nullptr,
            L"Window registration failed.",
            L"Error",
            MB_ICONERROR
        );

        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Memory Dump Receiver",
        WS_OVERLAPPED |
        WS_CAPTION |
        WS_SYSMENU |
        WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        780,
        640,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!hwnd)
    {
        MessageBoxW(
            nullptr,
            L"Window creation failed.",
            L"Error",
            MB_ICONERROR
        );

        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};

    while (GetMessageW(
        &msg,
        nullptr,
        0,
        0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}