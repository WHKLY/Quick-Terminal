# Quick Terminal 中文指南

[English README](README.md) · [项目说明](PROJECT.md)

Quick Terminal 是一个轻量级 Windows 托盘工具，可以通过全局快捷键快速打开 Windows Terminal。

## 功能

- 支持全局快捷键，默认是 `Ctrl+Alt+T`
- 可选择只启动 Windows Terminal，或通过 Terminal 打开 PowerShell 标签页
- 支持托盘图标和托盘菜单
- 支持当前用户级别的开机自启
- 优先使用现代通知，失败时自动回退
- 使用配置文件持久化设置

## 快速开始

### 方式一：直接下载发布版本

1. 打开 GitHub 仓库的 `Releases` 页面
2. 下载 `quick-terminal-v0.1.1.exe`
3. 在 Windows 上直接运行

如果你直接使用发布附件，命令示例可以这样写：

```powershell
.\quick-terminal-v0.1.1.exe
.\quick-terminal-v0.1.1.exe --set-terminal-mode terminal-only
```

### 方式二：克隆后本地编译

```bash
git clone https://github.com/WHKLY/Quick-Terminal.git
cd Quick-Terminal
```

默认在仓库根目录执行以下命令：

```powershell
New-Item -ItemType Directory -Force build | Out-Null
windres resources\quick-terminal.rc -O coff -o build\quick-terminal-res.o
gcc -municode -mwindows -g -O0 -Wall -Wextra src/main.c src/app.c src/config.c src/autostart.c src/notifications.c src/tray.c src/hotkey.c src/terminal.c src/strsafe_compat.c build\quick-terminal-res.o -o build\quick-terminal.exe -lshell32 -lole32 -luuid -lcrypt32
```

## 运行

启动后台热键监听：

```powershell
.\build\quick-terminal.exe
```

默认行为：

- 程序在后台驻留
- 如果配置允许，会显示托盘图标
- 按下 `Ctrl+Alt+T` 启动 Windows Terminal

## 常用命令

如果你是本地编译版本，使用：

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

## 配置文件

配置文件位置：

```text
%APPDATA%\QuickTerminal\config.ini
```

默认配置示例：

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

支持的终端模式：

- `terminal-only`
- `terminal-with-powershell`

模式说明：

- `terminal-only`：只启动 Windows Terminal
- `terminal-with-powershell`：通过 Windows Terminal 打开 PowerShell 新标签页

补充说明：

- 当 `mode=terminal-only` 时，`arguments` 会被忽略
- 当 `mode=terminal-with-powershell` 时，会组合使用 `command` 和 `arguments`

## 常见排查

- 如果编译时报 `Permission denied`，通常是旧的 `quick-terminal.exe` 还在运行
- 如果快捷键注册失败，通常是被别的程序占用了
- 如果某些机器上通过 Terminal 打开 PowerShell 失败，可以切到 `terminal-only`
- 如果 `wt.exe` 不可用，可以修改 `[terminal]` 配置
- 如果通知没有弹出，可以先执行 `--test-notification`

## 许可证

MIT，详见 [LICENSE](LICENSE)。
