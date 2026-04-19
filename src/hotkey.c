#include "hotkey.h"
#include "qt_strsafe.h"

#include <stdlib.h>

BOOL ParseHotkeyModifiersString(const wchar_t *value, UINT *modifiers)
{
    wchar_t buffer[128];
    wchar_t *token;
    UINT parsed_modifiers = 0;

    if (value == NULL || modifiers == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(buffer, sizeof(buffer) / sizeof(buffer[0]), value)))
    {
        return FALSE;
    }

    token = wcstok(buffer, L"+");
    while (token != NULL)
    {
        TrimWhitespaceInPlace(token);

        if (lstrcmpiW(token, L"Ctrl") == 0 || lstrcmpiW(token, L"Control") == 0)
        {
            parsed_modifiers |= MOD_CONTROL;
        }
        else if (lstrcmpiW(token, L"Alt") == 0)
        {
            parsed_modifiers |= MOD_ALT;
        }
        else if (lstrcmpiW(token, L"Shift") == 0)
        {
            parsed_modifiers |= MOD_SHIFT;
        }
        else if (lstrcmpiW(token, L"Win") == 0 || lstrcmpiW(token, L"Windows") == 0)
        {
            parsed_modifiers |= MOD_WIN;
        }
        else
        {
            return FALSE;
        }

        token = wcstok(NULL, L"+");
    }

    if (parsed_modifiers == 0)
    {
        return FALSE;
    }

    *modifiers = parsed_modifiers;
    return TRUE;
}

BOOL ParseHotkeyKeyString(const wchar_t *value, UINT *virtual_key)
{
    wchar_t upper[64];
    wchar_t *end_ptr = NULL;
    long number;

    if (value == NULL || virtual_key == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(upper, sizeof(upper) / sizeof(upper[0]), value)))
    {
        return FALSE;
    }

    TrimWhitespaceInPlace(upper);
    CharUpperBuffW(upper, lstrlenW(upper));

    if (lstrlenW(upper) == 1)
    {
        wchar_t ch = upper[0];

        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9'))
        {
            *virtual_key = (UINT)ch;
            return TRUE;
        }
    }

    if (upper[0] == L'F' && upper[1] != L'\0')
    {
        number = wcstol(upper + 1, &end_ptr, 10);
        if (end_ptr != NULL && *end_ptr == L'\0' && number >= 1 && number <= 24)
        {
            *virtual_key = (UINT)(VK_F1 + (number - 1));
            return TRUE;
        }
    }

    if (lstrcmpW(upper, L"TAB") == 0)
    {
        *virtual_key = VK_TAB;
        return TRUE;
    }
    if (lstrcmpW(upper, L"ENTER") == 0)
    {
        *virtual_key = VK_RETURN;
        return TRUE;
    }
    if (lstrcmpW(upper, L"ESC") == 0 || lstrcmpW(upper, L"ESCAPE") == 0)
    {
        *virtual_key = VK_ESCAPE;
        return TRUE;
    }
    if (lstrcmpW(upper, L"SPACE") == 0)
    {
        *virtual_key = VK_SPACE;
        return TRUE;
    }
    if (lstrcmpW(upper, L"UP") == 0)
    {
        *virtual_key = VK_UP;
        return TRUE;
    }
    if (lstrcmpW(upper, L"DOWN") == 0)
    {
        *virtual_key = VK_DOWN;
        return TRUE;
    }
    if (lstrcmpW(upper, L"LEFT") == 0)
    {
        *virtual_key = VK_LEFT;
        return TRUE;
    }
    if (lstrcmpW(upper, L"RIGHT") == 0)
    {
        *virtual_key = VK_RIGHT;
        return TRUE;
    }
    if (lstrcmpW(upper, L"HOME") == 0)
    {
        *virtual_key = VK_HOME;
        return TRUE;
    }
    if (lstrcmpW(upper, L"END") == 0)
    {
        *virtual_key = VK_END;
        return TRUE;
    }
    if (lstrcmpW(upper, L"PAGEUP") == 0 || lstrcmpW(upper, L"PGUP") == 0)
    {
        *virtual_key = VK_PRIOR;
        return TRUE;
    }
    if (lstrcmpW(upper, L"PAGEDOWN") == 0 || lstrcmpW(upper, L"PGDN") == 0)
    {
        *virtual_key = VK_NEXT;
        return TRUE;
    }
    if (lstrcmpW(upper, L"INSERT") == 0)
    {
        *virtual_key = VK_INSERT;
        return TRUE;
    }
    if (lstrcmpW(upper, L"DELETE") == 0)
    {
        *virtual_key = VK_DELETE;
        return TRUE;
    }
    if (lstrcmpW(upper, L"BACKSPACE") == 0)
    {
        *virtual_key = VK_BACK;
        return TRUE;
    }

    return FALSE;
}
