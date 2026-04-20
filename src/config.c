#include "config.h"
#include "hotkey.h"
#include "qt_strsafe.h"
#include "terminal.h"

#include <shlobj.h>

static wchar_t g_config_directory_path[32768];
static wchar_t g_config_file_path[32768];

static BOOL ReadIniStringWithDefault(
    const wchar_t *section,
    const wchar_t *key,
    const wchar_t *default_value,
    wchar_t *buffer,
    DWORD buffer_count,
    const wchar_t *file_path,
    BOOL *was_missing)
{
    static const wchar_t *kMissingSentinel = L"__QT_MISSING__";
    wchar_t default_copy[2048];

    if (section == NULL || key == NULL || default_value == NULL ||
        buffer == NULL || buffer_count == 0 || file_path == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(
            default_copy,
            sizeof(default_copy) / sizeof(default_copy[0]),
            default_value)))
    {
        return FALSE;
    }

    GetPrivateProfileStringW(
        section,
        key,
        kMissingSentinel,
        buffer,
        buffer_count,
        file_path);

    if (lstrcmpW(buffer, kMissingSentinel) == 0)
    {
        if (was_missing != NULL)
        {
            *was_missing = TRUE;
        }

        return CopyConfigString(buffer, buffer_count, default_copy);
    }

    if (was_missing != NULL)
    {
        *was_missing = FALSE;
    }

    return TRUE;
}

void SetDefaultConfig(AppConfig *config)
{
    if (config == NULL)
    {
        return;
    }

    config->show_tray = TRUE;
    config->show_startup_notification = TRUE;
    CopyConfigString(config->terminal_mode, sizeof(config->terminal_mode) / sizeof(config->terminal_mode[0]), kDefaultTerminalMode);
    CopyConfigString(config->terminal_command, sizeof(config->terminal_command) / sizeof(config->terminal_command[0]), kDefaultTerminalCommand);
    CopyConfigString(config->terminal_arguments, sizeof(config->terminal_arguments) / sizeof(config->terminal_arguments[0]), kDefaultTerminalArguments);
    CopyConfigString(config->hotkey_modifiers, sizeof(config->hotkey_modifiers) / sizeof(config->hotkey_modifiers[0]), kDefaultHotkeyModifiers);
    CopyConfigString(config->hotkey_key, sizeof(config->hotkey_key) / sizeof(config->hotkey_key[0]), kDefaultHotkeyKey);
}

static BOOL GetAppDataPath(wchar_t *buffer, size_t buffer_count)
{
    wchar_t appdata_path[MAX_PATH];

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata_path)))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchCopyW(buffer, buffer_count, appdata_path));
}

BOOL BuildConfigDirectoryPathFromName(const wchar_t *directory_name, wchar_t *buffer, size_t buffer_count)
{
    wchar_t appdata_path[32768];

    if (directory_name == NULL || directory_name[0] == L'\0')
    {
        return FALSE;
    }

    if (!GetAppDataPath(appdata_path, sizeof(appdata_path) / sizeof(appdata_path[0])))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"%s\\%s",
        appdata_path,
        directory_name));
}

static BOOL SetResolvedConfigDirectoryPath(const wchar_t *directory_path)
{
    DWORD full_path_length;

    if (directory_path == NULL || directory_path[0] == L'\0')
    {
        return FALSE;
    }

    full_path_length = GetFullPathNameW(
        directory_path,
        sizeof(g_config_directory_path) / sizeof(g_config_directory_path[0]),
        g_config_directory_path,
        NULL);
    if (full_path_length == 0 || full_path_length >= (sizeof(g_config_directory_path) / sizeof(g_config_directory_path[0])))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        g_config_file_path,
        sizeof(g_config_file_path) / sizeof(g_config_file_path[0]),
        L"%s\\%s",
        g_config_directory_path,
        kConfigFileName));
}

static BOOL GetDefaultConfigDirectoryPath(wchar_t *buffer, size_t buffer_count)
{
    return BuildConfigDirectoryPathFromName(kConfigDirectoryName, buffer, buffer_count);
}

static BOOL ReadStoredConfigDirectory(wchar_t *buffer, size_t buffer_count, BOOL *has_value)
{
    HKEY key = NULL;
    DWORD type = 0;
    DWORD size_bytes;
    LONG status;

    if (buffer == NULL || buffer_count == 0 || has_value == NULL)
    {
        return FALSE;
    }

    *has_value = FALSE;

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

    size_bytes = (DWORD)(buffer_count * sizeof(wchar_t));
    status = RegQueryValueExW(
        key,
        kConfigDirectoryValueName,
        NULL,
        &type,
        (LPBYTE)buffer,
        &size_bytes);
    RegCloseKey(key);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        return TRUE;
    }

    if (status != ERROR_SUCCESS || type != REG_SZ)
    {
        return FALSE;
    }

    *has_value = (buffer[0] != L'\0');
    return TRUE;
}

BOOL WriteStoredConfigDirectory(const wchar_t *directory_path)
{
    HKEY key = NULL;
    LONG status;
    DWORD size_bytes;

    if (directory_path == NULL || directory_path[0] == L'\0')
    {
        return FALSE;
    }

    status = RegCreateKeyExW(
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
        return FALSE;
    }

    size_bytes = (DWORD)((lstrlenW(directory_path) + 1) * sizeof(wchar_t));
    status = RegSetValueExW(
        key,
        kConfigDirectoryValueName,
        0,
        REG_SZ,
        (const BYTE *)directory_path,
        size_bytes);
    RegCloseKey(key);
    return (status == ERROR_SUCCESS);
}

const wchar_t *GetResolvedConfigDirectoryPath(void)
{
    return g_config_directory_path;
}

BOOL ResolveConfigDirectory(const AppOptions *options)
{
    wchar_t stored_directory[32768];
    BOOL has_stored_directory = FALSE;
    wchar_t default_directory[32768];

    if (options != NULL && options->has_custom_config_directory)
    {
        return SetResolvedConfigDirectoryPath(options->custom_config_directory);
    }

    if (!ReadStoredConfigDirectory(
            stored_directory,
            sizeof(stored_directory) / sizeof(stored_directory[0]),
            &has_stored_directory))
    {
        return FALSE;
    }

    if (has_stored_directory)
    {
        return SetResolvedConfigDirectoryPath(stored_directory);
    }

    if (!GetDefaultConfigDirectoryPath(default_directory, sizeof(default_directory) / sizeof(default_directory[0])))
    {
        return FALSE;
    }

    return SetResolvedConfigDirectoryPath(default_directory);
}

static BOOL ReadLegacyTrayPreference(BOOL *is_enabled, BOOL *has_value)
{
    HKEY key = NULL;
    DWORD type = 0;
    DWORD value = 0;
    DWORD size = sizeof(value);
    LONG status;

    if (is_enabled == NULL || has_value == NULL)
    {
        return FALSE;
    }

    *is_enabled = TRUE;
    *has_value = FALSE;

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

    *has_value = TRUE;
    *is_enabled = (value != 0);
    return TRUE;
}

static BOOL IsDirectoryEmpty(const wchar_t *directory_path, BOOL *is_empty)
{
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    wchar_t search_pattern[32768];

    if (directory_path == NULL || is_empty == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            search_pattern,
            sizeof(search_pattern) / sizeof(search_pattern[0]),
            L"%s\\*",
            directory_path)))
    {
        return FALSE;
    }

    find_handle = FindFirstFileW(search_pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        return (error == ERROR_FILE_NOT_FOUND) ? (*is_empty = TRUE, TRUE) : FALSE;
    }

    *is_empty = TRUE;
    do
    {
        if (lstrcmpW(find_data.cFileName, L".") != 0 &&
            lstrcmpW(find_data.cFileName, L"..") != 0)
        {
            *is_empty = FALSE;
            break;
        }
    } while (FindNextFileW(find_handle, &find_data));

    FindClose(find_handle);
    return TRUE;
}

static BOOL EnsureConfigDirectoryExists(void)
{
    DWORD attributes = GetFileAttributesW(g_config_directory_path);
    int create_result;

    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        return ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
    }

    create_result = SHCreateDirectoryExW(NULL, g_config_directory_path, NULL);
    return (create_result == ERROR_SUCCESS ||
            create_result == ERROR_FILE_EXISTS ||
            create_result == ERROR_ALREADY_EXISTS);
}

static void ShowConfigDirectoryConflictMessage(void)
{
    wchar_t message[4096];

    if (FAILED(StringCchPrintfW(
            message,
            sizeof(message) / sizeof(message[0]),
            L"The config directory already exists and is not empty, but no Quick Terminal config file was found.\n\n"
            L"Directory:\n%s\n\n"
            L"Please use --config-dir-name <name> or --config-dir <path> to choose a different config directory.",
            g_config_directory_path)))
    {
        ShowErrorMessage(L"The config directory already exists and is not empty, but no Quick Terminal config file was found.");
        return;
    }

    ShowErrorMessage(message);
}

BOOL SaveConfig(const AppConfig *config)
{
    if (config == NULL)
    {
        return FALSE;
    }

    if (!EnsureConfigDirectoryExists())
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"general",
            L"show_tray",
            config->show_tray ? L"true" : L"false",
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"general",
            L"show_startup_notification",
            config->show_startup_notification ? L"true" : L"false",
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"terminal",
            L"mode",
            config->terminal_mode,
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"terminal",
            L"command",
            config->terminal_command,
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"terminal",
            L"arguments",
            config->terminal_arguments,
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"hotkey",
            L"modifiers",
            config->hotkey_modifiers,
            g_config_file_path))
    {
        return FALSE;
    }

    if (!WritePrivateProfileStringW(
            L"hotkey",
            L"key",
            config->hotkey_key,
            g_config_file_path))
    {
        return FALSE;
    }

    return TRUE;
}

BOOL LoadConfig(AppConfig *config)
{
    DWORD config_attributes;
    DWORD directory_attributes;
    BOOL legacy_tray_value = TRUE;
    BOOL has_legacy_tray_value = FALSE;
    wchar_t value_buffer[32];
    BOOL is_directory_empty = TRUE;
    BOOL config_needs_save = FALSE;
    BOOL was_missing = FALSE;

    if (config == NULL)
    {
        return FALSE;
    }

    SetDefaultConfig(config);

    if (!ReadLegacyTrayPreference(&legacy_tray_value, &has_legacy_tray_value))
    {
        return FALSE;
    }

    config_attributes = GetFileAttributesW(g_config_file_path);
    if (config_attributes == INVALID_FILE_ATTRIBUTES)
    {
        DWORD config_error = GetLastError();

        if (config_error != ERROR_FILE_NOT_FOUND && config_error != ERROR_PATH_NOT_FOUND)
        {
            return FALSE;
        }

        directory_attributes = GetFileAttributesW(g_config_directory_path);
        if (directory_attributes != INVALID_FILE_ATTRIBUTES &&
            (directory_attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            if (!IsDirectoryEmpty(g_config_directory_path, &is_directory_empty))
            {
                return FALSE;
            }

            if (!is_directory_empty)
            {
                ShowConfigDirectoryConflictMessage();
                return FALSE;
            }
        }
        else if (directory_attributes != INVALID_FILE_ATTRIBUTES)
        {
            ShowConfigDirectoryConflictMessage();
            return FALSE;
        }

        if (has_legacy_tray_value)
        {
            config->show_tray = legacy_tray_value;
        }

        return SaveConfig(config);
    }

    if (!ReadIniStringWithDefault(
            L"general",
            L"show_tray",
            config->show_tray ? L"true" : L"false",
            value_buffer,
            sizeof(value_buffer) / sizeof(value_buffer[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);
    config->show_tray = (lstrcmpiW(value_buffer, L"false") != 0 && lstrcmpiW(value_buffer, L"0") != 0);

    if (!ReadIniStringWithDefault(
            L"general",
            L"show_startup_notification",
            config->show_startup_notification ? L"true" : L"false",
            value_buffer,
            sizeof(value_buffer) / sizeof(value_buffer[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);
    config->show_startup_notification = (lstrcmpiW(value_buffer, L"false") != 0 && lstrcmpiW(value_buffer, L"0") != 0);

    if (!ReadIniStringWithDefault(
            L"terminal",
            L"mode",
            config->terminal_mode,
            config->terminal_mode,
            sizeof(config->terminal_mode) / sizeof(config->terminal_mode[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);

    if (!ReadIniStringWithDefault(
            L"terminal",
            L"command",
            config->terminal_command,
            config->terminal_command,
            sizeof(config->terminal_command) / sizeof(config->terminal_command[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);

    if (!ReadIniStringWithDefault(
            L"terminal",
            L"arguments",
            config->terminal_arguments,
            config->terminal_arguments,
            sizeof(config->terminal_arguments) / sizeof(config->terminal_arguments[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);

    if (!ReadIniStringWithDefault(
            L"hotkey",
            L"modifiers",
            config->hotkey_modifiers,
            config->hotkey_modifiers,
            sizeof(config->hotkey_modifiers) / sizeof(config->hotkey_modifiers[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);

    if (!ReadIniStringWithDefault(
            L"hotkey",
            L"key",
            config->hotkey_key,
            config->hotkey_key,
            sizeof(config->hotkey_key) / sizeof(config->hotkey_key[0]),
            g_config_file_path,
            &was_missing))
    {
        return FALSE;
    }
    config_needs_save = (config_needs_save || was_missing);

    if (config_needs_save)
    {
        return SaveConfig(config);
    }

    return TRUE;
}

BOOL ValidateConfig(const AppConfig *config)
{
    UINT modifiers = 0;
    UINT virtual_key = 0;

    if (config == NULL)
    {
        return FALSE;
    }

    if (config->terminal_command[0] == L'\0')
    {
        ShowErrorMessage(L"The terminal command in config.ini cannot be empty.");
        return FALSE;
    }

    if (!IsSupportedTerminalMode(config->terminal_mode))
    {
        ShowErrorMessage(L"The terminal mode in config.ini is invalid. Use terminal-only or terminal-with-powershell.");
        return FALSE;
    }

    if (!ParseHotkeyModifiersString(config->hotkey_modifiers, &modifiers))
    {
        ShowErrorMessage(L"The hotkey modifiers in config.ini are invalid.");
        return FALSE;
    }

    if (!ParseHotkeyKeyString(config->hotkey_key, &virtual_key))
    {
        ShowErrorMessage(L"The hotkey key in config.ini is invalid.");
        return FALSE;
    }

    return TRUE;
}
