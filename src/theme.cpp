#include "theme.h"
#include <dwmapi.h>
#include <uxtheme.h>

// Undocumented dark mode APIs from uxtheme.dll
using fnSetPreferredAppMode              = int(WINAPI *)(int);
using fnAllowDarkModeForWindow           = BOOL(WINAPI *)(HWND, BOOL);
using fnRefreshImmersiveColorPolicyState = void(WINAPI *)();
using fnFlushMenuThemes                  = void(WINAPI *)();

static fnSetPreferredAppMode pSetPreferredAppMode                           = nullptr;
static fnAllowDarkModeForWindow pAllowDarkModeForWindow                     = nullptr;
static fnRefreshImmersiveColorPolicyState pRefreshImmersiveColorPolicyState = nullptr;
static fnFlushMenuThemes pFlushMenuThemes                                   = nullptr;
static bool s_darkApisLoaded                                                = false;

static void LoadDarkModeAPIs()
{
    if(s_darkApisLoaded)
        return;
    s_darkApisLoaded = true;

    HMODULE hUxTheme = GetModuleHandleW(L"uxtheme.dll");
    if(!hUxTheme)
    {
        wchar_t sysDir[MAX_PATH];
        GetSystemDirectoryW(sysDir, MAX_PATH);
        wcscat_s(sysDir, L"\\uxtheme.dll");
        hUxTheme = LoadLibraryW(sysDir);
    }
    if(!hUxTheme)
        return;

    pSetPreferredAppMode    = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
    pAllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
    pRefreshImmersiveColorPolicyState =
        (fnRefreshImmersiveColorPolicyState)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(104));
    pFlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));
}

Theme::Theme() = default;

Theme::~Theme()
{
    if(m_editBgBrush)
        DeleteObject(m_editBgBrush);
    if(m_menuBgBrush)
        DeleteObject(m_menuBgBrush);
    if(m_menuFont)
        DeleteObject(m_menuFont);
}

void Theme::Initialize()
{
    m_systemIsDark = IsSystemDarkMode();
    UpdateColors();
}

void Theme::EnableDarkModeForApp()
{
    LoadDarkModeAPIs();
    if(pSetPreferredAppMode)
        pSetPreferredAppMode(1); // AllowDark
}

bool Theme::IsSystemDarkMode()
{
    HKEY hKey;
    if(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD value = 1;
        DWORD size  = sizeof(value);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr, (BYTE *)&value, &size);
        RegCloseKey(hKey);
        return value == 0;
    }
    return false;
}

void Theme::SetMode(ThemeMode mode)
{
    m_mode = mode;
    UpdateColors();
}

bool Theme::IsDark() const
{
    switch(m_mode)
    {
        case ThemeMode::Dark:
            return true;
        case ThemeMode::Light:
            return false;
        case ThemeMode::System:
            return m_systemIsDark;
    }
    return false;
}

void Theme::UpdateColors()
{
    if(m_editBgBrush)
    {
        DeleteObject(m_editBgBrush);
        m_editBgBrush = nullptr;
    }

    if(IsDark())
    {
        m_editBgColor = RGB(20, 20, 20);
        m_editFgColor = RGB(220, 220, 220);
    }
    else
    {
        m_editBgColor = RGB(255, 255, 255);
        m_editFgColor = RGB(0, 0, 0);
    }

    m_editBgBrush = CreateSolidBrush(m_editBgColor);

    if(m_menuBgBrush)
    {
        DeleteObject(m_menuBgBrush);
        m_menuBgBrush = nullptr;
    }
    if(IsDark())
        m_menuBgBrush = CreateSolidBrush(RGB(38, 38, 38));

    // Cache menu font (lazy init, recreated on DPI change)
    if(!m_menuFont)
    {
        NONCLIENTMETRICSW ncm = {sizeof(ncm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        m_menuFont = CreateFontIndirectW(&ncm.lfMenuFont);
    }
}

void Theme::ApplyToWindow(HWND hwnd)
{
    LoadDarkModeAPIs();

    BOOL useDark = IsDark() ? TRUE : FALSE;

    // Update preferred app mode for popup/context menus
    // 1=AllowDark (follow system), 2=ForceDark, 3=ForceLight
    if(pSetPreferredAppMode)
    {
        int mode = (m_mode == ThemeMode::System) ? 1 : (useDark ? 2 : 3);
        pSetPreferredAppMode(mode);
    }

    // Dark title bar (DWMWA_USE_IMMERSIVE_DARK_MODE = 20)
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));

    // Dark mode for window (menu bar), context menus, scrollbars
    if(pAllowDarkModeForWindow)
        pAllowDarkModeForWindow(hwnd, useDark);

    // Apply DarkMode_Explorer theme to the main window for native dark menu bar
    SetWindowTheme(hwnd, useDark ? L"DarkMode_Explorer" : nullptr, nullptr);

    if(pFlushMenuThemes)
        pFlushMenuThemes();

    if(pRefreshImmersiveColorPolicyState)
        pRefreshImmersiveColorPolicyState();

    UpdateColors();

    DrawMenuBar(hwnd);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void Theme::OnSystemThemeChanged()
{
    m_systemIsDark = IsSystemDarkMode();
    if(m_mode == ThemeMode::System)
        UpdateColors();
}

void Theme::LoadFromSettings(DWORD mode)
{
    if(mode <= 2)
        m_mode = (ThemeMode)mode;
}

// --- Scrollbar dark mode ---

void Theme::ApplyToScrollbars(HWND hwndEdit, bool dark)
{
    SetWindowTheme(hwndEdit, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}

void Theme::InvalidateMenuFont(int newDpi)
{
    if(m_menuFont)
    {
        DeleteObject(m_menuFont);
        m_menuFont = nullptr;
    }

    // Use DPI-aware API if available (Win10 1607+)
    using fnSPIForDpi      = BOOL(WINAPI *)(UINT, UINT, PVOID, UINT, UINT);
    static auto pSPIForDpi = (fnSPIForDpi)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SystemParametersInfoForDpi");

    NONCLIENTMETRICSW ncm = {sizeof(ncm)};
    if(pSPIForDpi)
        pSPIForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, newDpi);
    else
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);

    m_menuFont = CreateFontIndirectW(&ncm.lfMenuFont);
}

bool Theme::HandleUahDrawMenu(HWND hwnd, LPARAM lParam)
{
    if(!IsDark())
        return false;

    auto *pUDM      = (UAHMENU *)lParam;
    MENUBARINFO mbi = {sizeof(mbi)};
    if(GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        RECT rcWin;
        GetWindowRect(hwnd, &rcWin);
        RECT rcBar = mbi.rcBar;
        OffsetRect(&rcBar, -rcWin.left, -rcWin.top);
        HBRUSH hBr = m_menuBgBrush ? m_menuBgBrush : (HBRUSH)GetStockObject(BLACK_BRUSH);
        FillRect(pUDM->hdc, &rcBar, hBr);
    }
    return true;
}

bool Theme::HandleUahDrawMenuItem(HWND /*hwnd*/, LPARAM lParam)
{
    if(!IsDark())
        return false;

    auto *pUDMI = (UAHDRAWMENUITEM *)lParam;

    // Get menu item text
    wchar_t text[256] = {};
    MENUITEMINFOW mii = {sizeof(mii)};
    mii.fMask         = MIIM_STRING;
    mii.dwTypeData    = text;
    mii.cch           = 255;
    GetMenuItemInfoW(pUDMI->um.hMenu, pUDMI->umi.iPosition, TRUE, &mii);

    // Choose colors
    COLORREF bgColor   = RGB(38, 38, 38);
    COLORREF textColor = RGB(230, 230, 230);
    if((pUDMI->dis.itemState & ODS_HOTLIGHT) || (pUDMI->dis.itemState & ODS_SELECTED))
        bgColor = RGB(65, 65, 65);

    HBRUSH hBr = CreateSolidBrush(bgColor);
    FillRect(pUDMI->um.hdc, &pUDMI->dis.rcItem, hBr);
    DeleteObject(hBr);

    // Draw text with cached menu font
    HFONT hOldFont = m_menuFont ? (HFONT)SelectObject(pUDMI->um.hdc, m_menuFont) : nullptr;

    SetBkMode(pUDMI->um.hdc, TRANSPARENT);
    SetTextColor(pUDMI->um.hdc, textColor);

    RECT rcText = pUDMI->dis.rcItem;
    DrawTextW(pUDMI->um.hdc, text, -1, &rcText, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    if(hOldFont)
        SelectObject(pUDMI->um.hdc, hOldFont);

    return true;
}

void Theme::PaintDarkMenuBar(HWND hwnd)
{
    if(!IsDark())
        return;

    HDC hdc = GetWindowDC(hwnd);
    if(!hdc)
        return;

    MENUBARINFO mbi = {sizeof(mbi)};
    if(!GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
    {
        ReleaseDC(hwnd, hdc);
        return;
    }

    RECT rcWin;
    GetWindowRect(hwnd, &rcWin);
    RECT rcBar = mbi.rcBar;
    OffsetRect(&rcBar, -rcWin.left, -rcWin.top);
    rcBar.bottom += 1; // Cover the border line

    HBRUSH hBr = m_menuBgBrush ? m_menuBgBrush : (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(hdc, &rcBar, hBr);

    // Draw menu item text
    HMENU hMenu = GetMenu(hwnd);
    int count   = GetMenuItemCount(hMenu);

    HFONT hOldFont = m_menuFont ? (HFONT)SelectObject(hdc, m_menuFont) : nullptr;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(230, 230, 230));

    for(int i = 0; i < count; i++)
    {
        MENUBARINFO itemMbi = {sizeof(itemMbi)};
        if(GetMenuBarInfo(hwnd, OBJID_MENU, i + 1, &itemMbi))
        {
            RECT rcItem = itemMbi.rcBar;
            OffsetRect(&rcItem, -rcWin.left, -rcWin.top);

            wchar_t text[256] = {};
            MENUITEMINFOW mii = {sizeof(mii)};
            mii.fMask         = MIIM_STRING;
            mii.dwTypeData    = text;
            mii.cch           = 255;
            GetMenuItemInfoW(hMenu, i, TRUE, &mii);

            DrawTextW(hdc, text, -1, &rcItem, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
    }

    if(hOldFont)
        SelectObject(hdc, hOldFont);
    ReleaseDC(hwnd, hdc);
}
