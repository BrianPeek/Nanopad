#include "file_io.h"
#include <commdlg.h>
#include <memory>

bool FileIO::ShowOpenDialog(HWND hwndOwner, std::wstring &outPath)
{
    wchar_t fileName[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hwndOwner;
    ofn.lpstrFilter   = L"Text Files (*.txt)\0*.txt\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = fileName;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = L"txt";

    if(!GetOpenFileNameW(&ofn))
        return false;

    outPath = fileName;
    return true;
}

bool FileIO::ShowSaveDialog(HWND hwndOwner, std::wstring &outPath)
{
    wchar_t fileName[MAX_PATH] = {};

    OPENFILENAMEW ofn = {};
    ofn.lStructSize   = sizeof(ofn);
    ofn.hwndOwner     = hwndOwner;
    ofn.lpstrFilter   = L"Text Files (*.txt)\0*.txt\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = fileName;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";

    if(!GetSaveFileNameW(&ofn))
        return false;

    outPath = fileName;
    return true;
}

bool FileIO::IsValidUTF8(const uint8_t *data, size_t size)
{
    for(size_t i = 0; i < size;)
    {
        if(data[i] < 0x80)
        {
            i++;
        }
        else if((data[i] & 0xE0) == 0xC0)
        {
            if(i + 1 >= size || (data[i + 1] & 0xC0) != 0x80)
                return false;
            i += 2;
        }
        else if((data[i] & 0xF0) == 0xE0)
        {
            if(i + 2 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
                return false;
            i += 3;
        }
        else if((data[i] & 0xF8) == 0xF0)
        {
            if(i + 3 >= size || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 ||
               (data[i + 3] & 0xC0) != 0x80)
                return false;
            i += 4;
        }
        else
        {
            return false;
        }
    }
    return true;
}

Encoding FileIO::DetectEncoding(const uint8_t *data, size_t size)
{
    if(size == 0)
        return Encoding::UTF8;

    // Check BOM
    if(size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
        return Encoding::UTF8_BOM;
    if(size >= 2 && data[0] == 0xFF && data[1] == 0xFE)
        return Encoding::UTF16_LE;
    if(size >= 2 && data[0] == 0xFE && data[1] == 0xFF)
        return Encoding::UTF16_BE;

    // Heuristic: check if valid UTF-8 with multibyte sequences
    if(IsValidUTF8(data, size))
        return Encoding::UTF8;

    return Encoding::ANSI;
}

LineEnding FileIO::DetectLineEnding(const wchar_t *text, size_t len)
{
    bool hasCRLF = false, hasLF = false, hasCR = false;

    for(size_t i = 0; i < len; i++)
    {
        if(text[i] == L'\r')
        {
            if(i + 1 < len && text[i + 1] == L'\n')
            {
                hasCRLF = true;
                i++; // skip the LF
            }
            else
            {
                hasCR = true;
            }
        }
        else if(text[i] == L'\n')
        {
            hasLF = true;
        }
    }

    int count = (hasCRLF ? 1 : 0) + (hasLF ? 1 : 0) + (hasCR ? 1 : 0);
    if(count > 1)
        return LineEnding::Mixed;
    if(hasLF)
        return LineEnding::LF;
    if(hasCR)
        return LineEnding::CR;
    return LineEnding::CRLF;
}

bool FileIO::ReadFile(const std::wstring &path, std::wstring &outText, FileInfo &info)
{
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
    if(hFile == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER fileSize;
    if(!GetFileSizeEx(hFile, &fileSize))
    {
        CloseHandle(hFile);
        return false;
    }

    size_t size = (size_t)fileSize.QuadPart;

    // Guard against files too large for int-based Win32 APIs
    if(fileSize.QuadPart > 0x7FFFFFFF)
    {
        CloseHandle(hFile);
        return false;
    }

    if(size == 0)
    {
        CloseHandle(hFile);
        outText.clear();
        info.filePath   = path;
        info.encoding   = Encoding::UTF8;
        info.lineEnding = LineEnding::CRLF;
        return true;
    }

    // For large files, keep the memory-mapped view open and process directly
    // from it, avoiding an intermediate buffer copy.
    const uint8_t *rawData = nullptr;
    std::unique_ptr<uint8_t[]> smallBuffer;
    HANDLE hMapping   = nullptr;
    const void *pView = nullptr;

    if(size > 1024 * 1024)
    {
        hMapping = CreateFileMappingW(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if(!hMapping)
        {
            CloseHandle(hFile);
            return false;
        }

        pView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if(!pView)
        {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            return false;
        }

        rawData = (const uint8_t *)pView;
    }
    else
    {
        smallBuffer = std::make_unique<uint8_t[]>(size);
        DWORD bytesRead;
        if(!::ReadFile(hFile, smallBuffer.get(), (DWORD)size, &bytesRead, nullptr))
        {
            CloseHandle(hFile);
            return false;
        }
        rawData = smallBuffer.get();
    }

    CloseHandle(hFile);

    // Detect encoding
    Encoding enc = DetectEncoding(rawData, size);

    const uint8_t *textData = rawData;
    size_t textSize         = size;

    switch(enc)
    {
        case Encoding::UTF8_BOM:
            textData += 3;
            textSize -= 3;
            [[fallthrough]];
        case Encoding::UTF8:
        {
            if(textSize == 0)
            {
                outText.clear();
                break;
            }
            int wlen = MultiByteToWideChar(CP_UTF8, 0, (const char *)textData, (int)textSize, nullptr, 0);
            outText.resize(wlen);
            MultiByteToWideChar(CP_UTF8, 0, (const char *)textData, (int)textSize, outText.data(), wlen);
            break;
        }
        case Encoding::UTF16_LE:
        {
            textData += 2;
            textSize -= 2;
            outText.assign((const wchar_t *)textData, textSize / sizeof(wchar_t));
            break;
        }
        case Encoding::UTF16_BE:
        {
            textData += 2;
            textSize -= 2;
            size_t wlen = textSize / sizeof(wchar_t);
            outText.resize(wlen);
            for(size_t i = 0; i < wlen; i++)
            {
                uint8_t hi = textData[i * 2];
                uint8_t lo = textData[i * 2 + 1];
                outText[i] = (wchar_t)(hi << 8 | lo);
            }
            break;
        }
        case Encoding::ANSI:
        {
            if(textSize == 0)
            {
                outText.clear();
                break;
            }
            int wlen = MultiByteToWideChar(CP_ACP, 0, (const char *)textData, (int)textSize, nullptr, 0);
            outText.resize(wlen);
            MultiByteToWideChar(CP_ACP, 0, (const char *)textData, (int)textSize, outText.data(), wlen);
            break;
        }
    }

    // Release memory-mapped view now that conversion is done
    if(pView)
        UnmapViewOfFile(pView);
    if(hMapping)
        CloseHandle(hMapping);
    smallBuffer.reset();

    LineEnding le = DetectLineEnding(outText.c_str(), outText.size());

    // The EDIT control expects \r\n line endings. Convert if necessary.
    // Use bulk segment copies instead of per-character append.
    if(le == LineEnding::LF || le == LineEnding::CR)
    {
        // Count line endings to pre-allocate exactly
        size_t extraChars = 0;
        for(size_t i = 0; i < outText.size(); i++)
        {
            if(outText[i] == L'\r')
            {
                if(i + 1 < outText.size() && outText[i + 1] == L'\n')
                    i++; // already CRLF
                else
                    extraChars++; // bare CR -> CRLF
            }
            else if(outText[i] == L'\n')
            {
                extraChars++; // bare LF -> CRLF
            }
        }

        std::wstring normalized;
        normalized.resize(outText.size() + extraChars);
        size_t dst      = 0;
        size_t segStart = 0;

        for(size_t i = 0; i < outText.size(); i++)
        {
            if(outText[i] == L'\r')
            {
                if(i + 1 < outText.size() && outText[i + 1] == L'\n')
                {
                    i++; // already CRLF, include both
                    continue;
                }
                // Bare CR: copy segment up to and including CR, then insert LF
                size_t len = i - segStart + 1;
                wmemcpy(normalized.data() + dst, outText.data() + segStart, len);
                dst += len;
                normalized[dst++] = L'\n';
                segStart          = i + 1;
            }
            else if(outText[i] == L'\n')
            {
                // Bare LF: copy segment before LF, insert CRLF
                size_t len = i - segStart;
                if(len > 0)
                {
                    wmemcpy(normalized.data() + dst, outText.data() + segStart, len);
                    dst += len;
                }
                normalized[dst++] = L'\r';
                normalized[dst++] = L'\n';
                segStart          = i + 1;
            }
        }
        // Copy remaining segment
        if(segStart < outText.size())
        {
            wmemcpy(normalized.data() + dst, outText.data() + segStart, outText.size() - segStart);
            dst += outText.size() - segStart;
        }

        outText = std::move(normalized);
    }

    info.filePath   = path;
    info.encoding   = enc;
    info.lineEnding = le;

    return true;
}

bool FileIO::WriteFile(const std::wstring &path, const std::wstring &text, Encoding enc, LineEnding le)
{
    // For LF/CR targets, strip \r\n to \n or \r in-place on a single copy.
    // For CRLF (or Mixed), pass through directly — no copy needed.
    const std::wstring *pText = &text;
    std::wstring converted;

    if(le == LineEnding::LF || le == LineEnding::CR)
    {
        wchar_t replacement = (le == LineEnding::LF) ? L'\n' : L'\r';
        // Shrinking: output ≤ input size, pre-size to input and track length
        converted.resize(text.size());
        size_t dst = 0, segStart = 0;
        for(size_t i = 0; i < text.size(); i++)
        {
            if(text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n')
            {
                // Copy segment before \r\n, then replacement char
                size_t segLen = i - segStart;
                if(segLen > 0)
                {
                    wmemcpy(converted.data() + dst, text.data() + segStart, segLen);
                    dst += segLen;
                }
                converted[dst++] = replacement;
                i++;
                segStart = i + 1;
            }
        }
        if(segStart < text.size())
        {
            wmemcpy(converted.data() + dst, text.data() + segStart, text.size() - segStart);
            dst += text.size() - segStart;
        }
        converted.resize(dst);
        pText = &converted;
    }

    const std::wstring &outText = *pText;

    // Convert to target encoding — compute size first, allocate once
    size_t outputSize = 0;
    size_t bomSize    = 0;

    switch(enc)
    {
        case Encoding::UTF8_BOM:
            bomSize = 3;
            [[fallthrough]];
        case Encoding::UTF8:
            outputSize = bomSize + WideCharToMultiByte(CP_UTF8, 0, outText.c_str(), (int)outText.size(), nullptr, 0,
                                                       nullptr, nullptr);
            break;
        case Encoding::UTF16_LE:
            outputSize = 2 + outText.size() * sizeof(wchar_t);
            break;
        case Encoding::UTF16_BE:
            outputSize = 2 + outText.size() * sizeof(wchar_t);
            break;
        case Encoding::ANSI:
            outputSize =
                WideCharToMultiByte(CP_ACP, 0, outText.c_str(), (int)outText.size(), nullptr, 0, nullptr, nullptr);
            break;
    }

    auto output   = std::make_unique<uint8_t[]>(outputSize);
    size_t offset = 0;

    switch(enc)
    {
        case Encoding::UTF8_BOM:
            output[0] = 0xEF;
            output[1] = 0xBB;
            output[2] = 0xBF;
            offset    = 3;
            [[fallthrough]];
        case Encoding::UTF8:
            WideCharToMultiByte(CP_UTF8, 0, outText.c_str(), (int)outText.size(), (char *)output.get() + offset,
                                (int)(outputSize - offset), nullptr, nullptr);
            break;
        case Encoding::UTF16_LE:
            output[0] = 0xFF;
            output[1] = 0xFE;
            memcpy(output.get() + 2, outText.c_str(), outText.size() * sizeof(wchar_t));
            break;
        case Encoding::UTF16_BE:
            output[0] = 0xFE;
            output[1] = 0xFF;
            for(size_t i = 0; i < outText.size(); i++)
            {
                output[2 + i * 2]     = (uint8_t)(outText[i] >> 8);
                output[2 + i * 2 + 1] = (uint8_t)(outText[i] & 0xFF);
            }
            break;
        case Encoding::ANSI:
            WideCharToMultiByte(CP_ACP, 0, outText.c_str(), (int)outText.size(), (char *)output.get(), (int)outputSize,
                                nullptr, nullptr);
            break;
    }

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written;
    BOOL ok = ::WriteFile(hFile, output.get(), (DWORD)outputSize, &written, nullptr);
    CloseHandle(hFile);

    return ok && written == (DWORD)outputSize;
}

const wchar_t *FileIO::EncodingToString(Encoding enc)
{
    switch(enc)
    {
        case Encoding::ANSI:
            return L"ANSI";
        case Encoding::UTF8:
            return L"UTF-8";
        case Encoding::UTF8_BOM:
            return L"UTF-8 with BOM";
        case Encoding::UTF16_LE:
            return L"UTF-16 LE";
        case Encoding::UTF16_BE:
            return L"UTF-16 BE";
    }
    return L"Unknown";
}

const wchar_t *FileIO::LineEndingToString(LineEnding le)
{
    switch(le)
    {
        case LineEnding::CRLF:
            return L"CRLF";
        case LineEnding::LF:
            return L"LF";
        case LineEnding::CR:
            return L"CR";
        case LineEnding::Mixed:
            return L"Mixed";
    }
    return L"Unknown";
}
