# Windows `Ctrl` + `Alt` + `T` Quick Terminal

## V1 Goal

On Windows, press `Ctrl` + `Alt` + `T` to open Windows Terminal and start PowerShell.

This version does not force a custom startup directory. PowerShell uses its default initial directory behavior.

## Recommended V1 Solution

Implement a small resident Windows program in `C` with WinAPI.

The program:

1. Registers the global hotkey `Ctrl` + `Alt` + `T`
2. Runs in the background with a message loop
3. Launches Windows Terminal on hotkey press
4. Opens a PowerShell tab by calling:

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

## V1 Behavior

After the program starts:

1. It waits in the background
2. Press `Ctrl` + `Alt` + `T`
3. Windows Terminal opens
4. A new PowerShell tab is created

## Implementation Notes

The first version includes:

- Global hotkey registration with `RegisterHotKey`
- Message loop based on `GetMessage`
- Terminal launch via `ShellExecuteW`
- A single-instance guard using a named mutex
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
- `--disable-autostart` removes that value
- `--autostart-status` shows whether the Run value exists and displays the stored command
- Running the program without arguments still starts the background hotkey listener

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
- If you move or rename the executable, run `--enable-autostart` again to refresh the registry value
- If the hotkey stops working after sign-in, check whether another program is already using `Ctrl` + `Alt` + `T`

## Validation Checklist

- The program starts successfully
- `Ctrl` + `Alt` + `T` works globally
- Windows Terminal opens
- PowerShell opens in a new tab
- Launching the program a second time does not create a competing hotkey listener
- `--enable-autostart` creates the expected Run registry value
- The hotkey works after sign-in without manual program launch

## Next Steps

Current status:

- Source review completed
- Manual compilation completed
- Runtime hotkey verification completed
- User-level auto-start support added to the source

Next planned step:

- Cross-machine compatibility hardening for open-source release
