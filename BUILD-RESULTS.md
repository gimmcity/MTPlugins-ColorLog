# Build results — Color Log 1.0.1

- Configuration: `Release | x64`
- Toolset: Visual Studio 2026 / MSVC `v145`
- Plugin project: `ColorLog.vcxproj`
- Test project: `ColorLogParserTests.vcxproj`
- Plugin output: `build/Release/ColorLog.dll`
- DLL SHA-256: `4E1724B2BE2A55000831D018060D00E5467965990EBCCFEAB839B881C68BC71A`
- Verified: PE x64, exported `ModToolsPluginEntry`, file/product version `1.0.1`
- Parser tests: passed, including diagnostics, positions, native `^0`-`^7`, split chunks, and the 250,000-line stress case
- Runtime UI validation: manual test in the BO3 Mod Tools Launcher is still required.
