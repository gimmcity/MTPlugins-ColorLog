# Color Log for BO3 Mod Tools

Color Log is an MTPlugins plugin for the Black Ops III Mod Tools Launcher. It adds native Call of Duty inline colors to launcher build output while preserving the existing launcher console and build workflow.

## Features

- Interprets inline `^0` through `^7` color codes and removes the control sequences from visible output.
- Supports multiple color changes inside one line.
- Colors recognized `WARNING` diagnostics yellow.
- Colors `ERROR`, `FATAL ERROR`, `UNRECOVERABLE ERROR`, and `SCRIPT ERROR` diagnostics red.
- Colors genuine `done` and `Build succeeded` status lines green.
- Repairs diagnostic prefixes split by launcher output chunks, such as `W` + `ARNING` and `^` + `1ERROR`.
- Keeps ordinary output in the launcher's original color.
- Adds `(Color Log)` to the launcher window title while active.

Explicit inline color codes have priority over automatic diagnostic classification. Output is inserted and formatted through the launcher's existing `QPlainTextEdit`, `QTextCursor`, and `QTextCharFormat` objects; compiler output is never interpreted as HTML.

## Native colors

| Code | Color |
| --- | --- |
| `^0` | Black |
| `^1` | Red |
| `^2` | Green |
| `^3` | Yellow |
| `^4` | Blue |
| `^5` | Cyan |
| `^6` | Magenta |
| `^7` | Default launcher foreground |

Example input:

```text
^1ERROR:^7 Failed to compile ^3zm_bzm
```

The raw `^1`, `^7`, and `^3` sequences are removed. Each remaining span is rendered in its selected color.

## Requirements

- Black Ops III Mod Tools
- [MTPlugins](https://github.com/Aidan601/MTPlugins) with Launcher plugin support
- Windows x64

Color Log runs only inside the Mod Tools Launcher process. It does not modify the game executable, contact Steam, or load into multiplayer. No software can promise account-enforcement outcomes, but this plugin is scoped to the official mod-tools launcher rather than the game.

## Installation for local testing

Create this folder under the Black Ops III installation:

```text
<BO3>\bin\plugins\ColorLog\
```

Copy these files into it:

```text
ColorLog.dll
ColorLog.ini
```

Start the Mod Tools Launcher. Open the MTPlugins window to confirm that **Color Log 1.0.0 [Launcher]** is enabled. The launcher title should contain `(Color Log)`.

When published through MTPlugins-Index, users should install it from **Browse plugins** instead of copying files manually.

## Configuration

`ColorLog.ini` contains:

```ini
[logcolors]
enabled=1
```

Set `enabled=0` to keep stock console formatting without uninstalling the plugin.

## Commands

The MTPlugins command interface supports:

- `logcolors status` — reports whether the formatter is attached.
- `logcolors test` — appends normal, warning, error, success, and done test lines.

## Building

Open `ColorLog.sln` in Visual Studio 2026 with the Desktop development with C++ workload, the `v145` toolset, and a Windows 10/11 SDK installed. Select `Release | x64` and choose **Build Solution**.

Alternatively, run:

```bat
build.bat
```

The build script locates the newest installed MSBuild, builds both projects, and runs the parser tests. Output:

```text
build\Release\ColorLog.dll
build\Release\ColorLogParserTests.exe
```

The project uses the static MSVC runtime (`/MT`) so the plugin does not require a matching redistributable DLL beside it. The Qt libraries are supplied by the BO3 Mod Tools Launcher at runtime.

## Tests

`ColorLogParserTests` covers:

- ordinary output and false-positive resistance;
- warning, error, unrecoverable error, script error, success, and done classification;
- inline `^0`-`^7` parsing;
- literal and unsupported caret sequences;
- split output chunks;
- a 250,000-line parser stress loop.

Runtime rendering still requires the actual Mod Tools Launcher because the Qt widget and exports exist only inside that process.

## Publishing through MTPlugins-Index

Create a GitHub release and attach `ColorLog.dll` and `ColorLog.ini`. Then add a matching entry to [MTPlugins-Index](https://github.com/Aidan601/MTPlugins-Index). The index entry must use `name: ColorLog`, `apps: ["launcher"]`, `apiVersion: 1`, the same `1.0.0` version reported by the DLL, and SHA-256 hashes for both files. Mark the INI with `"config": true` so updates preserve user settings.

Never replace assets belonging to an already indexed release. Increase both the DLL version and index version when publishing an update.

## Architecture

- `src/ColorLog.cpp` implements the MTPlugins entry point and lifecycle callbacks.
- `src/logcolors.cpp` locates the existing launcher `QPlainTextEdit`, removes inline tokens, repairs known chunk splits, and applies `QTextCharFormat` foreground colors.
- `src/logcolorparser.cpp` contains the independent parser and conservative diagnostic classifier.
- `sdk/` contains the MTPlugins API headers used to build the plugin.
- `tests/` contains focused parser tests without a BO3 or Qt dependency.

Raw build logs written by external tools are not changed; only the visible launcher document is formatted.

## License

GPL-3.0. See `LICENSE` and `NOTICE.md`.
