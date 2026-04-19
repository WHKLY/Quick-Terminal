#include "app.h"
#include "autostart.h"
#include "config.h"
#include "hotkey.h"
#include "notifications.h"
#include "qt_strsafe.h"
#include "terminal.h"
#include "tray.h"

#include <stdlib.h>

AppState g_app;
AppConfig g_config;
BOOL g_error_message_shown = FALSE;

const wchar_t *kAppTitle = L"Quick Terminal";
const wchar_t *kWindowClassName = L"QuickTerminalHiddenWindow";
const wchar_t *kMutexName = L"QuickTerminalHotkeySingleton";
const wchar_t *kDefaultTerminalCommand = L"wt.exe";
const wchar_t *kDefaultTerminalArguments = L"new-tab powershell.exe";
const wchar_t *kDefaultHotkeyModifiers = L"Ctrl+Alt";
const wchar_t *kDefaultHotkeyKey = L"T";
const wchar_t *kRunKeyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t *kRunValueName = L"QuickTerminalHotkey";
const wchar_t *kSettingsKeyPath = L"Software\\QuickTerminal";
const wchar_t *kTrayEnabledValueName = L"ShowTrayIcon";
const wchar_t *kConfigDirectoryValueName = L"ConfigDirectory";
const wchar_t *kConfigDirectoryName = L"QuickTerminal";
const wchar_t *kConfigFileName = L"config.ini";
const wchar_t *kToastAppUserModelId = L"QuickTerminal.App";
const wchar_t *kToastShortcutName = L"Quick Terminal.lnk";
const wchar_t *kUsageText =
    L"Quick Terminal\n\n"
    L"Supported arguments:\n"
    L"--enable-autostart\n"
    L"--disable-autostart\n"
    L"--autostart-status\n"
    L"--show-tray\n"
    L"--hide-tray\n"
    L"--tray-status\n"
    L"--test-notification\n"
    L"--config-dir <path>\n"
    L"--config-dir-name <name>\n"
    L"--help";

BOOL CopyConfigString(wchar_t *buffer, size_t buffer_count, const wchar_t *value)
{
    return SUCCEEDED(StringCchCopyW(buffer, buffer_count, value));
}

void TrimWhitespaceInPlace(wchar_t *value)
{
    wchar_t *end;

    if (value == NULL || value[0] == L'\0')
    {
        return;
    }

    while (*value == L' ' || *value == L'\t' || *value == L'\r' || *value == L'\n')
    {
        MoveMemory(value, value + 1, (lstrlenW(value) * sizeof(wchar_t)));
    }

    end = value + lstrlenW(value);
    while (end > value &&
           (end[-1] == L' ' || end[-1] == L'\t' || end[-1] == L'\r' || end[-1] == L'\n'))
    {
        --end;
    }

    *end = L'\0';
}

void ShowInfoMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONINFORMATION);
}

void ShowErrorMessage(const wchar_t *message)
{
    g_error_message_shown = TRUE;
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONERROR);
}

BOOL GetExecutablePath(wchar_t *buffer, size_t buffer_count)
{
    DWORD length;

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    length = GetModuleFileNameW(NULL, buffer, (DWORD)buffer_count);
    if (length == 0 || length >= buffer_count)
    {
        return FALSE;
    }

    return TRUE;
}

BOOL GetExecutableDirectory(wchar_t *buffer, size_t buffer_count)
{
    wchar_t path[32768];
    wchar_t *last_separator;

    if (!GetExecutablePath(path, sizeof(path) / sizeof(path[0])))
    {
        return FALSE;
    }

    last_separator = wcsrchr(path, L'\\');
    if (last_separator == NULL)
    {
        return FALSE;
    }

    *last_separator = L'\0';
    return SUCCEEDED(StringCchCopyW(buffer, buffer_count, path));
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
    options->has_custom_config_directory = FALSE;
    options->persist_custom_config_directory = FALSE;
    options->custom_config_directory[0] = L'\0';

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
        else if (lstrcmpiW(argv[i], L"--test-notification") == 0)
        {
            options->command_mode = COMMAND_TEST_NOTIFICATION;
        }
        else if (lstrcmpiW(argv[i], L"--config-dir") == 0)
        {
            if (i + 1 >= argc ||
                FAILED(StringCchCopyW(
                    options->custom_config_directory,
                    sizeof(options->custom_config_directory) / sizeof(options->custom_config_directory[0]),
                    argv[++i])))
            {
                LocalFree(argv);
                ShowErrorMessage(L"Missing or invalid value for --config-dir.");
                return FALSE;
            }

            options->has_custom_config_directory = TRUE;
            options->persist_custom_config_directory = TRUE;
        }
        else if (lstrcmpiW(argv[i], L"--config-dir-name") == 0)
        {
            if (i + 1 >= argc ||
                !BuildConfigDirectoryPathFromName(
                    argv[++i],
                    options->custom_config_directory,
                    sizeof(options->custom_config_directory) / sizeof(options->custom_config_directory[0])))
            {
                LocalFree(argv);
                ShowErrorMessage(L"Missing or invalid value for --config-dir-name.");
                return FALSE;
            }

            options->has_custom_config_directory = TRUE;
            options->persist_custom_config_directory = TRUE;
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

    case COMMAND_TEST_NOTIFICATION:
        return TestModernNotification() ? 0 : 1;

    case COMMAND_HELP:
        ShowInfoMessage(kUsageText);
        return 0;

    case COMMAND_NONE:
    default:
        return -1;
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

static void CleanupStartupFailure(BOOL destroy_window)
{
    if (destroy_window && g_app.window != NULL)
    {
        DestroyWindow(g_app.window);
    }

    DestroyAppIcons();

    if (g_app.mutex != NULL)
    {
        CloseHandle(g_app.mutex);
        g_app.mutex = NULL;
    }
}

static BOOL RegisterConfiguredHotkey(void)
{
    UINT hotkey_modifiers = 0;
    UINT hotkey_virtual_key = 0;

    if (!ParseHotkeyModifiersString(g_config.hotkey_modifiers, &hotkey_modifiers) ||
        !ParseHotkeyKeyString(g_config.hotkey_key, &hotkey_virtual_key))
    {
        ShowErrorMessage(L"Failed to parse the hotkey settings from config.ini.");
        return FALSE;
    }

    if (!RegisterHotKey(g_app.window, HOTKEY_ID, hotkey_modifiers | MOD_NOREPEAT, hotkey_virtual_key))
    {
        ShowErrorMessage(L"Failed to register the configured global hotkey. It may already be in use.");
        return FALSE;
    }

    return TRUE;
}

int AppMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
    AppOptions options;
    int management_exit_code;
    int exit_code;

    (void)previous;
    (void)command_line;
    (void)show_command;

    ZeroMemory(&g_app, sizeof(g_app));
    g_app.instance = instance;
    g_error_message_shown = FALSE;
    SetDefaultConfig(&g_config);

    if (!ParseCommandLine(&options))
    {
        return 1;
    }

    if (!ResolveConfigDirectory(&options))
    {
        ShowErrorMessage(L"Failed to resolve the configuration directory.");
        return 1;
    }

    if (!LoadConfig(&g_config))
    {
        if (!g_error_message_shown)
        {
            ShowErrorMessage(L"Failed to load the configuration file.");
        }
        return 1;
    }

    if (options.persist_custom_config_directory &&
        !WriteStoredConfigDirectory(GetResolvedConfigDirectoryPath()))
    {
        ShowErrorMessage(L"Failed to store the selected configuration directory.");
        return 1;
    }

    if (!ValidateConfig(&g_config))
    {
        return 1;
    }

    management_exit_code = HandleManagementCommand(&options);
    if (management_exit_code >= 0)
    {
        return management_exit_code;
    }

    g_app.show_startup_notification = g_config.show_startup_notification || options.show_startup_notification;
    g_app.tray_enabled = g_config.show_tray;

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
        CleanupStartupFailure(FALSE);
        return 1;
    }

    if (!InitializeApplicationWindow(instance))
    {
        CleanupStartupFailure(FALSE);
        return 1;
    }

    if (!RegisterConfiguredHotkey())
    {
        CleanupStartupFailure(TRUE);
        return 1;
    }

    if (g_app.tray_enabled && !InitializeTrayIcon(g_app.window))
    {
        ShowErrorMessage(L"Failed to create the system tray icon.");
        CleanupStartupFailure(TRUE);
        return 1;
    }

    if (g_app.show_startup_notification)
    {
        ShowStartupNotification();
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
