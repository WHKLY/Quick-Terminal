#ifndef QUICK_TERMINAL_HOTKEY_H
#define QUICK_TERMINAL_HOTKEY_H

#include "app.h"

BOOL ParseHotkeyModifiersString(const wchar_t *value, UINT *modifiers);
BOOL ParseHotkeyKeyString(const wchar_t *value, UINT *virtual_key);

#endif
