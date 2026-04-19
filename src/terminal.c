#include "terminal.h"

BOOL LaunchWindowsTerminal(void)
{
    HINSTANCE result = ShellExecuteW(
        NULL,
        L"open",
        g_config.terminal_command,
        g_config.terminal_arguments[0] != L'\0' ? g_config.terminal_arguments : NULL,
        NULL,
        SW_SHOWNORMAL);

    return ((INT_PTR)result > 32);
}
