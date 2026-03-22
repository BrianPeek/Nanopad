#include "print.h"
#include <commdlg.h>

// Iterator-based line walking — avoids building a vector of all lines
static size_t CountLines(const std::wstring &text)
{
    size_t count = 1;
    size_t pos   = 0;
    while((pos = text.find(L'\n', pos)) != std::wstring::npos)
    {
        count++;
        pos++;
    }
    return count;
}

// Line iterator: call with offset=0 to start, returns false at end of text
static bool NextLine(const std::wstring &text, size_t &offset, const wchar_t *&out, int &outLen)
{
    if(offset > text.size())
        return false;
    if(offset == text.size())
    {
        out    = nullptr;
        outLen = 0;
        offset++;
        return true;
    }

    size_t eol = text.find(L'\n', offset);
    size_t len = (eol == std::wstring::npos) ? text.size() - offset : eol - offset;
    if(len > 0 && text[offset + len - 1] == L'\r')
        len--;

    out    = text.c_str() + offset;
    outLen = (int)len;
    offset = (eol == std::wstring::npos) ? text.size() + 1 : eol + 1;
    return true;
}

bool Printer::Print(HWND hwndOwner, const std::wstring &text, const std::wstring &fileName, HFONT hFont)
{
    PRINTDLGW pd   = {};
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner   = hwndOwner;
    pd.Flags       = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOSELECTION;

    if(!PrintDlgW(&pd))
    {
        if(pd.hDevMode)
            GlobalFree(pd.hDevMode);
        if(pd.hDevNames)
            GlobalFree(pd.hDevNames);
        return false;
    }

    HDC hdc = pd.hDC;
    if(!hdc)
        return false;

    int pageWidth  = GetDeviceCaps(hdc, HORZRES);
    int pageHeight = GetDeviceCaps(hdc, VERTRES);
    int dpiX       = GetDeviceCaps(hdc, LOGPIXELSX);
    int dpiY       = GetDeviceCaps(hdc, LOGPIXELSY);

    int marginX     = dpiX;
    int marginY     = dpiY;
    int printWidth  = pageWidth - 2 * marginX;
    int printHeight = pageHeight - 2 * marginY - dpiY;

    HFONT hPrintFont = nullptr;
    if(hFont)
    {
        LOGFONTW lf;
        GetObjectW(hFont, sizeof(lf), &lf);
        // Scale font from screen DPI to printer DPI
        HDC hScreenDC = GetDC(nullptr);
        int screenDpi = hScreenDC ? GetDeviceCaps(hScreenDC, LOGPIXELSY) : 96;
        if(hScreenDC)
            ReleaseDC(nullptr, hScreenDC);
        lf.lfHeight = MulDiv(lf.lfHeight, dpiY, screenDpi);
        hPrintFont  = CreateFontIndirectW(&lf);
    }

    HFONT hOldFont = hPrintFont ? (HFONT)SelectObject(hdc, hPrintFont) : nullptr;

    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight   = tm.tmHeight + tm.tmExternalLeading;
    int linesPerPage = printHeight / lineHeight;
    if(linesPerPage < 1)
        linesPerPage = 1;

    int totalLines = (int)CountLines(text);
    int totalPages = (totalLines + linesPerPage - 1) / linesPerPage;
    if(totalPages < 1)
        totalPages = 1;

    // Extract just filename using pointer arithmetic
    const wchar_t *docName = fileName.c_str();
    size_t lastSlash       = fileName.find_last_of(L"\\/");
    if(lastSlash != std::wstring::npos)
        docName = fileName.c_str() + lastSlash + 1;
    int docNameLen = (int)wcslen(docName);

    DOCINFOW di    = {};
    di.cbSize      = sizeof(di);
    di.lpszDocName = docName;

    if(StartDocW(hdc, &di) <= 0)
    {
        if(hPrintFont)
        {
            SelectObject(hdc, hOldFont);
            DeleteObject(hPrintFont);
        }
        DeleteDC(hdc);
        return false;
    }

    int lineIndex     = 0;
    size_t textOffset = 0;
    for(int page = 0; page < totalPages; page++)
    {
        StartPage(hdc);

        if(hPrintFont)
            SelectObject(hdc, hPrintFont);

        SetTextAlign(hdc, TA_LEFT | TA_TOP);
        TextOutW(hdc, marginX, marginY / 2, docName, docNameLen);

        wchar_t footer[64];
        int footerLen = swprintf_s(footer, L"Page %d of %d", page + 1, totalPages);
        SetTextAlign(hdc, TA_CENTER | TA_TOP);
        TextOutW(hdc, pageWidth / 2, pageHeight - marginY / 2, footer, footerLen);

        SetTextAlign(hdc, TA_LEFT | TA_TOP);
        int y = marginY;
        for(int i = 0; i < linesPerPage && lineIndex < totalLines; i++, lineIndex++)
        {
            const wchar_t *linePtr;
            int lineLen;
            if(NextLine(text, textOffset, linePtr, lineLen) && lineLen > 0)
            {
                RECT rc = {marginX, y, marginX + printWidth, y + lineHeight};
                DrawTextW(hdc, linePtr, lineLen, &rc, DT_LEFT | DT_NOCLIP | DT_NOPREFIX | DT_SINGLELINE);
            }
            y += lineHeight;
        }

        EndPage(hdc);
    }

    EndDoc(hdc);

    if(hPrintFont)
    {
        SelectObject(hdc, hOldFont);
        DeleteObject(hPrintFont);
    }
    DeleteDC(hdc);
    if(pd.hDevMode)
        GlobalFree(pd.hDevMode);
    if(pd.hDevNames)
        GlobalFree(pd.hDevNames);

    return true;
}
