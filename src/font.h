#pragma once
#include <windows.h>

class FontManager
{
  public:
    FontManager();
    ~FontManager();

    bool ShowChooseFont(HWND hwndOwner);
    HFONT GetFont();

    void LoadFromSettings(const LOGFONTW &lf);
    void SaveToSettings(LOGFONTW &outLf) const
    {
        outLf = m_logFont;
    }

    // Recreate font scaled to new DPI
    void OnDpiChanged(int newDpi, int oldDpi);

  private:
    void EnsureFont();

    HFONT m_hFont      = nullptr;
    LOGFONTW m_logFont = {};
    bool m_fontCreated = false;
};
