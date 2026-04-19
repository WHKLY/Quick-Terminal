#ifndef QUICK_TERMINAL_AUTOSTART_H
#define QUICK_TERMINAL_AUTOSTART_H

#include "app.h"

BOOL QueryAutostartCommand(wchar_t *buffer, DWORD buffer_size_bytes, BOOL *is_enabled);
BOOL EnableAutostart(void);
BOOL DisableAutostart(void);
BOOL ShowAutostartStatus(void);

#endif
