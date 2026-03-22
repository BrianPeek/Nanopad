#pragma once
#include <windows.h>
#include "file_io.h"

class StatusBar
{
  public:
    StatusBar();
    ~StatusBar();

    bool Create(HWND parent, HINSTANCE hInst);
    void Resize();
    void Update(int line, int col, int charCount, int lineCount, Encoding enc, LineEnding le);
    void SetVisible(bool visible);
    bool IsVisible() const;
    HWND GetHwnd() const
    {
        return m_hwnd;
    }
    int GetHeight() const;
    void SetDarkMode(bool dark, COLORREF bg, COLORREF fg);

  private:
    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass,
                                         DWORD_PTR dwRefData);

    HWND m_hwnd               = nullptr;
    bool m_visible            = true;
    wchar_t m_partText[5][64] = {};
    COLORREF m_bgColor        = 0;
    COLORREF m_fgColor        = RGB(0, 0, 0);
    bool m_darkMode           = false;
    HBRUSH m_cachedBgBrush    = nullptr;
    HPEN m_cachedDivPen       = nullptr;
};
