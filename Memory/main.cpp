
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <sstream>

#pragma comment(lib, "Comdlg32.lib")

#define ID_BUTTON_OPEN 1001
#define ID_STATIC_INFO 1002

HWND hInfo = nullptr;

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

void OpenDumpFile(HWND hwnd)
{
    wchar_t fileName[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;

   
    ofn.lpstrFilter =
        L"Memory Dump (*.dmp)\0*.dmp\0"
        L"All Files (*.*)\0*.*\0";

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST |
        OFN_FILEMUSTEXIST |
        OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn))
        return;

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
        L"Dump received successfully.\r\n\r\n"
        L"File name:\r\n" +
        name +
        L"\r\n\r\n"
        L"Path:\r\n" +
        path +
        L"\r\n\r\n"
        L"Size:\r\n" +
        FormatFileSize(
            static_cast<ULONGLONG>(fileSize.QuadPart)
        );

    SetWindowTextW(hInfo, info.c_str());
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
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        HWND hButton = CreateWindowW(
            L"BUTTON",
            L"Open Memory Dump",
            WS_TABSTOP | WS_VISIBLE |
            WS_CHILD | BS_DEFPUSHBUTTON,
            20, 20,
            180, 35,
            hwnd,
            (HMENU)ID_BUTTON_OPEN,
            GetModuleHandleW(nullptr),
            nullptr
        );

        SendMessageW(
            hButton,
            WM_SETFONT,
            (WPARAM)hFont,
            TRUE
        );

        hInfo = CreateWindowW(
            L"STATIC",
            L"Select a .dmp file...",
            WS_VISIBLE |
            WS_CHILD |
            SS_LEFT,
            20, 75,
            520, 200,
            hwnd,
            (HMENU)ID_STATIC_INFO,
            GetModuleHandleW(nullptr),
            nullptr
        );

        SendMessageW(
            hInfo,
            WM_SETFONT,
            (WPARAM)hFont,
            TRUE
        );

        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ID_BUTTON_OPEN)
        {
            OpenDumpFile(hwnd);
        }

        return 0;
    }

    case WM_DESTROY:
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
        580,
        340,
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