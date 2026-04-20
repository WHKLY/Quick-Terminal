#include "terminal.h"
#include "config.h"
#include "notifications.h"
#include "qt_strsafe.h"

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
    const wchar_t *arguments = GetEffectiveTerminalArguments();
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
