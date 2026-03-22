# Agent Instructions

## Project Overview
Nanopad is a portable Win32 text editor written in C++ using the Win32 API directly — no frameworks, no MFC, no ATL.

## Code Style
- Use `.clang-format` in the repo root. Always run clang-format on modified source files after changes.
- Do not reorder `#includes` — Windows headers are order-dependent.
- Only comment non-obvious code. Do not comment the obvious.

## Build
- Visual Studio 2022 or later, v143+ toolset, x64 Debug/Release.
- Build from repo root: `msbuild Nanopad.sln /p:Configuration=Release /p:Platform=x64`
- Always verify a clean build after code changes.

## Architecture
- Source files live flat in `src/`, project files in `src/`, solution at root.
- All app settings stored in `nanopad.ini` next to the exe (portable, no registry for app settings).
- Version info lives in `src/version.h` — CI stamps it from git tags. Never manually bump the version.
- Dark mode uses undocumented Windows UAH messages (0x0091, 0x0092) and uxtheme ordinal APIs.

## Coding Practices
- Cache GDI objects (brushes, fonts, pens) — never create/destroy in paint handlers.
- Use `std::make_unique` for dynamic allocations, not raw `new`/`delete`.
- Prefer stack buffers for known-bounded sizes over `std::wstring`.
- All `LoadLibrary` calls must use full system paths to prevent DLL hijacking.
- All fixed-size buffers must have explicit bounds checks before writes.
- Keep the README updated when adding or changing features.

## Testing
- No automated test suite — verify manually after changes.
- Test dark/light mode switching, DPI changes, file encoding round-trips, and large files.
