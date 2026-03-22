#pragma once
#include <windows.h>

// Centers a MessageBox on the specified owner window instead of the monitor.
// Uses a thread-local CBT hook to reposition before display.

namespace MsgBoxCenter
{
inline HWND s_owner = nullptr;
inline HHOOK s_hook = nullptr;

inline LRESULT CALLBACK CBTProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if(nCode == HCBT_ACTIVATE && s_owner)
    {
        HWND hwndMsgBox = (HWND)wParam;
        RECT rcOwner, rcDlg;
        GetWindowRect(s_owner, &rcOwner);
        GetWindowRect(hwndMsgBox, &rcDlg);

        int dlgW = rcDlg.right - rcDlg.left;
        int dlgH = rcDlg.bottom - rcDlg.top;
        int x    = rcOwner.left + (rcOwner.right - rcOwner.left - dlgW) / 2;
        int y    = rcOwner.top + (rcOwner.bottom - rcOwner.top - dlgH) / 2;

        SetWindowPos(hwndMsgBox, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

        UnhookWindowsHookEx(s_hook);
        s_hook = nullptr;
    }
    return CallNextHookEx(s_hook, nCode, wParam, lParam);
}
} // namespace MsgBoxCenter

inline int CenteredMessageBox(HWND hwndOwner, const wchar_t *text, const wchar_t *caption, UINT type)
{
    MsgBoxCenter::s_owner = hwndOwner;
    MsgBoxCenter::s_hook  = SetWindowsHookExW(WH_CBT, MsgBoxCenter::CBTProc, nullptr, GetCurrentThreadId());
    return MessageBoxW(hwndOwner, text, caption, type);
}
