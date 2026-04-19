# Quick Terminal 中文指南

[English README](README.md)

Quick Terminal 是一个轻量级 Windows 小工具，可以通过全局快捷键快速打开 Windows Terminal，并在新标签页中启动 PowerShell。

## 功能特性

- 支持全局快捷键，默认是 `Ctrl+Alt+T`
- 打开 Windows Terminal，并新建 PowerShell 标签页
- 单实例后台运行，支持系统托盘
- 支持当前用户级别的开机自启
- 优先使用现代 Windows 通知，失败时自动回退
- 使用 `%APPDATA%\QuickTerminal\config.ini` 持久化配置
- 支持配置托盘显示、快捷键、终端命令和配置目录

## 运行环境

- Windows 10 或 Windows 11
- 已安装 Windows Terminal，并能通过 `wt.exe` 调用
- 如果要自行编译，需要有 MinGW 风格的 `gcc`
- 需要 `windres` 来编译资源文件

## 项目结构

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

## 手动编译

默认在仓库根目录执行以下命令：

```powershell
New-Item -ItemType Directory -Force build | Out-Null
windres resources\quick-terminal.rc -O coff -o build\quick-terminal-res.o
gcc -municode -mwindows -g -O0 -Wall -Wextra src/main.c src/app.c src/config.c src/autostart.c src/notifications.c src/tray.c src/hotkey.c src/terminal.c build\quick-terminal-res.o -o build\quick-terminal.exe -lshell32 -lole32 -luuid -lcrypt32
```

## 启动方式

不带参数运行时，会启动后台热键监听：

```powershell
.\build\quick-terminal.exe
```

默认行为：

- 程序在后台驻留
- 如果配置允许，会显示托盘图标
- 按下 `Ctrl+Alt+T` 后打开 Windows Terminal
- 在新标签页中启动 PowerShell

## 常用命令

```powershell
.\build\quick-terminal.exe --enable-autostart
.\build\quick-terminal.exe --disable-autostart
.\build\quick-terminal.exe --autostart-status
.\build\quick-terminal.exe --show-tray
.\build\quick-terminal.exe --hide-tray
.\build\quick-terminal.exe --tray-status
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

示例：

```ini
[general]
show_tray=true
show_startup_notification=true

[terminal]
command=wt.exe
arguments=new-tab powershell.exe

[hotkey]
modifiers=Ctrl+Alt
key=T
```

可用的快捷键修饰键：

- `Ctrl`
- `Alt`
- `Shift`
- `Win`

可用的按键示例：

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

## 开机自启

开机自启通过下面的注册表路径管理：

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

这样做的特点是：

- 只作用于当前用户
- 不需要管理员权限
- 适合轻量级桌面工具

## 托盘与通知

- 双击托盘图标可以打开 Windows Terminal
- 右键托盘图标可以打开控制菜单
- 程序优先使用现代 Windows 通知
- 如果现代通知不可用，会自动回退到托盘提示或对话框
- 隐藏托盘后，热键监听仍然继续工作

## 常见排查

- 如果编译时报 `Permission denied`，通常是旧的 `quick-terminal.exe` 还在运行
- 如果快捷键注册失败，通常是被别的程序占用了
- 如果开机自启失效，可以重新执行一次 `--enable-autostart`
- 如果通知没有弹出，可以先执行 `--test-notification`
- 如果 `wt.exe` 不可用，可以在配置文件里修改 `[terminal]` 段

## 发布前自查

- 程序可以正常启动
- 配置的快捷键能全局生效
- Windows Terminal 和 PowerShell 能正常打开
- 二次启动不会产生多个竞争实例
- 托盘功能正常
- 开机自启可以启用和禁用
- 通知可以正常显示或正确回退
- 配置文件修改后能持久化保存
