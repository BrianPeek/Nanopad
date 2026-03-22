#include "notepad_replace.h"
#include "msgbox.h"
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <cwchar>

bool NotepadReplace::IsElevated()
{
    BOOL elevated = FALSE;
    HANDLE hToken = nullptr;
    if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
        TOKEN_ELEVATION te = {};
        DWORD size         = sizeof(te);
        if(GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &size))
            elevated = te.TokenIsElevated;
        CloseHandle(hToken);
    }
    return elevated != FALSE;
}

bool NotepadReplace::GetExePath(wchar_t *buf, DWORD bufSize)
{
    return GetModuleFileNameW(nullptr, buf, bufSize) > 0;
}

bool NotepadReplace::RelaunchElevated(const wchar_t *args)
{
    wchar_t exePath[MAX_PATH];
    if(!GetExePath(exePath, MAX_PATH))
        return false;

    SHELLEXECUTEINFOW sei = {sizeof(sei)};
    sei.lpVerb            = L"runas";
    sei.lpFile            = exePath;
    sei.lpParameters      = args;
    sei.nShow             = SW_HIDE;
    sei.fMask             = SEE_MASK_NOCLOSEPROCESS;

    if(!ShellExecuteExW(&sei))
        return false;

    // Wait for the elevated process to finish
    if(sei.hProcess)
    {
        WaitForSingleObject(sei.hProcess, 10000);
        CloseHandle(sei.hProcess);
    }
    return true;
}

bool NotepadReplace::IsReplacing()
{
    HKEY hKey;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE, IFEO_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    // If UseFilter is enabled, IFEO Debugger is ignored
    DWORD useFilter = 0;
    DWORD ufSize    = sizeof(useFilter);
    RegQueryValueExW(hKey, L"UseFilter", nullptr, nullptr, (BYTE *)&useFilter, &ufSize);
    if(useFilter != 0)
    {
        RegCloseKey(hKey);
        return false;
    }

    wchar_t value[MAX_PATH] = {};
    DWORD size              = sizeof(value);
    DWORD type              = 0;
    bool result             = false;

    if(RegQueryValueExW(hKey, L"Debugger", nullptr, &type, (BYTE *)value, &size) == ERROR_SUCCESS && type == REG_SZ)
    {
        wchar_t exePath[MAX_PATH];
        if(GetExePath(exePath, MAX_PATH))
            result = (wcsstr(value, exePath) != nullptr);
    }

    RegCloseKey(hKey);
    return result;
}

bool NotepadReplace::Replace(HWND hwndOwner, Settings &settings)
{
    int result = CenteredMessageBox(hwndOwner,
                                    L"This will redirect all notepad.exe launches to Nanopad.\n\n"
                                    L"Requires administrator privileges.\n\n"
                                    L"Continue?",
                                    L"Replace Notepad", MB_YESNO | MB_ICONQUESTION);
    if(result != IDYES)
        return false;

    // Disable Store Notepad's App Paths redirect (HKCU, no admin needed)
    // Back up the current value and rename the key to disable it
    {
        static constexpr const wchar_t *APP_PATHS_KEY =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad.exe";
        static constexpr const wchar_t *APP_PATHS_BAK =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad.exe.nanopad-backup";

        HKEY hSrc;
        if(RegOpenKeyExW(HKEY_CURRENT_USER, APP_PATHS_KEY, 0, KEY_READ, &hSrc) == ERROR_SUCCESS)
        {
            // Read current default value
            wchar_t appPath[MAX_PATH] = {};
            DWORD size                = sizeof(appPath);
            RegQueryValueExW(hSrc, nullptr, nullptr, nullptr, (BYTE *)appPath, &size);
            RegCloseKey(hSrc);

            // Save to backup key
            HKEY hBak;
            if(RegCreateKeyExW(HKEY_CURRENT_USER, APP_PATHS_BAK, 0, nullptr, 0, KEY_WRITE, nullptr, &hBak, nullptr) ==
               ERROR_SUCCESS)
            {
                RegSetValueExW(hBak, nullptr, 0, REG_SZ, (const BYTE *)appPath,
                               (DWORD)((wcslen(appPath) + 1) * sizeof(wchar_t)));
                RegCloseKey(hBak);
            }

            // Delete the original key to disable Store Notepad
            RegDeleteTreeW(HKEY_CURRENT_USER, APP_PATHS_KEY);
        }
    }

    // If not elevated, relaunch with /register
    if(!IsElevated())
    {
        if(!RelaunchElevated(L"/register"))
        {
            CenteredMessageBox(hwndOwner, L"Failed to obtain administrator privileges.", L"Nanopad",
                               MB_OK | MB_ICONERROR);
            return false;
        }
        // Check if it worked
        return IsReplacing();
    }

    // We are elevated — do the registry work
    wchar_t exePath[MAX_PATH];
    if(!GetExePath(exePath, MAX_PATH))
        return false;

    // Read existing Debugger value (may belong to another Notepad replacement)
    HKEY hKey;
    if(RegCreateKeyExW(HKEY_LOCAL_MACHINE, IFEO_KEY, 0, nullptr, 0, KEY_READ | KEY_WRITE, nullptr, &hKey, nullptr) !=
       ERROR_SUCCESS)
        return false;

    wchar_t oldValue[MAX_PATH] = {};
    DWORD size                 = sizeof(oldValue);
    DWORD type                 = 0;
    RegQueryValueExW(hKey, L"Debugger", nullptr, &type, (BYTE *)oldValue, &size);

    // Save original UseFilter value
    DWORD oldUseFilter = 0;
    size               = sizeof(oldUseFilter);
    RegQueryValueExW(hKey, L"UseFilter", nullptr, nullptr, (BYTE *)&oldUseFilter, &size);

    // Save originals to settings
    wcscpy_s(settings.originalDebugger, oldValue);
    settings.originalDebuggerLoaded  = true;
    settings.originalUseFilter       = oldUseFilter;
    settings.originalUseFilterLoaded = true;
    settings.Save();

    // Write our exe as the Debugger and disable UseFilter
    wchar_t newValue[MAX_PATH + 4];
    swprintf_s(newValue, _countof(newValue), L"\"%s\"", exePath);
    DWORD newSize = (DWORD)((wcslen(newValue) + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, L"Debugger", 0, REG_SZ, (const BYTE *)newValue, newSize);

    DWORD useFilter = 0;
    RegSetValueExW(hKey, L"UseFilter", 0, REG_DWORD, (const BYTE *)&useFilter, sizeof(useFilter));

    RegCloseKey(hKey);

    return true;
}

bool NotepadReplace::Restore(HWND hwndOwner, Settings &settings)
{
    int result = CenteredMessageBox(hwndOwner,
                                    L"This will restore the original Notepad.\n\n"
                                    L"Requires administrator privileges.\n\n"
                                    L"Continue?",
                                    L"Restore Notepad", MB_YESNO | MB_ICONQUESTION);
    if(result != IDYES)
        return false;

    // Restore Store Notepad's App Paths redirect if we backed it up
    {
        static constexpr const wchar_t *APP_PATHS_KEY =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad.exe";
        static constexpr const wchar_t *APP_PATHS_BAK =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\notepad.exe.nanopad-backup";

        HKEY hBak;
        if(RegOpenKeyExW(HKEY_CURRENT_USER, APP_PATHS_BAK, 0, KEY_READ, &hBak) == ERROR_SUCCESS)
        {
            wchar_t appPath[MAX_PATH] = {};
            DWORD size                = sizeof(appPath);
            RegQueryValueExW(hBak, nullptr, nullptr, nullptr, (BYTE *)appPath, &size);
            RegCloseKey(hBak);

            if(appPath[0])
            {
                HKEY hDst;
                if(RegCreateKeyExW(HKEY_CURRENT_USER, APP_PATHS_KEY, 0, nullptr, 0, KEY_WRITE, nullptr, &hDst,
                                   nullptr) == ERROR_SUCCESS)
                {
                    RegSetValueExW(hDst, nullptr, 0, REG_SZ, (const BYTE *)appPath,
                                   (DWORD)((wcslen(appPath) + 1) * sizeof(wchar_t)));
                    RegCloseKey(hDst);
                }
            }

            RegDeleteTreeW(HKEY_CURRENT_USER, APP_PATHS_BAK);
        }
    }

    if(!IsElevated())
    {
        if(!RelaunchElevated(L"/unregister"))
        {
            CenteredMessageBox(hwndOwner, L"Failed to obtain administrator privileges.", L"Nanopad",
                               MB_OK | MB_ICONERROR);
            return false;
        }
        return !IsReplacing();
    }

    // We are elevated — restore registry
    HKEY hKey;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE, IFEO_KEY, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return false;

    if(settings.originalDebuggerLoaded && settings.originalDebugger[0])
    {
        DWORD size = (DWORD)((wcslen(settings.originalDebugger) + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"Debugger", 0, REG_SZ, (const BYTE *)settings.originalDebugger, size);
    }
    else
    {
        RegDeleteValueW(hKey, L"Debugger");
    }

    // Restore UseFilter
    if(settings.originalUseFilterLoaded)
    {
        RegSetValueExW(hKey, L"UseFilter", 0, REG_DWORD, (const BYTE *)&settings.originalUseFilter,
                       sizeof(settings.originalUseFilter));
    }

    RegCloseKey(hKey);

    // Clear saved original
    settings.originalDebugger[0]    = L'\0';
    settings.originalDebuggerLoaded = false;
    settings.Save();

    return true;
}

void NotepadReplace::StripNotepadFromCmdLine(std::wstring &cmdLine)
{
    // When launched via IFEO Debugger, the command line is:
    //   "C:\path\to\nanopad.exe" C:\Windows\notepad.exe somefile.txt
    // or:
    //   "C:\path\to\nanopad.exe" "C:\Windows\notepad.exe" somefile.txt
    // We need to strip the notepad.exe part.

    if(cmdLine.empty())
        return;

    // Find "notepad.exe" (case-insensitive) in the command line
    std::wstring lower = cmdLine;
    CharLowerBuffW(lower.data(), (DWORD)lower.size());

    size_t notepadLen = wcslen(L"notepad.exe");
    size_t pos        = lower.find(L"notepad.exe");
    if(pos == std::wstring::npos)
        return;

    size_t end = pos + notepadLen;

    // If notepad.exe was quoted, skip the closing quote
    if(end < cmdLine.size() && cmdLine[end] == L'"')
        end++;

    // Skip whitespace after it
    while(end < cmdLine.size() && cmdLine[end] == L' ')
        end++;

    // Check if there was a leading quote before notepad path
    if(pos > 0 && cmdLine[pos - 1] == L'"')
        pos--;
    // Skip whitespace before the notepad token
    while(pos > 0 && cmdLine[pos - 1] == L' ')
        pos--;

    // Replace the whole thing: keep everything after notepad.exe
    cmdLine = cmdLine.substr(end);
}

// --- Open With registration (HKCU, no admin) ---

static bool SetRegString(HKEY hParent, const wchar_t *subKey, const wchar_t *valueName, const wchar_t *data)
{
    HKEY hKey;
    if(RegCreateKeyExW(hParent, subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return false;
    DWORD size = (DWORD)((wcslen(data) + 1) * sizeof(wchar_t));
    RegSetValueExW(hKey, valueName, 0, REG_SZ, (const BYTE *)data, size);
    RegCloseKey(hKey);
    return true;
}

static void DeleteRegTree(HKEY hParent, const wchar_t *subKey)
{
    RegDeleteTreeW(hParent, subKey);
}

bool NotepadReplace::RegisterOpenWith()
{
    wchar_t exePath[MAX_PATH];
    if(!GetExePath(exePath, MAX_PATH))
        return false;

    // Build command: "C:\path\nanopad.exe" "%1"
    wchar_t command[MAX_PATH + 16];
    swprintf_s(command, L"\"%s\" \"%%1\"", exePath);

    // Register ProgId: HKCU\Software\Classes\Nanopad.TextFile\shell\open\command
    wchar_t progIdCmd[256];
    swprintf_s(progIdCmd, L"Software\\Classes\\%s\\shell\\open\\command", PROGID);
    SetRegString(HKEY_CURRENT_USER, progIdCmd, nullptr, command);

    // Set friendly name
    wchar_t progIdKey[256];
    swprintf_s(progIdKey, L"Software\\Classes\\%s", PROGID);
    SetRegString(HKEY_CURRENT_USER, progIdKey, nullptr, L"Nanopad Text File");

    // Set icon
    wchar_t iconKey[256];
    swprintf_s(iconKey, L"Software\\Classes\\%s\\DefaultIcon", PROGID);
    wchar_t iconPath[MAX_PATH + 4];
    swprintf_s(iconPath, L"\"%s\",0", exePath);
    SetRegString(HKEY_CURRENT_USER, iconKey, nullptr, iconPath);

    // Register app: HKCU\Software\Classes\Applications\nanopad.exe\shell\open\command
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\nanopad.exe\\shell\\open\\command", nullptr,
                 command);

    // Set friendly app name (shown in "Open with" instead of "nanopad.exe")
    SetRegString(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\nanopad.exe", L"FriendlyAppName", L"Nanopad");

    // For each text extension, add to OpenWithProgids
    for(const wchar_t *ext : TEXT_EXTENSIONS)
    {
        wchar_t extKey[128];
        swprintf_s(extKey, L"Software\\Classes\\%s\\OpenWithProgids", ext);

        HKEY hKey;
        if(RegCreateKeyExW(HKEY_CURRENT_USER, extKey, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) ==
           ERROR_SUCCESS)
        {
            // Empty REG_NONE value with the ProgId as the name
            RegSetValueExW(hKey, PROGID, 0, REG_NONE, nullptr, 0);
            RegCloseKey(hKey);
        }
    }

    // Notify the shell
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool NotepadReplace::UnregisterOpenWith()
{
    // Remove ProgId
    wchar_t progIdKey[256];
    swprintf_s(progIdKey, L"Software\\Classes\\%s", PROGID);
    DeleteRegTree(HKEY_CURRENT_USER, progIdKey);

    // Remove app registration
    DeleteRegTree(HKEY_CURRENT_USER, L"Software\\Classes\\Applications\\nanopad.exe");

    // Remove from each extension's OpenWithProgids
    for(const wchar_t *ext : TEXT_EXTENSIONS)
    {
        wchar_t extKey[128];
        swprintf_s(extKey, L"Software\\Classes\\%s\\OpenWithProgids", ext);

        HKEY hKey;
        if(RegOpenKeyExW(HKEY_CURRENT_USER, extKey, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
        {
            RegDeleteValueW(hKey, PROGID);
            RegCloseKey(hKey);
        }
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool NotepadReplace::IsOpenWithRegistered()
{
    wchar_t progIdKey[256];
    swprintf_s(progIdKey, L"Software\\Classes\\%s", PROGID);

    HKEY hKey;
    if(RegOpenKeyExW(HKEY_CURRENT_USER, progIdKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    RegCloseKey(hKey);
    return true;
}

// --- Context menu registration (HKCU, no admin) ---

// Classic context menu: HKCU\Software\Classes\*\shell\NanopadEdit
// Appears in "Show more options" on Win11, directly on Win10.
static constexpr const wchar_t *CTX_SHELL_KEY = L"Software\\Classes\\*\\shell\\NanopadEdit";
static constexpr const wchar_t *CTX_CMD_KEY   = L"Software\\Classes\\*\\shell\\NanopadEdit\\command";

bool NotepadReplace::RegisterContextMenu()
{
    wchar_t exePath[MAX_PATH];
    if(!GetExePath(exePath, MAX_PATH))
        return false;

    wchar_t command[MAX_PATH + 16];
    swprintf_s(command, _countof(command), L"\"%s\" \"%%1\"", exePath);

    // Create the shell verb
    SetRegString(HKEY_CURRENT_USER, CTX_SHELL_KEY, nullptr, L"Edit in Nanopad");

    // Set icon
    wchar_t iconPath[MAX_PATH + 4];
    swprintf_s(iconPath, _countof(iconPath), L"\"%s\",0", exePath);
    SetRegString(HKEY_CURRENT_USER, CTX_SHELL_KEY, L"Icon", iconPath);

    // Set command
    SetRegString(HKEY_CURRENT_USER, CTX_CMD_KEY, nullptr, command);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool NotepadReplace::UnregisterContextMenu()
{
    DeleteRegTree(HKEY_CURRENT_USER, CTX_SHELL_KEY);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool NotepadReplace::IsContextMenuRegistered()
{
    HKEY hKey;
    if(RegOpenKeyExW(HKEY_CURRENT_USER, CTX_SHELL_KEY, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;
    RegCloseKey(hKey);
    return true;
}
