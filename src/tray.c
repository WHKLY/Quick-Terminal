#include "tray.h"
#include "autostart.h"
#include "config.h"
#include "notifications.h"
#include "qt_strsafe.h"
#include "resource.h"
#include "terminal.h"

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

BOOL InitializeAppIcons(void)
{
    g_app.large_icon = LoadSizedAppIcon(
        GetSystemMetrics(SM_CXICON),
        GetSystemMetrics(SM_CYICON));
    g_app.small_icon = LoadSizedAppIcon(
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON));

    return (g_app.large_icon != NULL && g_app.small_icon != NULL);
}

void DestroyAppIcons(void)
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

BOOL QueryTrayPreference(BOOL *is_enabled)
{
    if (is_enabled == NULL)
    {
        return FALSE;
    }

    *is_enabled = g_config.show_tray;
    return TRUE;
}

BOOL SetTrayPreference(BOOL is_enabled)
{
    g_config.show_tray = is_enabled;

    if (!SaveConfig(&g_config))
    {
        ShowErrorMessage(L"Failed to write the tray visibility setting to the config file.");
        return FALSE;
    }

    return TRUE;
}

HWND FindRunningInstanceWindow(void)
{
    return FindWindowW(kWindowClassName, kAppTitle);
}

void SyncRunningInstanceTrayVisibility(BOOL is_enabled)
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

BOOL ShowTraySettingStatus(void)
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

BOOL EnableTrayPreference(void)
{
    if (!SetTrayPreference(TRUE))
    {
        return FALSE;
    }

    SyncRunningInstanceTrayVisibility(TRUE);
    ShowInfoNotificationWithFallback(L"Tray icon display has been enabled.");
    return TRUE;
}

BOOL DisableTrayPreference(void)
{
    if (!SetTrayPreference(FALSE))
    {
        return FALSE;
    }

    SyncRunningInstanceTrayVisibility(FALSE);
    ShowInfoNotificationWithFallback(L"Tray icon display has been disabled.");
    return TRUE;
}

void RemoveTrayIcon(void)
{
    if (g_app.tray_icon_added)
    {
        Shell_NotifyIconW(NIM_DELETE, &g_app.tray_icon);
        g_app.tray_icon_added = FALSE;
    }
}

BOOL ShowTrayNotification(const wchar_t *title, const wchar_t *message)
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

BOOL InitializeTrayIcon(HWND window)
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

void ApplyTrayVisibility(BOOL is_enabled)
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

void ShowTrayMenu(HWND window)
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

void HandleTrayCommand(HWND window, UINT command_id)
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
