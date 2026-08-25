#include <windows.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#pragma comment(lib, "Comdlg32.lib")

#define ID_BUTTON_OPEN 1001
#define ID_STATIC_INFO 1002


#define WM_APP_VOL_DONE (WM_APP + 1)

HWND hInfo = nullptr;


std::string g_volOutput;
std::mutex g_volOutputMutex;

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

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;

    PROCESS_INFORMATION pi{};

    // CreateProcessW for Buffer
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


void RunVolatilityAsync(HWND hwnd, std::wstring dumpPath)
{
    std::thread([hwnd, dumpPath]()
        {
            std::wstring cmd =
                L"python -m volatility3 -q -f \"" + dumpPath +
                L"\" -r json windows.pslist";

            std::string result = RunProcessAndCaptureOutput(cmd);

            {
                std::lock_guard<std::mutex> lock(g_volOutputMutex);
                g_volOutput = result;
            }

            
            PostMessageW(hwnd, WM_APP_VOL_DONE, 0, 0);

        }).detach();
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
        L"File name:\r\n" +
        name +
        L"\r\n\r\n"
        L"Path:\r\n" +
        path +
        L"\r\n\r\n"
        L"Size:\r\n" +
        FormatFileSize(
            static_cast<ULONGLONG>(fileSize.QuadPart)
        ) +
        L"\r\n\r\n"
        L"Running Volatility3 (windows.pslist)...\r\n"
        L"This can take a while on large full memory dumps.";

    SetWindowTextW(hInfo, info.c_str());

    
    RunVolatilityAsync(hwnd, path);
}

// -----------------------------------------------------------------------
// convert std to UTF8
// -----------------------------------------------------------------------
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
            L"EDIT",
            L"Select a memory dump file...",
            WS_VISIBLE | WS_CHILD | WS_BORDER |
            WS_VSCROLL | WS_HSCROLL |
            ES_LEFT | ES_MULTILINE |
            ES_READONLY | ES_AUTOVSCROLL,
            20, 75,
            720, 480,
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


    case WM_APP_VOL_DONE:
    {
        std::string resultCopy;
        {
            std::lock_guard<std::mutex> lock(g_volOutputMutex);
            resultCopy = g_volOutput;
        }

        if (resultCopy.empty())
        {
            SetWindowTextW(
                hInfo,
                L"Volatility returned no output.\r\n"
                L"Check that Python and volatility3 are installed "
                L"and available in PATH."
            );
        }
        else
        {
            std::wstring wideResult = Utf8ToWide(resultCopy);
            SetWindowTextW(hInfo, wideResult.c_str());
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
        780,
        620,
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