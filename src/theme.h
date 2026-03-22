#pragma once
#include <windows.h>

enum class ThemeMode
{
    System,
    Light,
    Dark
};

// Undocumented Windows messages for menu bar painting.
// Part of UAH (User32 Aero Hooks), present since Vista.
// Struct layouts reverse-engineered by the community.
// See: https://github.com/adzm/win32-custom-menubar-aero-theme
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092

struct UAHMENUITEM
{
    int iPosition;
    UINT32 dwFlags;
    HMENU hMenu;
    RECT rcItem;
};

struct UAHMENU
{
    HMENU hMenu;
    HDC hdc;
    DWORD dwFlags;
};

struct UAHDRAWMENUITEM
{
    DRAWITEMSTRUCT dis;
    UAHMENU um;
    UAHMENUITEM umi;
};

class Theme
{
  public:
    Theme();
    ~Theme();

    void Initialize();
    void SetMode(ThemeMode mode);
    ThemeMode GetMode() const
    {
        return m_mode;
    }
    bool IsDark() const;

    void ApplyToWindow(HWND hwnd);

    // UAH menu bar dark mode (no owner-draw needed)
    bool HandleUahDrawMenu(HWND hwnd, LPARAM lParam);
    bool HandleUahDrawMenuItem(HWND hwnd, LPARAM lParam);
    void PaintDarkMenuBar(HWND hwnd);

    // Scrollbar dark mode
    static void ApplyToScrollbars(HWND hwndEdit, bool dark);

    // Invalidate and recreate cached menu font for new DPI
    void InvalidateMenuFont(int newDpi);

    HBRUSH GetEditBgBrush() const
    {
        return m_editBgBrush;
    }
    COLORREF GetEditBgColor() const
    {
        return m_editBgColor;
    }
    COLORREF GetEditFgColor() const
    {
        return m_editFgColor;
    }

    void OnSystemThemeChanged();

    void LoadFromSettings(DWORD mode);
    void SaveToSettings(DWORD &outMode) const
    {
        outMode = (DWORD)m_mode;
    }

    static void EnableDarkModeForApp();
    static bool IsSystemDarkMode();

  private:
    void UpdateColors();

    ThemeMode m_mode       = ThemeMode::System;
    bool m_systemIsDark    = false;
    HBRUSH m_editBgBrush   = nullptr;
    COLORREF m_editBgColor = RGB(255, 255, 255);
    COLORREF m_editFgColor = RGB(0, 0, 0);
    HBRUSH m_menuBgBrush   = nullptr;
    HFONT m_menuFont       = nullptr;
};
