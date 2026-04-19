# Windows `Ctrl` + `Alt` + `T` Quick Terminal

## Goal

On Windows, press `Ctrl` + `Alt` + `T` to open Windows Terminal and start PowerShell.

This version does not force a custom startup directory. PowerShell uses its default initial directory behavior.

## Recommended V1 Solution

Implement a small resident Windows program in `C` with WinAPI.

The program:

1. Registers the global hotkey `Ctrl` + `Alt` + `T`
2. Runs in the background with a hidden window and message loop
3. Launches Windows Terminal on hotkey press
4. Adds a system tray icon for visibility and control
5. Opens a PowerShell tab by calling:

```powershell
wt.exe new-tab powershell.exe
```

## Why `C`

- Produces a standalone `.exe`
- Does not require AutoHotkey or another runtime
- Easy to extend later with startup registration or tray behavior
- Fits the existing local `gcc` environment

## Source Layout

```text
src/main.c
```

## Runtime Behavior

After the program starts:

1. It waits in the background
2. A tray icon appears in the Windows notification area
3. Press `Ctrl` + `Alt` + `T`
4. Windows Terminal opens
5. A new PowerShell tab is created

Tray behavior:

- Double-click the tray icon to open Windows Terminal
- Right-click the tray icon to open the control menu
- The control menu supports opening Terminal, enabling auto-start, disabling auto-start, showing status, and exiting the app
- When started from the auto-start registry entry, the app can show a tray balloon confirming that it is running
- Tray visibility can be controlled persistently from terminal commands

## Implementation Notes

The current version includes:

- Global hotkey registration with `RegisterHotKey`
- Hidden window hosting the app message loop
- Message loop based on `GetMessage`
- Terminal launch via `ShellExecuteW`
- A single-instance guard using a named mutex
- A system tray icon via `Shell_NotifyIcon`
- Tray menu actions for launch, status, auto-start toggle, and exit
- Optional startup balloon notification
- Persistent tray visibility settings controlled by command-line switches
- Basic error dialogs for common failures
- User-level auto-start management through command-line switches

## Auto-Start Technical Route

Recommended approach:

- Register the compiled `.exe` under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- Use the current user scope only
- Avoid requiring administrator privileges
- Keep the executable path configurable or discoverable at runtime

Why this route is preferred:

- No admin permission is required
- It applies only to the current user
- It is easy to enable and disable in code
- It is more direct than asking the user to manage a shortcut in the Startup folder

Current implementation approach:

1. Add command-line switches such as `--enable-autostart` and `--disable-autostart`
2. Write or remove the corresponding registry value
3. Keep the default program launch behavior unchanged when started normally

Possible fallback route:

- Create a shortcut in the Startup folder at `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup`

This fallback is workable, but it is less clean for a self-managed utility and adds shortcut creation logic.

## Auto-Start Commands

The current implementation supports these commands:

```powershell
.\build\quick-terminal.exe --enable-autostart
.\build\quick-terminal.exe --disable-autostart
.\build\quick-terminal.exe --autostart-status
.\build\quick-terminal.exe --help
```

Notes:

- `--enable-autostart` writes the current executable path to the current user Run key
- The stored command includes an internal startup flag so login launches can show a tray confirmation balloon
- `--disable-autostart` removes that value
- `--autostart-status` shows whether the Run value exists and displays the stored command
- Running the program without arguments still starts the background hotkey listener

## Tray Visibility Commands

The current implementation supports these tray visibility commands:

```powershell
.\build\quick-terminal.exe --show-tray
.\build\quick-terminal.exe --hide-tray
.\build\quick-terminal.exe --tray-status
```

Notes:

- `--show-tray` enables tray icon display for future launches
- `--hide-tray` disables tray icon display for future launches
- If an instance is already running, the app will try to apply the tray visibility change immediately
- `--tray-status` reports whether tray icon display is currently enabled
- Hiding the tray does not stop the hotkey listener; it only removes the tray icon and tray menu UI

## Manual Build Reference

Source generation is complete first. Compilation is intentionally left for a separate manual step.

One expected `gcc` command for MinGW-style environments is:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
gcc -municode -mwindows -g -O0 -Wall -Wextra src/main.c -o build\quick-terminal.exe -lshell32
```

I have not run this command yet.

## Auto-Start Debugging

Recommended local debug flow:

1. Rebuild the executable after source changes
2. Run:

```powershell
.\build\quick-terminal.exe --enable-autostart
```

3. Confirm the registry entry exists:

```powershell
Get-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name QuickTerminalHotkey
```

4. Verify the stored value looks like a quoted path to `quick-terminal.exe`
5. Run:

```powershell
.\build\quick-terminal.exe --autostart-status
```

6. Sign out and sign back in, or reboot, and confirm the hotkey works without manual launch
7. If needed, remove the entry with:

```powershell
.\build\quick-terminal.exe --disable-autostart
```

Useful fallback checks:

- If the registry value exists but the program does not start after sign-in, verify the stored path still matches the actual executable location
- If the registry value is from an older build, run `--enable-autostart` again so it refreshes to the latest command format
- If you move or rename the executable, run `--enable-autostart` again to refresh the registry value
- If the hotkey stops working after sign-in, check whether another program is already using `Ctrl` + `Alt` + `T`

## Tray Debugging

Recommended local debug flow:

1. Stop any running copy before rebuilding:

```powershell
Get-Process quick-terminal -ErrorAction SilentlyContinue
Stop-Process -Name quick-terminal -Force
```

2. Rebuild the executable
3. Launch the app normally:

```powershell
.\build\quick-terminal.exe
```

4. Confirm a tray icon appears in the Windows notification area
5. Double-click the tray icon and verify Windows Terminal opens
6. Right-click the tray icon and verify the menu items behave as expected
7. Choose `Exit` from the tray menu and confirm the process terminates

Startup balloon validation:

1. Run:

```powershell
.\build\quick-terminal.exe --enable-autostart
```

2. Sign out and sign back in
3. Confirm the tray icon appears automatically
4. Confirm a startup balloon appears stating that Quick Terminal is running

Tray visibility validation:

1. Start the app normally:

```powershell
.\build\quick-terminal.exe
```

2. Hide the tray:

```powershell
.\build\quick-terminal.exe --hide-tray
```

3. Confirm the tray icon disappears but `Ctrl` + `Alt` + `T` still works
4. Check the stored preference:

```powershell
.\build\quick-terminal.exe --tray-status
```

5. Show the tray again:

```powershell
.\build\quick-terminal.exe --show-tray
```

6. Confirm the tray icon reappears without restarting the running instance

## Validation Checklist

- The program starts successfully
- `Ctrl` + `Alt` + `T` works globally
- Windows Terminal opens
- PowerShell opens in a new tab
- Launching the program a second time does not create a competing hotkey listener
- The tray icon appears while the app is running
- The tray icon can be hidden and shown again through terminal commands
- The tray menu can open Terminal and exit the app
- `--enable-autostart` creates the expected Run registry value
- Login launches can show a tray startup confirmation balloon
- The hotkey works after sign-in without manual program launch

## Next Steps

Current status:

- Source review completed
- Manual compilation completed
- Runtime hotkey verification completed
- User-level auto-start support added to the source
- Tray support and startup notification support added to the source

Next planned step:

- Cross-machine compatibility hardening for open-source release
