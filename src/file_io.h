#pragma once
#include <windows.h>
#include <string>
#include <cstdint>

enum class Encoding
{
    ANSI,
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_BE
};

enum class LineEnding
{
    CRLF,
    LF,
    CR,
    Mixed
};

struct FileInfo
{
    std::wstring filePath;
    Encoding encoding     = Encoding::UTF8;
    LineEnding lineEnding = LineEnding::CRLF;
};

class FileIO
{
  public:
    static bool ShowOpenDialog(HWND hwndOwner, std::wstring &outPath);
    static bool ShowSaveDialog(HWND hwndOwner, std::wstring &outPath);
    static bool ReadFile(const std::wstring &path, std::wstring &outText, FileInfo &info);
    static bool WriteFile(const std::wstring &path, const std::wstring &text, Encoding enc, LineEnding le);

    static const wchar_t *EncodingToString(Encoding enc);
    static const wchar_t *LineEndingToString(LineEnding le);

  private:
    static Encoding DetectEncoding(const uint8_t *data, size_t size);
    static LineEnding DetectLineEnding(const wchar_t *text, size_t len);
    static bool IsValidUTF8(const uint8_t *data, size_t size);
};
