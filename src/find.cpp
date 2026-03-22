#include "find.h"
#include "msgbox.h"
#include <string>

FindReplace::FindReplace() = default;

void FindReplace::Initialize(HWND parent, HWND editor)
{
    m_hwndParent = parent;
    m_hwndEditor = editor;
}

UINT FindReplace::GetFindMessageId()
{
    static UINT id = RegisterWindowMessageW(FINDMSGSTRINGW);
    return id;
}

void FindReplace::PreFillFromSelection()
{
    DWORD selStart = 0, selEnd = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    if(selStart == selEnd || (selEnd - selStart) >= 255)
        return;

    int line      = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selStart, 0);
    int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
    int lineLen   = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, selStart, 0);
    if(lineLen <= 0 || lineLen >= 1024)
        return;

    wchar_t lineBuf[1024];
    // EM_GETLINE requires the first WORD to contain the buffer size.
    // It does NOT null-terminate the result.
    *(WORD *)lineBuf = (WORD)_countof(lineBuf);
    int got          = (int)SendMessage(m_hwndEditor, EM_GETLINE, line, (LPARAM)lineBuf);
    if(got <= 0 || got >= 1024)
        return;
    lineBuf[got] = L'\0';

    int selOffset = selStart - lineStart;
    int selLen    = selEnd - selStart;
    if(selOffset + selLen <= got)
        wcsncpy_s(m_findBuf, lineBuf + selOffset, selLen);
}

void FindReplace::ShowFind()
{
    if(m_hwndDialog)
    {
        SetFocus(m_hwndDialog);
        return;
    }

    PreFillFromSelection();

    ZeroMemory(&m_fr, sizeof(m_fr));
    m_fr.lStructSize   = sizeof(m_fr);
    m_fr.hwndOwner     = m_hwndParent;
    m_fr.lpstrFindWhat = m_findBuf;
    m_fr.wFindWhatLen  = (WORD)_countof(m_findBuf);
    m_fr.Flags         = FR_DOWN;

    m_hwndDialog = FindTextW(&m_fr);
}

void FindReplace::ShowReplace()
{
    if(m_hwndDialog)
    {
        SetFocus(m_hwndDialog);
        return;
    }

    PreFillFromSelection();

    ZeroMemory(&m_fr, sizeof(m_fr));
    m_fr.lStructSize      = sizeof(m_fr);
    m_fr.hwndOwner        = m_hwndParent;
    m_fr.lpstrFindWhat    = m_findBuf;
    m_fr.wFindWhatLen     = (WORD)_countof(m_findBuf);
    m_fr.lpstrReplaceWith = m_replaceBuf;
    m_fr.wReplaceWithLen  = (WORD)_countof(m_replaceBuf);
    m_fr.Flags            = FR_DOWN;

    m_hwndDialog = ReplaceTextW(&m_fr);
}

void FindReplace::FindNext(bool forward)
{
    if(m_findBuf[0] == L'\0')
    {
        ShowFind();
        return;
    }
    DoFind(forward);
}

bool FindReplace::HandleFindMessage(LPARAM lParam)
{
    FINDREPLACEW *pfr = (FINDREPLACEW *)lParam;

    if(pfr->Flags & FR_DIALOGTERM)
    {
        m_hwndDialog = nullptr;
        return true;
    }

    if(pfr->Flags & FR_FINDNEXT)
    {
        DoFind((pfr->Flags & FR_DOWN) != 0);
        return true;
    }

    if(pfr->Flags & FR_REPLACE)
    {
        DoReplace();
        return true;
    }

    if(pfr->Flags & FR_REPLACEALL)
    {
        DoReplaceAll();
        return true;
    }

    return false;
}

void FindReplace::DoFind(bool forward)
{
    if(m_findBuf[0] == L'\0')
        return;

    int textLen = GetWindowTextLengthW(m_hwndEditor);
    if(textLen == 0)
        return;

    // Single text copy — modified in-place for case-insensitive search
    std::wstring text((size_t)textLen, L'\0');
    GetWindowTextW(m_hwndEditor, text.data(), textLen + 1);

    DWORD selStart = 0, selEnd = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    if(selEnd > (DWORD)text.size())
        selEnd = (DWORD)text.size();

    bool matchCase = (m_fr.Flags & FR_MATCHCASE) != 0;

    // Build lowercase find term on stack (m_findBuf is max 256)
    wchar_t findLower[256];
    size_t findLen = wcslen(m_findBuf);
    wmemcpy(findLower, m_findBuf, findLen + 1);

    if(!matchCase)
    {
        CharLowerBuffW(text.data(), (DWORD)text.size());
        CharLowerBuffW(findLower, (DWORD)findLen);
    }

    size_t pos = std::wstring::npos;

    if(forward)
    {
        pos = text.find(findLower, selEnd, findLen);
        if(pos == std::wstring::npos)
            pos = text.find(findLower, 0, findLen);
    }
    else
    {
        size_t searchStart = (selStart > 0) ? selStart - 1 : 0;
        pos                = text.rfind(findLower, searchStart, findLen);
        if(pos == std::wstring::npos)
            pos = text.rfind(findLower, std::wstring::npos, findLen);
    }

    if(pos != std::wstring::npos)
    {
        SendMessage(m_hwndEditor, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + findLen));
        SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
    }
    else
    {
        CenteredMessageBox(m_hwndParent, L"Cannot find the specified text.", L"Nanopad", MB_OK | MB_ICONINFORMATION);
    }
}

void FindReplace::DoReplace()
{
    DWORD selStart = 0, selEnd = 0;
    SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);

    if(selStart != selEnd)
    {
        // Compare selection with find term using EM_GETLINE (no full text copy)
        int selLen     = selEnd - selStart;
        size_t findLen = wcslen(m_findBuf);
        bool matchCase = (m_fr.Flags & FR_MATCHCASE) != 0;

        if(selLen == (int)findLen && selLen < 1024)
        {
            int line      = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, selStart, 0);
            int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, line, 0);
            wchar_t lineBuf[1024];
            *(WORD *)lineBuf = (WORD)_countof(lineBuf);
            int got          = (int)SendMessage(m_hwndEditor, EM_GETLINE, line, (LPARAM)lineBuf);
            if(got <= 0 || got >= 1024)
            {
                DoFind(true);
                return;
            }
            lineBuf[got] = L'\0';

            int selOffset = selStart - lineStart;
            if(got > 0 && selOffset + selLen <= got)
            {
                bool matches = matchCase ? (wcsncmp(lineBuf + selOffset, m_findBuf, findLen) == 0)
                                         : (_wcsnicmp(lineBuf + selOffset, m_findBuf, findLen) == 0);

                if(matches)
                    SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)m_replaceBuf);
            }
        }
    }

    DoFind(true);
}

void FindReplace::DoReplaceAll()
{
    if(m_findBuf[0] == L'\0')
        return;

    int textLen = GetWindowTextLengthW(m_hwndEditor);
    if(textLen == 0)
        return;

    std::wstring text((size_t)textLen, L'\0');
    GetWindowTextW(m_hwndEditor, text.data(), textLen + 1);

    size_t findLen = wcslen(m_findBuf);
    bool matchCase = (m_fr.Flags & FR_MATCHCASE) != 0;

    std::wstring lowerText;
    wchar_t findLower[256];
    wmemcpy(findLower, m_findBuf, findLen + 1);

    if(!matchCase)
    {
        lowerText = text;
        CharLowerBuffW(lowerText.data(), (DWORD)lowerText.size());
        CharLowerBuffW(findLower, (DWORD)findLen);
    }

    const std::wstring &searchText = matchCase ? text : lowerText;

    const wchar_t *replaceStr = m_replaceBuf;
    size_t replaceLen         = wcslen(replaceStr);

    std::wstring result;
    result.reserve(text.size());
    int count  = 0;
    size_t pos = 0;

    while(pos < text.size())
    {
        size_t found = searchText.find(findLower, pos, findLen);
        if(found == std::wstring::npos)
        {
            result.append(text, pos);
            break;
        }
        result.append(text, pos, found - pos);
        result.append(replaceStr, replaceLen);
        pos = found + findLen;
        count++;
    }

    if(count > 0)
    {
        SetWindowTextW(m_hwndEditor, result.c_str());

        wchar_t msg[64];
        swprintf_s(msg, L"Replaced %d occurrence(s).", count);
        CenteredMessageBox(m_hwndParent, msg, L"Nanopad", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CenteredMessageBox(m_hwndParent, L"Cannot find the specified text.", L"Nanopad", MB_OK | MB_ICONINFORMATION);
    }
}
