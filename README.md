# Quick Terminal

[中文指南](README.zh-CN.md)

Quick Terminal is a lightweight Windows utility that lets you press a global hotkey to open Windows Terminal and start PowerShell.

## Features

- Global hotkey support, defaulting to `Ctrl+Alt+T`
- Launches Windows Terminal and opens PowerShell in a new tab
- Runs as a single background instance with a tray icon
- User-level auto-start support through the Windows Run key
- Toast-first notifications with tray and dialog fallbacks
- Persistent settings stored in `%APPDATA%\QuickTerminal\config.ini`
- Configurable tray visibility, hotkey, terminal launch mode, terminal command, and config directory

## Requirements

- Windows 10 or Windows 11
- Windows Terminal installed and available as `wt.exe`
- A MinGW-style `gcc` toolchain if you want to build from source
- `windres` for compiling the Windows resource file

## Project Layout

```text
src/main.c
src/app.c
src/app.h
src/config.c
src/config.h
src/autostart.c
src/autostart.h
src/notifications.c
src/notifications.h
src/strsafe_compat.c
src/qt_strsafe.h
src/tray.c
src/tray.h
src/hotkey.c
src/hotkey.h
src/terminal.c
src/terminal.h
src/resource.h
resources/quick-terminal.rc
assets/icons/quick-terminal.ico
```

## Build

Run the following commands from the repository root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
windres resources\quick-terminal.rc -O coff -o build\quick-terminal-res.o
gcc -municode -mwindows -g -O0 -Wall -Wextra src/main.c src/app.c src/config.c src/autostart.c src/notifications.c src/tray.c src/hotkey.c src/terminal.c src/strsafe_compat.c build\quick-terminal-res.o -o build\quick-terminal.exe -lshell32 -lole32 -luuid -lcrypt32
```

## Run

Run without arguments to start the background hotkey listener:

```powershell
.\build\quick-terminal.exe
```

Default behavior:

- The app stays in the background
- A tray icon appears unless disabled in config
- Pressing `Ctrl+Alt+T` opens Windows Terminal
- PowerShell opens in a new tab

## Commands

```powershell
.\build\quick-terminal.exe --enable-autostart
.\build\quick-terminal.exe --disable-autostart
.\build\quick-terminal.exe --autostart-status
.\build\quick-terminal.exe --show-tray
.\build\quick-terminal.exe --hide-tray
.\build\quick-terminal.exe --tray-status
.\build\quick-terminal.exe --set-terminal-mode terminal-only
.\build\quick-terminal.exe --set-terminal-mode terminal-with-powershell
.\build\quick-terminal.exe --test-notification
.\build\quick-terminal.exe --config-dir "C:\Path\To\ConfigDir"
.\build\quick-terminal.exe --config-dir-name QuickTerminalCustom
.\build\quick-terminal.exe --help
```

## Configuration

Quick Terminal stores app settings in:

```text
%APPDATA%\QuickTerminal\config.ini
```

Example:

```ini
[general]
show_tray=true
show_startup_notification=true

[terminal]
mode=terminal-with-powershell
command=wt.exe
arguments=new-tab powershell.exe

[hotkey]
modifiers=Ctrl+Alt
key=T
```

Supported hotkey modifier tokens:

- `Ctrl`
- `Alt`
- `Shift`
- `Win`

Supported hotkey key examples:

- `T`
- `1`
- `F1`
- `F12`
- `Tab`
- `Enter`
- `Escape`
- `Space`
- `Up`
- `Down`
- `Left`
- `Right`

Supported terminal modes:

- `terminal-only`
- `terminal-with-powershell`

Mode behavior:

- `terminal-only` launches Windows Terminal only
- `terminal-with-powershell` launches Windows Terminal and opens PowerShell in a new tab

You can also switch the mode from the terminal:

```powershell
.\build\quick-terminal.exe --set-terminal-mode terminal-only
.\build\quick-terminal.exe --set-terminal-mode terminal-with-powershell
```

## Auto-Start

Auto-start is managed through:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

This keeps setup user-scoped and does not require administrator privileges.

## Notifications

Quick Terminal prefers modern Windows toast notifications for lightweight confirmations and status messages. If toast delivery is unavailable, it falls back to the tray notification path or a classic dialog where appropriate.

## Tray Behavior

- Double-click the tray icon to open Windows Terminal
- Right-click the tray icon to access the control menu
- Use terminal commands to show or hide the tray persistently
- Hiding the tray does not disable the hotkey listener

## Troubleshooting

- If rebuild fails with `Permission denied`, stop any running `quick-terminal.exe` process first
- If the hotkey does not register, another application may already be using the same combination
- If auto-start stops working, refresh it with `--enable-autostart`
- If toast notifications do not appear, test with `--test-notification` and confirm the fallback behavior still works
- If `wt.exe` is unavailable, update the `[terminal]` section in `config.ini`

## Validation Checklist

- The app starts successfully
- The configured hotkey works globally
- Windows Terminal opens and PowerShell starts in a new tab
- Launching the app a second time does not create a competing instance
- Tray controls work as expected
- Auto-start can be enabled and disabled
- Notifications appear or fall back cleanly
- Config changes persist across restarts
