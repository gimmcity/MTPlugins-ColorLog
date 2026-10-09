# Build and test results

Validated on Windows x64 with Visual Studio 2026 Community, MSVC 14.51 (`v145`), and Windows SDK 10.0.26100.0.

- Configuration: `Release | x64`
- Compiler warnings: 0
- Compiler errors: 0
- Parser tests: passed
- Stress test: 250,000 lines passed
- Export check: `ModToolsPluginEntry` present
- Runtime dependencies: `USER32.dll`, `KERNEL32.dll`
- MSVC runtime: statically linked (`/MT`)

Release file hashes:

```text
ColorLog.dll  1603207c4ca04d16945fb9f482bd99e8ed86e9f4670a26d8948bb8a207cbad1c
ColorLog.ini  7ad645521c27789b117b57e862e2d6c797207487d1d78de36916aa0201e3e127
```

The parser test output was:

```text
logcolorparser: diagnostics, native ^0-^7, split chunks and 250000-line stress pass
```

The runtime console integration was previously verified inside the BO3 Mod Tools Launcher. Parser tests do not launch BO3 and therefore cannot independently exercise the live Qt widget.
