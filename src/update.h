#pragma once
#include <windows.h>
#include "version.h"

// Background update checker — runs on a worker thread, never blocks UI.
// Silently fails on network errors, 404s, or missing releases.
class UpdateChecker
{
  public:
    // Starts a background thread to check for updates.
    // If a newer version is found, posts WM_APP_UPDATE_AVAILABLE to hwnd.
    static void CheckAsync(HWND hwnd);

    // Call from the message handler when WM_APP_UPDATE_AVAILABLE arrives.
    static void ShowUpdateDialog(HWND hwnd);

    // Show About dialog with version info and update status.
    static void ShowAboutDialog(HWND hwnd);

    // Open the release page in the default browser.
    static void OpenReleasePage();

    static constexpr UINT WM_APP_UPDATE_AVAILABLE = WM_APP + 2;

  private:
    static DWORD WINAPI CheckThread(LPVOID param);
    static bool DoCheck();

    static wchar_t s_newVersion[64];
    static wchar_t s_releaseUrl[512];
    static bool s_updateAvailable;
};
