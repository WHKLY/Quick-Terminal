#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

#include "resource.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4 4
#endif

enum
{
    HOTKEY_ID = 1,
    WMAPP_TRAYICON = WM_APP + 1,
    WMAPP_SET_TRAY_VISIBILITY = WM_APP + 2,
    ID_TRAY_OPEN_TERMINAL = 1001,
    ID_TRAY_ENABLE_AUTOSTART = 1002,
    ID_TRAY_DISABLE_AUTOSTART = 1003,
    ID_TRAY_SHOW_STATUS = 1004,
    ID_TRAY_EXIT = 1005
};

typedef enum
{
    COMMAND_NONE = 0,
    COMMAND_ENABLE_AUTOSTART,
    COMMAND_DISABLE_AUTOSTART,
    COMMAND_AUTOSTART_STATUS,
    COMMAND_SHOW_TRAY,
    COMMAND_HIDE_TRAY,
    COMMAND_TRAY_STATUS,
    COMMAND_HELP
} CommandMode;

typedef struct AppOptionsTag
{
    CommandMode command_mode;
    BOOL show_startup_notification;
} AppOptions;

typedef struct AppStateTag
{
    HINSTANCE instance;
    HANDLE mutex;
    HWND window;
    NOTIFYICONDATAW tray_icon;
    HICON large_icon;
    HICON small_icon;
    BOOL tray_icon_added;
    BOOL tray_enabled;
    BOOL show_startup_notification;
} AppState;

static const wchar_t *kAppTitle = L"Quick Terminal";
static const wchar_t *kWindowClassName = L"QuickTerminalHiddenWindow";
static const wchar_t *kMutexName = L"QuickTerminalHotkeySingleton";
static const wchar_t *kTerminalExe = L"wt.exe";
static const wchar_t *kTerminalArgs = L"new-tab powershell.exe";
static const wchar_t *kRunKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *kRunValueName = L"QuickTerminalHotkey";
static const wchar_t *kSettingsKeyPath = L"Software\\QuickTerminal";
static const wchar_t *kTrayEnabledValueName = L"ShowTrayIcon";
static const wchar_t *kUsageText =
    L"Quick Terminal\n\n"
    L"Supported arguments:\n"
    L"--enable-autostart\n"
    L"--disable-autostart\n"
    L"--autostart-status\n"
    L"--show-tray\n"
    L"--hide-tray\n"
    L"--tray-status\n"
    L"--help";

static AppState g_app;

static void ShowInfoMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONINFORMATION);
}

static void ShowErrorMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONERROR);
}

static HICON LoadSizedAppIcon(int width, int height)
{
    HICON icon = (HICON)LoadImageW(
        g_app.instance,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        width,
        height,
        LR_DEFAULTCOLOR);

    if (icon == NULL)
    {
        icon = LoadIconW(NULL, IDI_APPLICATION);
    }

    return icon;
}

static BOOL InitializeAppIcons(void)
{
    g_app.large_icon = LoadSizedAppIcon(
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON));
    g_app.small_icon = LoadSizedAppIcon(
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON));

    return (g_app.large_icon != NULL && g_app.small_icon != NULL);
}

static void DestroyAppIcons(void)
{
    if (g_app.large_icon != NULL && g_app.large_icon != LoadIconW(NULL, IDI_APPLICATION))
    {
        DestroyIcon(g_app.large_icon);
    }

    if (g_app.small_icon != NULL &&
        g_app.small_icon != g_app.large_icon &&
        g_app.small_icon != LoadIconW(NULL, IDI_APPLICATION))
    {
        DestroyIcon(g_app.small_icon);
    }

    g_app.large_icon = NULL;
    g_app.small_icon = NULL;
}

static BOOL BuildExecutableCommandLine(wchar_t *buffer, size_t buffer_count, BOOL include_startup_notify)
{
    wchar_t executable_path[32768];

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    if (GetModuleFileNameW(NULL, executable_path, (DWORD)(sizeof(executable_path) / sizeof(executable_path[0]))) == 0)
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        include_startup_notify ? L"\"%s\" --startup-notify" : L"\"%s\"",
        executable_path));
}

static BOOL QueryAutostartCommand(wchar_t *buffer, DWORD buffer_size_bytes, BOOL *is_enabled)
{
    HKEY key = NULL;
    DWORD type = 0;
    LONG status;

    if (is_enabled != NULL)
    {
        *is_enabled = FALSE;
    }

    status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        kRunKeyPath,
        0,
        KEY_QUERY_VALUE,
        &key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        return FALSE;
    }

    status = RegQueryValueExW(
        key,
        kRunValueName,
        NULL,
        &type,
        (LPBYTE)buffer,
        &buffer_size_bytes);
    RegCloseKey(key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        return TRUE;
    }

    if (status != ERROR_SUCCESS || type != REG_SZ)
    {
        return FALSE;
    }

    if (is_enabled != NULL)
    {
        *is_enabled = TRUE;
    }

    return TRUE;
}

static BOOL QueryTrayPreference(BOOL *is_enabled)
{
    HKEY key = NULL;
    DWORD type = 0;
    DWORD value = 1;
    DWORD size = sizeof(value);
    LONG status;

    if (is_enabled == NULL)
    {
        return FALSE;
    }

    *is_enabled = TRUE;

    status = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        kSettingsKeyPath,
        0,
        KEY_QUERY_VALUE,
        &key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        return FALSE;
    }

    status = RegQueryValueExW(
        key,
        kTrayEnabledValueName,
        NULL,
        &type,
        (LPBYTE)&value,
        &size);
    RegCloseKey(key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        return TRUE;
    }

    if (status != ERROR_SUCCESS || type != REG_DWORD)
    {
        return FALSE;
    }

    *is_enabled = (value != 0);
    return TRUE;
}

static BOOL SetTrayPreference(BOOL is_enabled)
{
    HKEY key = NULL;
    DWORD value = is_enabled ? 1U : 0U;
    LONG status = RegCreateKeyExW(
        HKEY_CURRENT_USER,
        kSettingsKeyPath,
        0,
        NULL,
        0,
        KEY_SET_VALUE,
        NULL,
        &key,
        NULL);

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to open the current user settings registry key.");
        return FALSE;
    }

    status = RegSetValueExW(
        key,
        kTrayEnabledValueName,
        0,
        REG_DWORD,
        (const BYTE *)&value,
        sizeof(value));
    RegCloseKey(key);

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to write the tray visibility setting.");
        return FALSE;
    }

    return TRUE;
}

static HWND FindRunningInstanceWindow(void)
{
    return FindWindowW(kWindowClassName, kAppTitle);
}

static void SyncRunningInstanceTrayVisibility(BOOL is_enabled)
{
    HWND window = FindRunningInstanceWindow();

    if (window != NULL)
    {
        SendMessageTimeoutW(
            window,
            WMAPP_SET_TRAY_VISIBILITY,
            is_enabled ? 1U : 0U,
            0,
            SMTO_ABORTIFHUNG,
            2000,
            NULL);
    }
}

static BOOL ShowTraySettingStatus(void)
{
    BOOL is_enabled = TRUE;

    if (!QueryTrayPreference(&is_enabled))
    {
        ShowErrorMessage(L"Failed to query the tray visibility setting.");
        return FALSE;
    }

    ShowInfoMessage(
        is_enabled
            ? L"Tray icon display is enabled."
            : L"Tray icon display is disabled.");
    return TRUE;
}

static BOOL EnableTrayPreference(void)
{
    if (!SetTrayPreference(TRUE))
    {
        return FALSE;
    }

    SyncRunningInstanceTrayVisibility(TRUE);
    ShowInfoMessage(L"Tray icon display has been enabled.");
    return TRUE;
}

static BOOL DisableTrayPreference(void)
{
    if (!SetTrayPreference(FALSE))
    {
        return FALSE;
    }

    SyncRunningInstanceTrayVisibility(FALSE);
    ShowInfoMessage(L"Tray icon display has been disabled.");
    return TRUE;
}

static BOOL EnableAutostart(void)
{
    HKEY key = NULL;
    wchar_t command[32768];
    DWORD command_size;
    LONG status;

    if (!BuildExecutableCommandLine(command, sizeof(command) / sizeof(command[0]), TRUE))
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
    wchar_t command[32768];
    wchar_t message[33280];
    BOOL is_enabled = FALSE;

    if (!QueryAutostartCommand(command, sizeof(command), &is_enabled))
    {
        ShowErrorMessage(L"Failed to query the auto-start registry value.");
        return FALSE;
    }

    if (!is_enabled)
    {
        ShowInfoMessage(L"Auto-start is disabled.");
        return TRUE;
    }

    if (FAILED(StringCchPrintfW(message, sizeof(message) / sizeof(message[0]), L"Auto-start is enabled.\n\nCommand:\n%s", command)))
    {
        ShowInfoMessage(L"Auto-start is enabled.");
        return TRUE;
    }

    ShowInfoMessage(message);
    return TRUE;
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

static BOOL ParseCommandLine(AppOptions *options)
{
    int argc = 0;
    int i;
    LPWSTR *argv;

    if (options == NULL)
    {
        return FALSE;
    }

    options->command_mode = COMMAND_NONE;
    options->show_startup_notification = FALSE;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == NULL)
    {
        ShowErrorMessage(L"Failed to parse command-line arguments.");
        return FALSE;
    }

    for (i = 1; i < argc; ++i)
    {
        if (lstrcmpiW(argv[i], L"--startup-notify") == 0)
        {
            options->show_startup_notification = TRUE;
        }
        else if (lstrcmpiW(argv[i], L"--enable-autostart") == 0)
        {
            options->command_mode = COMMAND_ENABLE_AUTOSTART;
        }
        else if (lstrcmpiW(argv[i], L"--disable-autostart") == 0)
        {
            options->command_mode = COMMAND_DISABLE_AUTOSTART;
        }
        else if (lstrcmpiW(argv[i], L"--autostart-status") == 0)
        {
            options->command_mode = COMMAND_AUTOSTART_STATUS;
        }
        else if (lstrcmpiW(argv[i], L"--show-tray") == 0)
        {
            options->command_mode = COMMAND_SHOW_TRAY;
        }
        else if (lstrcmpiW(argv[i], L"--hide-tray") == 0)
        {
            options->command_mode = COMMAND_HIDE_TRAY;
        }
        else if (lstrcmpiW(argv[i], L"--tray-status") == 0)
        {
            options->command_mode = COMMAND_TRAY_STATUS;
        }
        else if (lstrcmpiW(argv[i], L"--help") == 0)
        {
            options->command_mode = COMMAND_HELP;
        }
        else
        {
            LocalFree(argv);
            ShowErrorMessage(L"Unknown argument. Use --help to see the supported options.");
            return FALSE;
        }
    }

    LocalFree(argv);
    return TRUE;
}

static int HandleManagementCommand(const AppOptions *options)
{
    if (options == NULL)
    {
        return 1;
    }

    switch (options->command_mode)
    {
    case COMMAND_ENABLE_AUTOSTART:
        return EnableAutostart() ? 0 : 1;

    case COMMAND_DISABLE_AUTOSTART:
        return DisableAutostart() ? 0 : 1;

    case COMMAND_AUTOSTART_STATUS:
        return ShowAutostartStatus() ? 0 : 1;

    case COMMAND_SHOW_TRAY:
        return EnableTrayPreference() ? 0 : 1;

    case COMMAND_HIDE_TRAY:
        return DisableTrayPreference() ? 0 : 1;

    case COMMAND_TRAY_STATUS:
        return ShowTraySettingStatus() ? 0 : 1;

    case COMMAND_HELP:
        ShowInfoMessage(kUsageText);
        return 0;

    case COMMAND_NONE:
    default:
        return -1;
    }
}

static void RemoveTrayIcon(void)
{
    if (g_app.tray_icon_added)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_app.tray_icon);
        g_app.tray_icon_added = FALSE;
    }
}

static BOOL ShowTrayNotification(const wchar_t *title, const wchar_t *message)
{
    NOTIFYICONDATAW notification = g_app.tray_icon;

    if (!g_app.tray_icon_added)
    {
        return FALSE;
    }

    notification.uFlags = NIF_INFO;
    notification.dwInfoFlags = NIIF_INFO;

    if (FAILED(StringCchCopyW(notification.szInfoTitle, sizeof(notification.szInfoTitle) / sizeof(notification.szInfoTitle[0]), title)))
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(notification.szInfo, sizeof(notification.szInfo) / sizeof(notification.szInfo[0]), message)))
    {
        return FALSE;
    }

    return Shell_NotifyIconW(NIM_MODIFY, &notification);
}

static BOOL InitializeTrayIcon(HWND window)
{
    ZeroMemory(&g_app.tray_icon, sizeof(g_app.tray_icon));
    g_app.tray_icon.cbSize = sizeof(g_app.tray_icon);
    g_app.tray_icon.hWnd = window;
    g_app.tray_icon.uID = 1;
    g_app.tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_app.tray_icon.uCallbackMessage = WMAPP_TRAYICON;
    g_app.tray_icon.hIcon = g_app.small_icon;

    if (FAILED(StringCchCopyW(
            g_app.tray_icon.szTip,
            sizeof(g_app.tray_icon.szTip) / sizeof(g_app.tray_icon.szTip[0]),
            kAppTitle)))
    {
        return FALSE;
    }

    if (!Shell_NotifyIconW(NIM_ADD, &g_app.tray_icon))
    {
        return FALSE;
    }

    g_app.tray_icon.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &g_app.tray_icon);
    g_app.tray_icon_added = TRUE;
    return TRUE;
}

static void ApplyTrayVisibility(BOOL is_enabled)
{
    if (is_enabled)
    {
        if (!g_app.tray_icon_added)
        {
            if (!InitializeTrayIcon(g_app.window))
            {
                ShowErrorMessage(L"Failed to create the system tray icon.");
                return;
            }
        }
    }
    else
    {
        RemoveTrayIcon();
    }

    g_app.tray_enabled = is_enabled;
}

static void ShowTrayMenu(HWND window)
{
    HMENU menu;
    POINT cursor;
    BOOL is_autostart_enabled = FALSE;
    BOOL autostart_query_ok;
    UINT enable_flags = MF_STRING;
    UINT disable_flags = MF_STRING;

    menu = CreatePopupMenu();
    if (menu == NULL)
    {
        return;
    }

    autostart_query_ok = QueryAutostartCommand(NULL, 0, &is_autostart_enabled);
    if (!autostart_query_ok)
    {
        enable_flags |= MF_GRAYED;
        disable_flags |= MF_GRAYED;
    }
    else if (is_autostart_enabled)
    {
        enable_flags |= MF_GRAYED;
    }
    else
    {
        disable_flags |= MF_GRAYED;
    }

    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_TERMINAL, L"Open Windows Terminal");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, enable_flags, ID_TRAY_ENABLE_AUTOSTART, L"Enable Auto-start");
    AppendMenuW(menu, disable_flags, ID_TRAY_DISABLE_AUTOSTART, L"Disable Auto-start");
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW_STATUS, L"Show Status");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, L"Exit");

    GetCursorPos(&cursor);
    SetForegroundWindow(window);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, window, NULL);
    PostMessageW(window, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static void HandleTrayCommand(HWND window, UINT command_id)
{
    switch (command_id)
    {
    case ID_TRAY_OPEN_TERMINAL:
        if (!LaunchWindowsTerminal())
        {
            ShowErrorMessage(L"Failed to launch Windows Terminal with PowerShell.");
        }
        break;

    case ID_TRAY_ENABLE_AUTOSTART:
        EnableAutostart();
        break;

    case ID_TRAY_DISABLE_AUTOSTART:
        DisableAutostart();
        break;

    case ID_TRAY_SHOW_STATUS:
        ShowAutostartStatus();
        break;

    case ID_TRAY_EXIT:
        DestroyWindow(window);
        break;

    default:
        break;
    }
}

static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    switch (message)
    {
    case WM_COMMAND:
        HandleTrayCommand(window, LOWORD(w_param));
        return 0;

    case WM_HOTKEY:
        if (w_param == HOTKEY_ID && !LaunchWindowsTerminal())
        {
            ShowErrorMessage(L"Failed to launch Windows Terminal with PowerShell.");
        }
        return 0;

    case WMAPP_SET_TRAY_VISIBILITY:
        ApplyTrayVisibility(w_param != 0);
        return 0;

    case WMAPP_TRAYICON:
        switch (LOWORD(l_param))
        {
        case WM_LBUTTONDBLCLK:
            if (!LaunchWindowsTerminal())
            {
                ShowErrorMessage(L"Failed to launch Windows Terminal with PowerShell.");
            }
            return 0;

        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
            ShowTrayMenu(window);
            return 0;

        default:
            return 0;
        }

    case WM_DESTROY:
        UnregisterHotKey(window, HOTKEY_ID);
        g_app.window = NULL;
        RemoveTrayIcon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

static BOOL RegisterMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW window_class;

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = g_app.large_icon;
    window_class.hIconSm = g_app.small_icon;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    return RegisterClassExW(&window_class) != 0;
}

static BOOL InitializeApplicationWindow(HINSTANCE instance)
{
    if (!RegisterMainWindowClass(instance))
    {
        ShowErrorMessage(L"Failed to register the application window class.");
        return FALSE;
    }

    g_app.window = CreateWindowExW(
        0,
        kWindowClassName,
        kAppTitle,
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        NULL,
        NULL,
        instance,
        NULL);

    if (g_app.window == NULL)
    {
        ShowErrorMessage(L"Failed to create the hidden application window.");
        return FALSE;
    }

    return TRUE;
}

static int RunApplicationMessageLoop(void)
{
    MSG message;

    while (GetMessageW(&message, NULL, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return (int)message.wParam;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    AppOptions options;
    int management_exit_code;
    int exit_code;

    (void)previous;
    (void)command_line;
    (void)show_command;

    ZeroMemory(&g_app, sizeof(g_app));
    g_app.instance = instance;

    if (!ParseCommandLine(&options))
    {
        return 1;
    }

    management_exit_code = HandleManagementCommand(&options);
    if (management_exit_code >= 0)
    {
        return management_exit_code;
    }

    g_app.show_startup_notification = options.show_startup_notification;
    g_app.tray_enabled = TRUE;

    g_app.mutex = CreateMutexW(NULL, TRUE, kMutexName);
    if (g_app.mutex == NULL)
    {
        ShowErrorMessage(L"Failed to create the application mutex.");
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 0;
    }

    if (!InitializeAppIcons())
    {
        ShowErrorMessage(L"Failed to load the application icon resources.");
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 1;
    }

    if (!InitializeApplicationWindow(instance))
    {
        DestroyAppIcons();
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 1;
    }

    if (!RegisterHotKey(g_app.window, HOTKEY_ID, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T'))
    {
        ShowErrorMessage(L"Failed to register Ctrl+Alt+T. The hotkey may already be in use.");
        DestroyWindow(g_app.window);
        DestroyAppIcons();
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 1;
    }

    if (!QueryTrayPreference(&g_app.tray_enabled))
    {
        ShowErrorMessage(L"Failed to read the tray visibility setting.");
        UnregisterHotKey(g_app.window, HOTKEY_ID);
        DestroyWindow(g_app.window);
        DestroyAppIcons();
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 1;
    }

    if (g_app.tray_enabled && !InitializeTrayIcon(g_app.window))
    {
        ShowErrorMessage(L"Failed to create the system tray icon.");
        UnregisterHotKey(g_app.window, HOTKEY_ID);
        DestroyWindow(g_app.window);
        DestroyAppIcons();
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
        return 1;
    }

    if (g_app.show_startup_notification && g_app.tray_icon_added)
    {
        ShowTrayNotification(
            L"Quick Terminal",
            L"Quick Terminal is running. Press Ctrl+Alt+T to open Windows Terminal.");
    }

    exit_code = RunApplicationMessageLoop();

    if (g_app.mutex != NULL)
    {
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
    }

    DestroyAppIcons();

    return exit_code;
}
