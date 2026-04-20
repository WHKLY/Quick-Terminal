# Quick Terminal

[中文指南](README.zh-CN.md) · [Project Notes](PROJECT.md)

Quick Terminal is a lightweight Windows tray utility that opens Windows Terminal with a global hotkey.

## Features

- Global hotkey support, defaulting to `Ctrl+Alt+T`
- Opens Windows Terminal, optionally with a PowerShell tab
- Tray icon and tray menu controls
- User-level auto-start support
- Toast-first notifications with fallback behavior
- Persistent config file support

## Get Started

### Option 1: Download a release

1. Open the repository's `Releases` page on GitHub
2. Download `quick-terminal-v0.1.1.exe`
3. Run it directly on Windows

If you use the release asset directly, example commands look like:

```powershell
.\quick-terminal-v0.1.1.exe
.\quick-terminal-v0.1.1.exe --set-terminal-mode terminal-only
```

### Option 2: Clone and build from source

```bash
git clone https://github.com/WHKLY/Quick-Terminal.git
cd Quick-Terminal
```

Build from the repository root:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
windres resources\quick-terminal.rc -O coff -o build\quick-terminal-res.o
gcc -municode -mwindows -g -O0 -Wall -Wextra src/main.c src/app.c src/config.c src/autostart.c src/notifications.c src/tray.c src/hotkey.c src/terminal.c src/strsafe_compat.c build\quick-terminal-res.o -o build\quick-terminal.exe -lshell32 -lole32 -luuid -lcrypt32
```

## Run

Start the background hotkey listener:

```powershell
.\build\quick-terminal.exe
```

Default behavior:

- The app stays in the background
- A tray icon appears unless disabled in config
- Press `Ctrl+Alt+T` to launch Windows Terminal

## Commands

If you built from source, use:

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

Config file location:

```text
%APPDATA%\QuickTerminal\config.ini
```

Default config example:

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

Supported terminal modes:

- `terminal-only`
- `terminal-with-powershell`

Mode behavior:

- `terminal-only` launches Windows Terminal only
- `terminal-with-powershell` launches Windows Terminal and opens PowerShell in a new tab

Notes:

- When `mode=terminal-only`, `arguments` are ignored
- When `mode=terminal-with-powershell`, `command` and `arguments` are used together

## Troubleshooting

- If rebuild fails with `Permission denied`, stop any running `quick-terminal.exe` process first
- If the hotkey does not register, another app may already be using that combination
- If launching PowerShell through Windows Terminal fails on a specific machine, switch to `terminal-only`
- If `wt.exe` is unavailable, update the `[terminal]` section in `config.ini`
- If toast notifications do not appear, test with `--test-notification`

## License

MIT. See [LICENSE](LICENSE).
