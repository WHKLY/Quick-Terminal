#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <propsys.h>
#include <strsafe.h>
#include <wincrypt.h>

#include "resource.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4 4
#endif

#ifndef CRYPT_STRING_NOCRLF
#define CRYPT_STRING_NOCRLF 0x40000000
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
    COMMAND_TEST_NOTIFICATION,
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
static const wchar_t *kToastAppUserModelId = L"QuickTerminal.App";
static const wchar_t *kToastShortcutName = L"Quick Terminal.lnk";
static const wchar_t *kUsageText =
    L"Quick Terminal\n\n"
    L"Supported arguments:\n"
    L"--enable-autostart\n"
    L"--disable-autostart\n"
    L"--autostart-status\n"
    L"--show-tray\n"
    L"--hide-tray\n"
    L"--tray-status\n"
    L"--test-notification\n"
    L"--help";

static AppState g_app;

static BOOL ShowTrayNotification(const wchar_t *title, const wchar_t *message);

static void ShowInfoMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONINFORMATION);
}

static void ShowErrorMessage(const wchar_t *message)
{
    MessageBoxW(NULL, message, kAppTitle, MB_OK | MB_ICONERROR);
}

static BOOL GetExecutablePath(wchar_t *buffer, size_t buffer_count)
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

static BOOL GetExecutableDirectory(wchar_t *buffer, size_t buffer_count)
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

static HRESULT InitPropVariantFromWideStringValue(const wchar_t *value, PROPVARIANT *variant)
{
    size_t value_length_bytes;

    if (value == NULL || variant == NULL)
    {
        return E_INVALIDARG;
    }

    PropVariantInit(variant);
    value_length_bytes = ((size_t)lstrlenW(value) + 1U) * sizeof(wchar_t);
    variant->pwszVal = (LPWSTR)CoTaskMemAlloc(value_length_bytes);
    if (variant->pwszVal == NULL)
    {
        return E_OUTOFMEMORY;
    }

    CopyMemory(variant->pwszVal, value, value_length_bytes);
    variant->vt = VT_LPWSTR;
    return S_OK;
}

static BOOL GetToastShortcutPath(wchar_t *buffer, size_t buffer_count)
{
    wchar_t programs_path[MAX_PATH];

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, programs_path)))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"%s\\%s",
        programs_path,
        kToastShortcutName));
}

static BOOL EnsureToastShortcutInstalled(void)
{
    HRESULT hr;
    BOOL should_uninitialize = FALSE;
    IShellLinkW *shell_link = NULL;
    IPropertyStore *property_store = NULL;
    IPersistFile *persist_file = NULL;
    PROPVARIANT app_id;
    wchar_t executable_path[32768];
    wchar_t executable_directory[32768];
    wchar_t shortcut_path[32768];
    BOOL success = FALSE;

    if (!GetExecutablePath(executable_path, sizeof(executable_path) / sizeof(executable_path[0])) ||
        !GetExecutableDirectory(executable_directory, sizeof(executable_directory) / sizeof(executable_directory[0])) ||
        !GetToastShortcutPath(shortcut_path, sizeof(shortcut_path) / sizeof(shortcut_path[0])))
    {
        return FALSE;
    }

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        should_uninitialize = TRUE;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        return FALSE;
    }

    hr = CoCreateInstance(
        &CLSID_ShellLink,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IShellLinkW,
        (void **)&shell_link);
    if (FAILED(hr) || shell_link == NULL)
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetPath(shell_link, executable_path);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetArguments(shell_link, L"");
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetWorkingDirectory(shell_link, executable_directory);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetDescription(shell_link, kAppTitle);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetIconLocation(shell_link, executable_path, 0);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->QueryInterface(
        shell_link,
        &IID_IPropertyStore,
        (void **)&property_store);
    if (FAILED(hr) || property_store == NULL)
    {
        goto cleanup;
    }

    hr = InitPropVariantFromWideStringValue(kToastAppUserModelId, &app_id);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = property_store->lpVtbl->SetValue(property_store, &PKEY_AppUserModel_ID, &app_id);
    PropVariantClear(&app_id);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = property_store->lpVtbl->Commit(property_store);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->QueryInterface(
        shell_link,
        &IID_IPersistFile,
        (void **)&persist_file);
    if (FAILED(hr) || persist_file == NULL)
    {
        goto cleanup;
    }

    hr = persist_file->lpVtbl->Save(persist_file, shortcut_path, TRUE);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    success = TRUE;

cleanup:
    if (persist_file != NULL)
    {
        persist_file->lpVtbl->Release(persist_file);
    }

    if (property_store != NULL)
    {
        property_store->lpVtbl->Release(property_store);
    }

    if (shell_link != NULL)
    {
        shell_link->lpVtbl->Release(shell_link);
    }

    if (should_uninitialize)
    {
        CoUninitialize();
    }

    return success;
}

static BOOL EncodePowerShellCommand(const wchar_t *script, wchar_t *buffer, DWORD *buffer_count)
{
    DWORD characters_required = 0;
    const BYTE *script_bytes = (const BYTE *)script;
    DWORD script_bytes_length;

    if (script == NULL || buffer == NULL || buffer_count == NULL)
    {
        return FALSE;
    }

    script_bytes_length = (DWORD)(lstrlenW(script) * sizeof(wchar_t));

    if (!CryptBinaryToStringW(
            script_bytes,
            script_bytes_length,
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            NULL,
            &characters_required))
    {
        return FALSE;
    }

    if (*buffer_count < characters_required)
    {
        *buffer_count = characters_required;
        return FALSE;
    }

    if (!CryptBinaryToStringW(
            script_bytes,
            script_bytes_length,
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            buffer,
            &characters_required))
    {
        return FALSE;
    }

    *buffer_count = characters_required;
    return TRUE;
}

static BOOL RunHiddenProcessAndWait(const wchar_t *command_line, DWORD timeout_ms)
{
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    wchar_t mutable_command[32768];
    DWORD wait_result;
    DWORD exit_code = 1;

    if (command_line == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(
            mutable_command,
            sizeof(mutable_command) / sizeof(mutable_command[0]),
            command_line)))
    {
        return FALSE;
    }

    ZeroMemory(&startup_info, sizeof(startup_info));
    ZeroMemory(&process_info, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_HIDE;

    if (!CreateProcessW(
            NULL,
            mutable_command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info))
    {
        return FALSE;
    }

    wait_result = WaitForSingleObject(process_info.hProcess, timeout_ms);
    if (wait_result == WAIT_OBJECT_0)
    {
        GetExitCodeProcess(process_info.hProcess, &exit_code);
    }
    else
    {
        TerminateProcess(process_info.hProcess, 1);
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return (wait_result == WAIT_OBJECT_0 && exit_code == 0);
}

static BOOL ShowToastNotification(const wchar_t *title, const wchar_t *message)
{
    wchar_t system_directory[MAX_PATH];
    wchar_t powershell_path[32768];
    wchar_t script[8192];
    wchar_t encoded_script[16384];
    DWORD encoded_script_length = sizeof(encoded_script) / sizeof(encoded_script[0]);
    wchar_t command_line[32768];

    if (title == NULL || message == NULL)
    {
        return FALSE;
    }

    if (!EnsureToastShortcutInstalled())
    {
        return FALSE;
    }

    if (GetSystemDirectoryW(system_directory, sizeof(system_directory) / sizeof(system_directory[0])) == 0)
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            powershell_path,
            sizeof(powershell_path) / sizeof(powershell_path[0]),
            L"%s\\WindowsPowerShell\\v1.0\\powershell.exe",
            system_directory)))
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            script,
            sizeof(script) / sizeof(script[0]),
            L"$ErrorActionPreference='Stop';"
            L"[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null;"
            L"[Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType = WindowsRuntime] > $null;"
            L"$xml = New-Object Windows.Data.Xml.Dom.XmlDocument;"
            L"$xml.LoadXml(\"<toast><visual><binding template='ToastGeneric'><text>%s</text><text>%s</text></binding></visual></toast>\");"
            L"$toast = [Windows.UI.Notifications.ToastNotification]::new($xml);"
            L"[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('%s').Show($toast);"
            L"exit 0;",
            title,
            message,
            kToastAppUserModelId)))
    {
        return FALSE;
    }

    if (!EncodePowerShellCommand(script, encoded_script, &encoded_script_length))
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            command_line,
            sizeof(command_line) / sizeof(command_line[0]),
            L"\"%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand %s",
            powershell_path,
            encoded_script)))
    {
        return FALSE;
    }

    return RunHiddenProcessAndWait(command_line, 10000);
}

static BOOL ShowStartupNotification(void)
{
    if (ShowToastNotification(
            L"Quick Terminal",
            L"Quick Terminal is running. Press Ctrl+Alt+T to open Windows Terminal."))
    {
        return TRUE;
    }

    if (g_app.tray_icon_added)
    {
        return ShowTrayNotification(
            L"Quick Terminal",
            L"Quick Terminal is running. Press Ctrl+Alt+T to open Windows Terminal.");
    }

    return FALSE;
}

static void ShowInfoNotificationWithFallback(const wchar_t *message)
{
    if (ShowToastNotification(kAppTitle, message))
    {
        return;
    }

    if (g_app.tray_icon_added && ShowTrayNotification(kAppTitle, message))
    {
        return;
    }

    ShowInfoMessage(message);
}

static void ShowStatusNotificationWithFallback(const wchar_t *toast_message, const wchar_t *fallback_message)
{
    if (toast_message == NULL)
    {
        return;
    }

    if (ShowToastNotification(kAppTitle, toast_message))
    {
        return;
    }

    if (g_app.tray_icon_added && ShowTrayNotification(kAppTitle, toast_message))
    {
        return;
    }

    ShowInfoMessage(fallback_message != NULL ? fallback_message : toast_message);
}

static BOOL TestModernNotification(void)
{
    if (ShowToastNotification(
            L"Quick Terminal",
            L"This is a test notification from Quick Terminal."))
    {
        return TRUE;
    }

    ShowErrorMessage(L"Failed to show the Windows toast notification.");
    return FALSE;
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

    ShowStatusNotificationWithFallback(
        is_enabled
            ? L"Tray icon display is enabled."
            : L"Tray icon display is disabled.",
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
    ShowInfoNotificationWithFallback(L"Tray icon display has been enabled.");
    return TRUE;
}

static BOOL DisableTrayPreference(void)
{
    if (!SetTrayPreference(FALSE))
    {
        return FALSE;
    }

    SyncRunningInstanceTrayVisibility(FALSE);
    ShowInfoNotificationWithFallback(L"Tray icon display has been disabled.");
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

    ShowInfoNotificationWithFallback(L"Auto-start has been enabled for the current user.");
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
        ShowInfoNotificationWithFallback(L"Auto-start is already disabled.");
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
        ShowInfoNotificationWithFallback(L"Auto-start is already disabled.");
        return TRUE;
    }

    if (status != ERROR_SUCCESS)
    {
        ShowErrorMessage(L"Failed to remove the auto-start registry value.");
        return FALSE;
    }

    ShowInfoNotificationWithFallback(L"Auto-start has been disabled for the current user.");
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
        ShowStatusNotificationWithFallback(L"Auto-start is disabled.", L"Auto-start is disabled.");
        return TRUE;
    }

    if (FAILED(StringCchPrintfW(message, sizeof(message) / sizeof(message[0]), L"Auto-start is enabled.\n\nCommand:\n%s", command)))
    {
        ShowStatusNotificationWithFallback(L"Auto-start is enabled.", L"Auto-start is enabled.");
        return TRUE;
    }

    ShowStatusNotificationWithFallback(L"Auto-start is enabled.", message);
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
        else if (lstrcmpiW(argv[i], L"--test-notification") == 0)
        {
            options->command_mode = COMMAND_TEST_NOTIFICATION;
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
