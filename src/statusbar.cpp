#include "statusbar.h"
#include "resource.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <cstdio>

StatusBar::StatusBar() = default;

StatusBar::~StatusBar()
{
    if(m_hwnd)
        RemoveWindowSubclass(m_hwnd, SubclassProc, 0);
    if(m_cachedBgBrush)
        DeleteObject(m_cachedBgBrush);
    if(m_cachedDivPen)
        DeleteObject(m_cachedDivPen);
}

bool StatusBar::Create(HWND parent, HINSTANCE hInst)
{
    m_hwnd = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, parent,
                             (HMENU)(UINT_PTR)IDC_STATUSBAR, hInst, nullptr);

    if(!m_hwnd)
        return false;

    SetWindowSubclass(m_hwnd, SubclassProc, 0, (DWORD_PTR)this);

    // Set parts: Line/Col | Chars/Lines | Encoding | Line Endings | (stretch)
    int parts[] = {200, 380, 490, 560, -1};
    SendMessage(m_hwnd, SB_SETPARTS, 5, (LPARAM)parts);

    return true;
}

void StatusBar::Resize()
{
    if(m_hwnd)
        SendMessage(m_hwnd, WM_SIZE, 0, 0);
}

void StatusBar::Update(int line, int col, int charCount, int lineCount, Encoding enc, LineEnding le)
{
    if(!m_hwnd || !m_visible)
        return;

    swprintf_s(m_partText[0], L"  Ln %d, Col %d", line, col);
    swprintf_s(m_partText[1], L"  %d chars, %d lines", charCount, lineCount);
    swprintf_s(m_partText[2], L"  %s", FileIO::EncodingToString(enc));
    swprintf_s(m_partText[3], L"  %s", FileIO::LineEndingToString(le));
    m_partText[4][0] = L'\0'; // Stretch part (fills remaining width)

    if(m_darkMode)
    {
        // Subclass proc handles painting
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
    else
    {
        for(int i = 0; i < 5; i++)
            SendMessage(m_hwnd, SB_SETTEXTW, i, (LPARAM)m_partText[i]);
    }
}

void StatusBar::SetVisible(bool visible)
{
    m_visible = visible;
    if(m_hwnd)
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
}

bool StatusBar::IsVisible() const
{
    return m_visible;
}

int StatusBar::GetHeight() const
{
    if(!m_hwnd || !m_visible)
        return 0;
    RECT rc;
    GetWindowRect(m_hwnd, &rc);
    return rc.bottom - rc.top;
}

void StatusBar::SetDarkMode(bool dark, COLORREF bg, COLORREF fg)
{
    m_darkMode = dark;
    m_bgColor  = bg;
    m_fgColor  = fg;

    // Recreate cached GDI objects
    if(m_cachedBgBrush)
    {
        DeleteObject(m_cachedBgBrush);
        m_cachedBgBrush = nullptr;
    }
    if(m_cachedDivPen)
    {
        DeleteObject(m_cachedDivPen);
        m_cachedDivPen = nullptr;
    }

    if(dark)
    {
        m_cachedBgBrush = CreateSolidBrush(bg);
        m_cachedDivPen  = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    }

    if(m_hwnd)
    {
        SetWindowTheme(m_hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

        if(!dark)
        {
            SendMessage(m_hwnd, SB_SETBKCOLOR, 0, (LPARAM)CLR_DEFAULT);
            for(int i = 0; i < 5; i++)
                SendMessage(m_hwnd, SB_SETTEXTW, i, (LPARAM)m_partText[i]);
        }

        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

LRESULT CALLBACK StatusBar::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR,
                                         DWORD_PTR dwRefData)
{
    auto *sb = (StatusBar *)dwRefData;

    if(msg == WM_ERASEBKGND && sb->m_darkMode)
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        if(sb->m_cachedBgBrush)
            FillRect(hdc, &rc, sb->m_cachedBgBrush);
        return 1;
    }

    if(msg == WM_PAINT && sb->m_darkMode)
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        // Fill entire background with cached brush
        if(sb->m_cachedBgBrush)
            FillRect(hdc, &rc, sb->m_cachedBgBrush);

        // Get part edges
        int parts[5] = {};
        int numParts = (int)SendMessage(hwnd, SB_GETPARTS, 5, (LPARAM)parts);

        // Select font
        HFONT hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
        if(!hFont)
            hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, sb->m_fgColor);

        // Use cached pen for dividers
        HPEN hOldPen = sb->m_cachedDivPen ? (HPEN)SelectObject(hdc, sb->m_cachedDivPen) : nullptr;

        int left = 0;
        for(int i = 0; i < numParts; i++)
        {
            int right = (parts[i] == -1) ? rc.right : parts[i];

            if(i > 0)
            {
                MoveToEx(hdc, left, rc.top + 3, nullptr);
                LineTo(hdc, left, rc.bottom - 3);
            }

            // Draw text
            if(sb->m_partText[i][0])
            {
                RECT rcPart = {left + 2, rc.top, right - 2, rc.bottom};
                DrawTextW(hdc, sb->m_partText[i], -1, &rcPart, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }

            left = right;
        }

        // Sizing grip
        COLORREF divColor = RGB(60, 60, 60);
        int gs            = GetSystemMetrics(SM_CXHSCROLL);
        for(int row = 0; row < 3; row++)
        {
            for(int col = row; col < 3; col++)
            {
                int x = rc.right - gs + col * 4 + 2;
                int y = rc.bottom - gs + row * 4 + 4;
                SetPixelV(hdc, x, y, divColor);
                SetPixelV(hdc, x + 1, y, divColor);
                SetPixelV(hdc, x, y + 1, divColor);
                SetPixelV(hdc, x + 1, y + 1, divColor);
            }
        }

        if(hOldPen)
            SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wParam, lParam);
}
