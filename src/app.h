#ifndef QUICK_TERMINAL_APP_H
#define QUICK_TERMINAL_APP_H

#include <stddef.h>
#include <windows.h>
#include <shellapi.h>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#ifndef NOTIFYICON_VERSION_4
#define NOTIFYICON_VERSION_4 4
#endif

enum
{
    HOTKEY_ID = 1,
    WMAPP_TRAYICON = WM_APP + 1,
    WMAPP_SET_TRAY_VISIBILITY = WM_APP + 2,
    ID_TRAY_OPEN_TERMINAL = 1001,
    ID_TRAY_ENABLE_AUTOSTART = 1002,
    ID_TRAY_DISABLE_AUTOSTART = 1003,
    ID_TRAY_SHOW_STATUS = 1004,
    ID_TRAY_EXIT = 1005
};

typedef enum
{
    COMMAND_NONE = 0,
    COMMAND_ENABLE_AUTOSTART,
    COMMAND_DISABLE_AUTOSTART,
    COMMAND_AUTOSTART_STATUS,
    COMMAND_SHOW_TRAY,
    COMMAND_HIDE_TRAY,
    COMMAND_TRAY_STATUS,
    COMMAND_TEST_NOTIFICATION,
    COMMAND_HELP
} CommandMode;

typedef struct AppOptionsTag
{
    CommandMode command_mode;
    BOOL show_startup_notification;
    BOOL has_custom_config_directory;
    BOOL persist_custom_config_directory;
    wchar_t custom_config_directory[32768];
} AppOptions;

typedef struct AppConfigTag
{
    BOOL show_tray;
    BOOL show_startup_notification;
    wchar_t terminal_command[512];
    wchar_t terminal_arguments[2048];
    wchar_t hotkey_modifiers[64];
    wchar_t hotkey_key[64];
} AppConfig;

typedef struct AppStateTag
{
    HINSTANCE instance;
    HANDLE mutex;
    HWND window;
    NOTIFYICONDATAW tray_icon;
    HICON large_icon;
    HICON small_icon;
    BOOL tray_icon_added;
    BOOL tray_enabled;
    BOOL show_startup_notification;
} AppState;

extern AppState g_app;
extern AppConfig g_config;
extern BOOL g_error_message_shown;

extern const wchar_t *kAppTitle;
extern const wchar_t *kWindowClassName;
extern const wchar_t *kMutexName;
extern const wchar_t *kDefaultTerminalCommand;
extern const wchar_t *kDefaultTerminalArguments;
extern const wchar_t *kDefaultHotkeyModifiers;
extern const wchar_t *kDefaultHotkeyKey;
extern const wchar_t *kRunKeyPath;
extern const wchar_t *kRunValueName;
extern const wchar_t *kSettingsKeyPath;
extern const wchar_t *kTrayEnabledValueName;
extern const wchar_t *kConfigDirectoryValueName;
extern const wchar_t *kConfigDirectoryName;
extern const wchar_t *kConfigFileName;
extern const wchar_t *kToastAppUserModelId;
extern const wchar_t *kToastShortcutName;
extern const wchar_t *kUsageText;

BOOL CopyConfigString(wchar_t *buffer, size_t buffer_count, const wchar_t *value);
void TrimWhitespaceInPlace(wchar_t *value);
void ShowInfoMessage(const wchar_t *message);
void ShowErrorMessage(const wchar_t *message);
BOOL GetExecutablePath(wchar_t *buffer, size_t buffer_count);
BOOL GetExecutableDirectory(wchar_t *buffer, size_t buffer_count);
int AppMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command);

#endif
