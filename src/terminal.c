#include "terminal.h"
#include "config.h"
#include "notifications.h"
#include "qt_strsafe.h"

#include <wincrypt.h>

#ifndef CRYPT_STRING_NOCRLF
#define CRYPT_STRING_NOCRLF 0x40000000
#endif

static const wchar_t *kLastDirectoryStateFileName = L"last-directory.txt";

static BOOL GetLastDirectoryStateFilePath(wchar_t *buffer, size_t buffer_count)
{
    const wchar_t *config_directory = GetResolvedConfigDirectoryPath();

    if (buffer == NULL || buffer_count == 0 || config_directory == NULL || config_directory[0] == L'\0')
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"%s\\%s",
        config_directory,
        kLastDirectoryStateFileName));
}

static BOOL LoadLastWorkingDirectory(wchar_t *buffer, size_t buffer_count)
{
    HANDLE file_handle;
    DWORD bytes_read = 0;
    DWORD bytes_to_read;
    wchar_t *text;
    wchar_t state_file_path[32768];

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    buffer[0] = L'\0';

    if (!GetLastDirectoryStateFilePath(state_file_path, sizeof(state_file_path) / sizeof(state_file_path[0])))
    {
        return FALSE;
    }

    file_handle = CreateFileW(
        state_file_path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    bytes_to_read = (DWORD)((buffer_count - 1) * sizeof(wchar_t));
    if (!ReadFile(file_handle, buffer, bytes_to_read, &bytes_read, NULL))
    {
        CloseHandle(file_handle);
        buffer[0] = L'\0';
        return FALSE;
    }

    CloseHandle(file_handle);

    buffer[bytes_read / sizeof(wchar_t)] = L'\0';
    text = buffer;
    if (text[0] == 0xFEFF)
    {
        MoveMemory(text, text + 1, lstrlenW(text) * sizeof(wchar_t));
    }

    TrimWhitespaceInPlace(text);

    if (text[0] == L'\0')
    {
        return FALSE;
    }

    if (GetFileAttributesW(text) == INVALID_FILE_ATTRIBUTES)
    {
        buffer[0] = L'\0';
        return FALSE;
    }

    return TRUE;
}

static BOOL EscapePowerShellSingleQuotedString(
    const wchar_t *input,
    wchar_t *buffer,
    size_t buffer_count)
{
    const wchar_t *cursor;
    wchar_t *write_cursor;
    size_t remaining;

    if (input == NULL || buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    cursor = input;
    write_cursor = buffer;
    remaining = buffer_count;

    while (*cursor != L'\0')
    {
        if (remaining <= 1)
        {
            return FALSE;
        }

        if (*cursor == L'\'')
        {
            if (remaining <= 2)
            {
                return FALSE;
            }

            *write_cursor++ = L'\'';
            *write_cursor++ = L'\'';
            remaining -= 2;
        }
        else
        {
            *write_cursor++ = *cursor;
            remaining -= 1;
        }

        ++cursor;
    }

    *write_cursor = L'\0';
    return TRUE;
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

static BOOL BuildTrackedPowerShellArguments(wchar_t *buffer, size_t buffer_count)
{
    wchar_t state_file_path[32768];
    wchar_t state_file_path_escaped[32768];
    wchar_t start_directory[32768];
    wchar_t start_directory_escaped[32768];
    wchar_t script[8192];
    wchar_t encoded_script[16384];
    DWORD encoded_script_length = sizeof(encoded_script) / sizeof(encoded_script[0]);
    BOOL has_start_directory = FALSE;

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    if (!GetLastDirectoryStateFilePath(state_file_path, sizeof(state_file_path) / sizeof(state_file_path[0])) ||
        !EscapePowerShellSingleQuotedString(
            state_file_path,
            state_file_path_escaped,
            sizeof(state_file_path_escaped) / sizeof(state_file_path_escaped[0])))
    {
        return FALSE;
    }

    has_start_directory = LoadLastWorkingDirectory(
        start_directory,
        sizeof(start_directory) / sizeof(start_directory[0]));

    if (has_start_directory &&
        !EscapePowerShellSingleQuotedString(
            start_directory,
            start_directory_escaped,
            sizeof(start_directory_escaped) / sizeof(start_directory_escaped[0])))
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            script,
            sizeof(script) / sizeof(script[0]),
            has_start_directory
                ? L"$global:__qtStatePath='%s';"
                  L"$startPath='%s';"
                  L"$global:__qtOriginalPrompt=$function:prompt;"
                  L"function global:prompt { try { $path=(Get-Location).ProviderPath; if (-not [string]::IsNullOrWhiteSpace($path)) { Set-Content -LiteralPath $global:__qtStatePath -Value $path -Encoding Unicode -Force } } catch {} if ($null -ne $global:__qtOriginalPrompt) { & $global:__qtOriginalPrompt } else { 'PS ' + $(Get-Location) + '> ' } };"
                  L"if (Test-Path -LiteralPath $startPath -PathType Container) { Set-Location -LiteralPath $startPath };"
                  L"try { $path=(Get-Location).ProviderPath; if (-not [string]::IsNullOrWhiteSpace($path)) { Set-Content -LiteralPath $global:__qtStatePath -Value $path -Encoding Unicode -Force } } catch {}"
                : L"$global:__qtStatePath='%s';"
                  L"$global:__qtOriginalPrompt=$function:prompt;"
                  L"function global:prompt { try { $path=(Get-Location).ProviderPath; if (-not [string]::IsNullOrWhiteSpace($path)) { Set-Content -LiteralPath $global:__qtStatePath -Value $path -Encoding Unicode -Force } } catch {} if ($null -ne $global:__qtOriginalPrompt) { & $global:__qtOriginalPrompt } else { 'PS ' + $(Get-Location) + '> ' } };"
                  L"try { $path=(Get-Location).ProviderPath; if (-not [string]::IsNullOrWhiteSpace($path)) { Set-Content -LiteralPath $global:__qtStatePath -Value $path -Encoding Unicode -Force } } catch {}",
            state_file_path_escaped,
            has_start_directory ? start_directory_escaped : L"",
            state_file_path_escaped)))
    {
        return FALSE;
    }

    if (!EncodePowerShellCommand(script, encoded_script, &encoded_script_length))
    {
        return FALSE;
    }

    (void)start_directory;

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"new-tab powershell.exe -NoExit -EncodedCommand %s",
        encoded_script));
}

BOOL IsSupportedTerminalMode(const wchar_t *mode)
{
    if (mode == NULL || mode[0] == L'\0')
    {
        return FALSE;
    }

    return (lstrcmpiW(mode, L"terminal-only") == 0 ||
            lstrcmpiW(mode, L"terminal-with-powershell") == 0);
}

static const wchar_t *GetEffectiveTerminalArguments(void)
{
    if (lstrcmpiW(g_config.terminal_mode, L"terminal-only") == 0)
    {
        return NULL;
    }

    if (g_config.terminal_arguments[0] == L'\0')
    {
        return NULL;
    }

    return g_config.terminal_arguments;
}

BOOL IsTerminalOnlyMode(void)
{
    return (lstrcmpiW(g_config.terminal_mode, L"terminal-only") == 0);
}

BOOL LaunchWindowsTerminal(void)
{
    wchar_t tracked_arguments[32768];
    const wchar_t *arguments = GetEffectiveTerminalArguments();

    if (!IsTerminalOnlyMode())
    {
        if (!BuildTrackedPowerShellArguments(
                tracked_arguments,
                sizeof(tracked_arguments) / sizeof(tracked_arguments[0])))
        {
            ShowErrorMessage(L"Failed to prepare the PowerShell launch command.");
            return FALSE;
        }

        arguments = tracked_arguments;
    }

    HINSTANCE result = ShellExecuteW(
        NULL,
        L"open",
        g_config.terminal_command,
        arguments,
        NULL,
        SW_SHOWNORMAL);

    return ((INT_PTR)result > 32);
}

void ShowTerminalLaunchError(void)
{
    wchar_t message[4096];
    const wchar_t *arguments = GetEffectiveTerminalArguments();
    const wchar_t *mode = IsTerminalOnlyMode() ? L"terminal-only" : L"terminal-with-powershell";
    const wchar_t *display_arguments = (arguments != NULL && arguments[0] != L'\0') ? arguments : L"(none)";

    if (FAILED(StringCchPrintfW(
            message,
            sizeof(message) / sizeof(message[0]),
            L"Failed to launch Windows Terminal.\n\nMode: %s\nCommand: %s\nArguments: %s",
            mode,
            g_config.terminal_command,
            display_arguments)))
    {
        ShowErrorMessage(L"Failed to launch Windows Terminal.");
        return;
    }

    ShowErrorMessage(message);
}

BOOL SetTerminalMode(const wchar_t *mode)
{
    wchar_t normalized_mode[64];
    wchar_t message[256];

    if (!IsSupportedTerminalMode(mode))
    {
        ShowErrorMessage(L"Invalid terminal mode. Use terminal-only or terminal-with-powershell.");
        return FALSE;
    }

    if (!CopyConfigString(
            normalized_mode,
            sizeof(normalized_mode) / sizeof(normalized_mode[0]),
            mode))
    {
        ShowErrorMessage(L"Failed to store the requested terminal mode.");
        return FALSE;
    }

    if (!CopyConfigString(
            g_config.terminal_mode,
            sizeof(g_config.terminal_mode) / sizeof(g_config.terminal_mode[0]),
            normalized_mode))
    {
        ShowErrorMessage(L"Failed to apply the requested terminal mode.");
        return FALSE;
    }

    if (!SaveConfig(&g_config))
    {
        ShowErrorMessage(L"Failed to write the terminal mode setting to the config file.");
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            message,
            sizeof(message) / sizeof(message[0]),
            L"Terminal launch mode has been set to %s.",
            normalized_mode)))
    {
        ShowInfoNotificationWithFallback(L"Terminal launch mode has been updated.");
        return TRUE;
    }

    ShowInfoNotificationWithFallback(message);
    return TRUE;
}
