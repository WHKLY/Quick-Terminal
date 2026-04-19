#include "notifications.h"
#include "qt_strsafe.h"
#include "tray.h"

#include <propkey.h>
#include <propsys.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wincrypt.h>

#ifndef CRYPT_STRING_NOCRLF
#define CRYPT_STRING_NOCRLF 0x40000000
#endif

static HRESULT InitPropVariantFromWideStringValue(const wchar_t *value, PROPVARIANT *variant)
{
    size_t value_length_bytes;

    if (value == NULL || variant == NULL)
    {
        return E_INVALIDARG;
    }

    PropVariantInit(variant);
    value_length_bytes = ((size_t)lstrlenW(value) + 1U) * sizeof(wchar_t);
    variant->pwszVal = (LPWSTR)CoTaskMemAlloc(value_length_bytes);
    if (variant->pwszVal == NULL)
    {
        return E_OUTOFMEMORY;
    }

    CopyMemory(variant->pwszVal, value, value_length_bytes);
    variant->vt = VT_LPWSTR;
    return S_OK;
}

static BOOL GetToastShortcutPath(wchar_t *buffer, size_t buffer_count)
{
    wchar_t programs_path[MAX_PATH];

    if (buffer == NULL || buffer_count == 0)
    {
        return FALSE;
    }

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, programs_path)))
    {
        return FALSE;
    }

    return SUCCEEDED(StringCchPrintfW(
        buffer,
        buffer_count,
        L"%s\\%s",
        programs_path,
        kToastShortcutName));
}

static BOOL EnsureToastShortcutInstalled(void)
{
    HRESULT hr;
    BOOL should_uninitialize = FALSE;
    IShellLinkW *shell_link = NULL;
    IPropertyStore *property_store = NULL;
    IPersistFile *persist_file = NULL;
    PROPVARIANT app_id;
    wchar_t executable_path[32768];
    wchar_t executable_directory[32768];
    wchar_t shortcut_path[32768];
    BOOL success = FALSE;

    if (!GetExecutablePath(executable_path, sizeof(executable_path) / sizeof(executable_path[0])) ||
        !GetExecutableDirectory(executable_directory, sizeof(executable_directory) / sizeof(executable_directory[0])) ||
        !GetToastShortcutPath(shortcut_path, sizeof(shortcut_path) / sizeof(shortcut_path[0])))
    {
        return FALSE;
    }

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(hr))
    {
        should_uninitialize = TRUE;
    }
    else if (hr != RPC_E_CHANGED_MODE)
    {
        return FALSE;
    }

    hr = CoCreateInstance(
        &CLSID_ShellLink,
        NULL,
        CLSCTX_INPROC_SERVER,
        &IID_IShellLinkW,
        (void **)&shell_link);
    if (FAILED(hr) || shell_link == NULL)
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetPath(shell_link, executable_path);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetArguments(shell_link, L"");
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetWorkingDirectory(shell_link, executable_directory);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetDescription(shell_link, kAppTitle);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->SetIconLocation(shell_link, executable_path, 0);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->QueryInterface(
        shell_link,
        &IID_IPropertyStore,
        (void **)&property_store);
    if (FAILED(hr) || property_store == NULL)
    {
        goto cleanup;
    }

    hr = InitPropVariantFromWideStringValue(kToastAppUserModelId, &app_id);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = property_store->lpVtbl->SetValue(property_store, &PKEY_AppUserModel_ID, &app_id);
    PropVariantClear(&app_id);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = property_store->lpVtbl->Commit(property_store);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    hr = shell_link->lpVtbl->QueryInterface(
        shell_link,
        &IID_IPersistFile,
        (void **)&persist_file);
    if (FAILED(hr) || persist_file == NULL)
    {
        goto cleanup;
    }

    hr = persist_file->lpVtbl->Save(persist_file, shortcut_path, TRUE);
    if (FAILED(hr))
    {
        goto cleanup;
    }

    success = TRUE;

cleanup:
    if (persist_file != NULL)
    {
        persist_file->lpVtbl->Release(persist_file);
    }

    if (property_store != NULL)
    {
        property_store->lpVtbl->Release(property_store);
    }

    if (shell_link != NULL)
    {
        shell_link->lpVtbl->Release(shell_link);
    }

    if (should_uninitialize)
    {
        CoUninitialize();
    }

    return success;
}

static BOOL EncodePowerShellCommand(const wchar_t *script, wchar_t *buffer, DWORD *buffer_count)
{
    DWORD characters_required = 0;
    const BYTE *script_bytes = (const BYTE *)script;
    DWORD script_bytes_length;

    if (script == NULL || buffer == NULL || buffer_count == NULL)
    {
        return FALSE;
    }

    script_bytes_length = (DWORD)(lstrlenW(script) * sizeof(wchar_t));

    if (!CryptBinaryToStringW(
            script_bytes,
            script_bytes_length,
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            NULL,
            &characters_required))
    {
        return FALSE;
    }

    if (*buffer_count < characters_required)
    {
        *buffer_count = characters_required;
        return FALSE;
    }

    if (!CryptBinaryToStringW(
            script_bytes,
            script_bytes_length,
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            buffer,
            &characters_required))
    {
        return FALSE;
    }

    *buffer_count = characters_required;
    return TRUE;
}

static BOOL RunHiddenProcessAndWait(const wchar_t *command_line, DWORD timeout_ms)
{
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;
    wchar_t mutable_command[32768];
    DWORD wait_result;
    DWORD exit_code = 1;

    if (command_line == NULL)
    {
        return FALSE;
    }

    if (FAILED(StringCchCopyW(
            mutable_command,
            sizeof(mutable_command) / sizeof(mutable_command[0]),
            command_line)))
    {
        return FALSE;
    }

    ZeroMemory(&startup_info, sizeof(startup_info));
    ZeroMemory(&process_info, sizeof(process_info));
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_HIDE;

    if (!CreateProcessW(
            NULL,
            mutable_command,
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &startup_info,
            &process_info))
    {
        return FALSE;
    }

    wait_result = WaitForSingleObject(process_info.hProcess, timeout_ms);
    if (wait_result == WAIT_OBJECT_0)
    {
        GetExitCodeProcess(process_info.hProcess, &exit_code);
    }
    else
    {
        TerminateProcess(process_info.hProcess, 1);
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return (wait_result == WAIT_OBJECT_0 && exit_code == 0);
}

BOOL ShowToastNotification(const wchar_t *title, const wchar_t *message)
{
    wchar_t system_directory[MAX_PATH];
    wchar_t powershell_path[32768];
    wchar_t script[8192];
    wchar_t encoded_script[16384];
    DWORD encoded_script_length = sizeof(encoded_script) / sizeof(encoded_script[0]);
    wchar_t command_line[32768];

    if (title == NULL || message == NULL)
    {
        return FALSE;
    }

    if (!EnsureToastShortcutInstalled())
    {
        return FALSE;
    }

    if (GetSystemDirectoryW(system_directory, sizeof(system_directory) / sizeof(system_directory[0])) == 0)
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            powershell_path,
            sizeof(powershell_path) / sizeof(powershell_path[0]),
            L"%s\\WindowsPowerShell\\v1.0\\powershell.exe",
            system_directory)))
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            script,
            sizeof(script) / sizeof(script[0]),
            L"$ErrorActionPreference='Stop';"
            L"[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] > $null;"
            L"[Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType = WindowsRuntime] > $null;"
            L"$xml = New-Object Windows.Data.Xml.Dom.XmlDocument;"
            L"$xml.LoadXml(\"<toast><visual><binding template='ToastGeneric'><text>%s</text><text>%s</text></binding></visual></toast>\");"
            L"$toast = [Windows.UI.Notifications.ToastNotification]::new($xml);"
            L"[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('%s').Show($toast);"
            L"exit 0;",
            title,
            message,
            kToastAppUserModelId)))
    {
        return FALSE;
    }

    if (!EncodePowerShellCommand(script, encoded_script, &encoded_script_length))
    {
        return FALSE;
    }

    if (FAILED(StringCchPrintfW(
            command_line,
            sizeof(command_line) / sizeof(command_line[0]),
            L"\"%s\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -EncodedCommand %s",
            powershell_path,
            encoded_script)))
    {
        return FALSE;
    }

    return RunHiddenProcessAndWait(command_line, 10000);
}

BOOL ShowStartupNotification(void)
{
    if (ShowToastNotification(
            L"Quick Terminal",
            L"Quick Terminal is running. Press Ctrl+Alt+T to open Windows Terminal."))
    {
        return TRUE;
    }

    if (g_app.tray_icon_added)
    {
        return ShowTrayNotification(
            L"Quick Terminal",
            L"Quick Terminal is running. Press Ctrl+Alt+T to open Windows Terminal.");
    }

    return FALSE;
}

void ShowInfoNotificationWithFallback(const wchar_t *message)
{
    if (ShowToastNotification(kAppTitle, message))
    {
        return;
    }

    if (g_app.tray_icon_added && ShowTrayNotification(kAppTitle, message))
    {
        return;
    }

    ShowInfoMessage(message);
}

void ShowStatusNotificationWithFallback(const wchar_t *toast_message, const wchar_t *fallback_message)
{
    if (toast_message == NULL)
    {
        return;
    }

    if (ShowToastNotification(kAppTitle, toast_message))
    {
        return;
    }

    if (g_app.tray_icon_added && ShowTrayNotification(kAppTitle, toast_message))
    {
        return;
    }

    ShowInfoMessage(fallback_message != NULL ? fallback_message : toast_message);
}

BOOL TestModernNotification(void)
{
    if (ShowToastNotification(
            L"Quick Terminal",
            L"This is a test notification from Quick Terminal."))
    {
        return TRUE;
    }

    ShowErrorMessage(L"Failed to show the Windows toast notification.");
    return FALSE;
}
