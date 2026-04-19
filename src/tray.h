#ifndef QUICK_TERMINAL_TRAY_H
#define QUICK_TERMINAL_TRAY_H

#include "app.h"

BOOL InitializeAppIcons(void);
void DestroyAppIcons(void);
BOOL InitializeTrayIcon(HWND window);
void RemoveTrayIcon(void);
BOOL ShowTrayNotification(const wchar_t *title, const wchar_t *message);
void ApplyTrayVisibility(BOOL is_enabled);
void ShowTrayMenu(HWND window);
void HandleTrayCommand(HWND window, UINT command_id);
HWND FindRunningInstanceWindow(void);
void SyncRunningInstanceTrayVisibility(BOOL is_enabled);
BOOL QueryTrayPreference(BOOL *is_enabled);
BOOL SetTrayPreference(BOOL is_enabled);
BOOL ShowTraySettingStatus(void);
BOOL EnableTrayPreference(void);
BOOL DisableTrayPreference(void);

#endif
