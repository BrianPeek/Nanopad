#include "font.h"
#include <commdlg.h>

FontManager::FontManager()
{
    ZeroMemory(&m_logFont, sizeof(m_logFont));
    // 11pt Consolas scaled by screen DPI (GetDpiForSystem() available on Win10+)
    HDC hdc = GetDC(nullptr);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
    if(hdc)
        ReleaseDC(nullptr, hdc);
    m_logFont.lfHeight  = -MulDiv(11, dpi, 72);
    m_logFont.lfWeight  = FW_NORMAL;
    m_logFont.lfCharSet = DEFAULT_CHARSET;
    m_logFont.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(m_logFont.lfFaceName, L"Consolas");
}

FontManager::~FontManager()
{
    if(m_hFont)
        DeleteObject(m_hFont);
}

void FontManager::EnsureFont()
{
    if(!m_fontCreated)
    {
        m_hFont       = CreateFontIndirectW(&m_logFont);
        m_fontCreated = true;
    }
}

HFONT FontManager::GetFont()
{
    EnsureFont();
    return m_hFont;
}

bool FontManager::ShowChooseFont(HWND hwndOwner)
{
    CHOOSEFONTW cf = {};
    cf.lStructSize = sizeof(cf);
    cf.hwndOwner   = hwndOwner;
    cf.lpLogFont   = &m_logFont;
    cf.Flags       = CF_INITTOLOGFONTSTRUCT | CF_SCREENFONTS | CF_EFFECTS;

    if(!ChooseFontW(&cf))
        return false;

    if(m_hFont)
        DeleteObject(m_hFont);

    m_hFont       = CreateFontIndirectW(&m_logFont);
    m_fontCreated = true;
    return m_hFont != nullptr;
}

void FontManager::LoadFromSettings(const LOGFONTW &lf)
{
    m_logFont = lf;
    if(m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    m_fontCreated = false;
}

void FontManager::OnDpiChanged(int newDpi, int oldDpi)
{
    if(oldDpi == 0)
        oldDpi = 96;
    m_logFont.lfHeight = MulDiv(m_logFont.lfHeight, newDpi, oldDpi);

    if(m_hFont)
    {
        DeleteObject(m_hFont);
        m_hFont = nullptr;
    }
    m_hFont       = CreateFontIndirectW(&m_logFont);
    m_fontCreated = true;
}
