#include "autostart.h"
#include "notifications.h"
#include "qt_strsafe.h"

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

BOOL QueryAutostartCommand(wchar_t *buffer, DWORD buffer_size_bytes, BOOL *is_enabled)
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

BOOL EnableAutostart(void)
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

BOOL DisableAutostart(void)
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

BOOL ShowAutostartStatus(void)
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
