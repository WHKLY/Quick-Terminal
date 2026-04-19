#include <windows.h>
#include <shellapi.h>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

enum
{
    HOTKEY_ID = 1
};

static const wchar_t *kMutexName = L"QuickTerminalHotkeySingleton";
static const wchar_t *kTerminalExe = L"wt.exe";
static const wchar_t *kTerminalArgs = L"new-tab powershell.exe";
static const wchar_t *kRunKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *kRunValueName = L"QuickTerminalHotkey";
static const wchar_t *kUsageText =
    L"Quick Terminal\n\n"
    L"Supported arguments:\n"
    L"--enable-autostart\n"
    L"--disable-autostart\n"
    L"--autostart-status\n"
    L"--help";

static void ShowInfoMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, L"Quick Terminal", MB_OK | MB_ICONINFORMATION);
}

static void ShowErrorMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, L"Quick Terminal", MB_OK | MB_ICONERROR);
}

static BOOL GetExecutableCommandLine(wchar_t *buffer, DWORD buffer_count)
{
    DWORD path_length;

    if (buffer == NULL || buffer_count < 4)
    {
        return FALSE;
    }

    path_length = GetModuleFileNameW(NULL, buffer + 1, buffer_count - 3);
    if (path_length == 0 || path_length >= buffer_count - 3)
    {
        return FALSE;
    }

    buffer[0] = L'"';
    buffer[path_length + 1] = L'"';
    buffer[path_length + 2] = L'\0';
    return TRUE;
}

static BOOL EnableAutostart(void)
{
    HKEY key = NULL;
    wchar_t command[MAX_PATH + 3];
    DWORD command_size;
    LONG status;

    if (!GetExecutableCommandLine(command, sizeof(command) / sizeof(command[0])))
    {
        ShowErrorMessage(L"Failed to resolve the current executable path.");
        return FALSE;
    }

    status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        NULL,
        0,
        KEY_SET_VALUE,
        NULL,
        &key,
        NULL);
    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to open the current user Run registry key.");
        return FALSE;
    }

    command_size = (lstrlenW(command) + 1) * (DWORD)sizeof(wchar_t);
    status = RegSetValueExW(
        key,
        kRunValueName,
        0,
        REG_SZ,
        (const BYTE *)command,
        command_size);
    RegCloseKey(key);

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to write the auto-start registry value.");
        return FALSE;
    }

    ShowInfoMessage(L"Auto-start has been enabled for the current user.");
    return TRUE;
}

static BOOL DisableAutostart(void)
{
    HKEY key = NULL;
    LONG status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        KEY_SET_VALUE,
        &key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        ShowInfoMessage(L"Auto-start is already disabled.");
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to open the current user Run registry key.");
        return FALSE;
    }

    status = RegDeleteValueW(key, kRunValueName);
    RegCloseKey(key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        ShowInfoMessage(L"Auto-start is already disabled.");
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to remove the auto-start registry value.");
        return FALSE;
    }

    ShowInfoMessage(L"Auto-start has been disabled for the current user.");
    return TRUE;
}

static BOOL ShowAutostartStatus(void)
{
    HKEY key = NULL;
    wchar_t command[1024];
    wchar_t message[1200];
    DWORD type = 0;
    DWORD size = sizeof(command);
    LONG status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        KEY_QUERY_VALUE,
        &key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        ShowInfoMessage(L"Auto-start is disabled.");
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to open the current user Run registry key.");
        return FALSE;
    }

    status = RegQueryValueExW(
        key,
        kRunValueName,
        NULL,
        &type,
        (LPBYTE)command,
        &size);
    RegCloseKey(key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        ShowInfoMessage(L"Auto-start is disabled.");
        return TRUE;
    }

    if (status != ERROR_SUCCESS || type != REG_SZ)
    {
        ShowErrorMessage(L"Failed to query the auto-start registry value.");
        return FALSE;
    }

    if (wsprintfW(message, L"Auto-start is enabled.\n\nCommand:\n%s", command) <= 0)
    {
        ShowInfoMessage(L"Auto-start is enabled.");
        return TRUE;
    }

    ShowInfoMessage(message);
    return TRUE;
}

static int HandleManagementCommand(void)
{
    int argc = 0;
    int exit_code = -1;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argv == NULL)
    {
        ShowErrorMessage(L"Failed to parse command-line arguments.");
        return 1;
    }

    if (argc <= 1)
    {
        LocalFree(argv);
        return -1;
    }

    if (lstrcmpiW(argv[1], L"--enable-autostart") == 0)
    {
        exit_code = EnableAutostart() ? 0 : 1;
    }
    else if (lstrcmpiW(argv[1], L"--disable-autostart") == 0)
    {
        exit_code = DisableAutostart() ? 0 : 1;
    }
    else if (lstrcmpiW(argv[1], L"--autostart-status") == 0)
    {
        exit_code = ShowAutostartStatus() ? 0 : 1;
    }
    else if (lstrcmpiW(argv[1], L"--help") == 0)
    {
        ShowInfoMessage(kUsageText);
        exit_code = 0;
    }
    else
    {
        ShowErrorMessage(L"Unknown argument. Use --help to see the supported options.");
        exit_code = 1;
    }

    LocalFree(argv);
    return exit_code;
}

static BOOL LaunchWindowsTerminal(void)
{
    HINSTANCE result = ShellExecuteW(
        NULL,
        L"open",
        kTerminalExe,
        kTerminalArgs,
        NULL,
        SW_SHOWNORMAL);

    return ((INT_PTR)result > 32);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    (void)instance;
    (void)previous;
    (void)command_line;
    (void)show_command;

    {
        int management_exit_code = HandleManagementCommand();
        if (management_exit_code >= 0)
        {
            return management_exit_code;
        }
    }

    HANDLE mutex = CreateMutexW(NULL, TRUE, kMutexName);
    if (mutex == NULL)
    {
        ShowErrorMessage(L"Failed to create the application mutex.");
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(mutex);
        return 0;
    }

    if (!RegisterHotKey(NULL, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T'))
    {
        ShowErrorMessage(L"Failed to register Ctrl+Alt+T. The hotkey may already be in use.");
        CloseHandle(mutex);
        return 1;
    }

    MSG message;
    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        if (message.message == WM_HOTKEY && message.wParam == HOTKEY_ID)
        {
            if (!LaunchWindowsTerminal())
            {
                ShowErrorMessage(L"Failed to launch Windows Terminal with PowerShell.");
            }

            continue;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterHotKey(NULL, HOTKEY_ID);
    CloseHandle(mutex);
    return 0;
}
