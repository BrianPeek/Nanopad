#pragma once
#include <windows.h>
#include <string>

class Printer
{
  public:
    static bool Print(HWND hwndOwner, const std::wstring &text, const std::wstring &fileName, HFONT hFont);
};
