#ifndef QUICK_TERMINAL_TERMINAL_H
#define QUICK_TERMINAL_TERMINAL_H

#include "app.h"

BOOL IsSupportedTerminalMode(const wchar_t *mode);
BOOL IsTerminalOnlyMode(void);
BOOL LaunchWindowsTerminal(void);
void ShowTerminalLaunchError(void);
BOOL SetTerminalMode(const wchar_t *mode);

#endif
