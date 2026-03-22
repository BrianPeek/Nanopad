#pragma once
#include <windows.h>
#include <commdlg.h>

class FindReplace
{
  public:
    FindReplace();

    void Initialize(HWND parent, HWND editor);
    void ShowFind();
    void ShowReplace();
    void FindNext(bool forward);
    bool HandleFindMessage(LPARAM lParam);

    HWND GetDialog() const
    {
        return m_hwndDialog;
    }
    static UINT GetFindMessageId();

  private:
    void DoFind(bool forward);
    void DoReplace();
    void DoReplaceAll();
    void PreFillFromSelection();

    HWND m_hwndParent         = nullptr;
    HWND m_hwndEditor         = nullptr;
    HWND m_hwndDialog         = nullptr;
    FINDREPLACEW m_fr         = {};
    wchar_t m_findBuf[256]    = {};
    wchar_t m_replaceBuf[256] = {};
};
