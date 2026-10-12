# Color Log for BO3 Mod Tools

Color Log is an MTPlugins plugin for the Black Ops III Mod Tools Launcher. It formats native `^0`-`^7` colors and adds a fixed diagnostic navigator to the upper-right corner of the lower linker/output pane.

## Version 1.0.1

- Counts every visibly yellow line as a warning and every visibly red line as an error.
- Keeps bold warning/error counters fixed inside the lower output pane, with glowing PNG status icons.
- Cycles through all matching lines when a counter is clicked and smoothly scrolls to the selected diagnostic.
- Flashes the complete selected row twice with a subtle yellow or red full-width highlight.
- Resets the Qt insertion format so later linker output cannot accidentally inherit a previous warning/error color.
- Preserves `ColorLog.ini` during MTPlugins updates.

## Requirements

- Black Ops III Mod Tools
- [MTPlugins](https://github.com/Aidan601/MTPlugins) with Launcher plugin support
- Windows x64

## Building

Build `ColorLog.vcxproj` as `Release | x64` with Visual Studio 2026 (`v145`). The index build output is:

```text
build\Release\ColorLog.dll
```

The parser test project can be built and run separately:

```powershell
msbuild ColorLogParserTests.vcxproj /p:Configuration=Release /p:Platform=x64
.\build\Release\ColorLogParserTests.exe
```

Runtime UI behavior requires the actual Mod Tools Launcher and is tested manually.

## Installation

Install through the MTPlugins **Browse plugins** window after version 1.0.1 is published. For local testing, place `ColorLog.dll` and `ColorLog.ini` in `<BO3>\bin\plugins\ColorLog\`.

## License

GPL-3.0.
