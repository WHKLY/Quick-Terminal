#ifndef QUICK_TERMINAL_NOTIFICATIONS_H
#define QUICK_TERMINAL_NOTIFICATIONS_H

#include "app.h"

BOOL ShowToastNotification(const wchar_t *title, const wchar_t *message);
BOOL ShowStartupNotification(void);
void ShowInfoNotificationWithFallback(const wchar_t *message);
void ShowStatusNotificationWithFallback(const wchar_t *toast_message, const wchar_t *fallback_message);
BOOL TestModernNotification(void);

#endif
