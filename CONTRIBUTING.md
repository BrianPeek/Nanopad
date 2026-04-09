# Contributing to Nanopad

Thanks for your interest in contributing to Nanopad!

## Getting Started

1. Fork the repository
2. Clone your fork
3. Open `Nanopad.sln` in Visual Studio 2022 or later (or build from command line: `msbuild Nanopad.sln /p:Configuration=Release /p:Platform=x64`)
4. Build and run

## Code Style

This project uses `.clang-format` for consistent formatting. Please format your code before submitting:

- In Visual Studio: **Ctrl+K, Ctrl+D**
- From command line: `clang-format -i -style=file src/*.cpp src/*.h`

## Pull Requests

- Keep PRs focused — one feature or fix per PR
- Ensure a clean Release build with no warnings
- Update `README.md` if your change adds or modifies features
- Update `CHANGELOG.md` for user-facing changes by adding entries under `Unreleased`
- Test dark mode, light mode, and theme switching
- Test with both small and large files

## AI-Generated Contributions

AI-assisted contributions are welcome. See [AI_POLICY.md](AI_POLICY.md) for details.

## Reporting Issues

- Use GitHub Issues
- Include your Windows version and build number
- Include steps to reproduce
- Attach screenshots for visual issues
