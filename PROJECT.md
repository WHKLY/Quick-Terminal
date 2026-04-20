# Project Notes

This document keeps the implementation-oriented notes out of the main README.

## Scope

Quick Terminal is a native Windows utility written in C and built around a small background process with a hidden window, a global hotkey, and an optional tray icon.

## Tech Stack

- `C`
  Core application language
- `Win32 API`
  Windowing, hotkeys, tray icon, process launch, registry access, and message loop
- `Windows Terminal`
  Terminal entry point, usually through `wt.exe`
- `Windows PowerShell`
  Used for toast-notification delivery
- `MinGW gcc`
  Native Windows build toolchain
- `windres`
  Windows resource compilation

## Current Architecture

- `src/main.c`
  Thin entry point
- `src/app.c` / `src/app.h`
  App startup, command-line parsing, window procedure, and main loop
- `src/config.c` / `src/config.h`
  Config loading, saving, validation, and config-directory handling
- `src/autostart.c` / `src/autostart.h`
  User-level auto-start integration through the Run key
- `src/notifications.c` / `src/notifications.h`
  Toast-first notification flow and fallback handling
- `src/tray.c` / `src/tray.h`
  Tray icon lifecycle and tray menu actions
- `src/hotkey.c` / `src/hotkey.h`
  Hotkey parsing and validation
- `src/terminal.c` / `src/terminal.h`
  Windows Terminal launch behavior and terminal mode switching
- `src/qt_strsafe.h` / `src/strsafe_compat.c`
  Compatibility layer for older MinGW `strsafe` behavior

## Runtime Pieces

- Hidden Win32 window for message dispatch
- Global hotkey registration through `RegisterHotKey`
- Tray icon through `Shell_NotifyIcon`
- Auto-start through `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- App config through `%APPDATA%\QuickTerminal\config.ini`

## Terminal Modes

The current release supports:

- `terminal-only`
- `terminal-with-powershell`

The default remains `terminal-with-powershell` for backward compatibility.

## Config Overview

Current config-owned settings:

- `show_tray`
- `show_startup_notification`
- `terminal.mode`
- `terminal.command`
- `terminal.arguments`
- `hotkey.modifiers`
- `hotkey.key`

## System Changes

Quick Terminal may create or update the following items on a user's computer:

- `%APPDATA%\QuickTerminal\config.ini`
  Stores app configuration
- `%APPDATA%\QuickTerminal\`
  Config directory
- `HKCU\Software\QuickTerminal`
  Stores app-specific registry values such as the remembered config directory
- `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
  Used only when the user enables auto-start
- Start Menu shortcut for toast attribution
  Created or refreshed so Windows toast notifications can be attributed correctly

Quick Terminal does not require administrator privileges for its normal user-level features.

## Compatibility Notes

- The project targets Windows 10 and Windows 11
- The MinGW build path includes a small `strsafe` compatibility shim for older environments
- Some user environments may fail to resolve `powershell.exe` when launched through `wt.exe`; `terminal-only` exists to handle that case cleanly
