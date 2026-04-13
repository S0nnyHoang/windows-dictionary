# EnViDict

A lightweight English→Vietnamese popup dictionary for Windows. It sits in
the tray, watches for text selections anywhere on the desktop, and shows a
small "D" bubble next to the selection. Click the bubble and a tooltip
appears with the Vietnamese meaning — similar to macOS's built-in
"Look Up" feature.

## Goals

- **Lightweight.** Native C++ + Win32 only. Static CRT, no .NET, no Electron,
  no Qt. Target: < 20 MB private bytes at idle, near-zero CPU.
- **Battery-friendly.** Strictly event-driven: UI Automation selection events
  and (optionally) a global mouse hook. No polling, no idle timers.
- **Offline.** Dictionary data is bundled as a single SQLite file next to the
  executable. No network calls.
- **Unobtrusive.** The bubble and tooltip are non-activating top-most layered
  windows — they never steal focus from the app you're working in.

## How it triggers a lookup

Windows, unlike macOS, does **not** let third-party apps inject items into
the native right-click "Look Up" menu of arbitrary applications. The closest
UX that works in every app is the iOS/macOS-style **floating bubble**:

1. You select a word in any app.
2. A 22 px "D" bubble appears next to the selection.
3. You click it → a tooltip opens with the Vietnamese meaning and a speaker
   button for pronunciation.
4. The bubble auto-hides after a few seconds if ignored.

### Selection-detection strategies

The watcher uses **UI Automation** (`IUIAutomationTextPattern` +
`UIA_Text_TextSelectionChangedEventId`) as the primary mechanism. It is pure
event-driven: UIA wakes us up only when a selection changes, so we consume no
CPU between interactions.

Some apps do not expose TextPattern:

| App | Primary path? | Notes |
|---|---|---|
| Microsoft Word, WordPad, Sumatra PDF | UIA | Reliable |
| Chrome / Edge / Electron apps | Clipboard fallback | Chromium turns off the accessibility tree by default. Workaround: launch with `--force-renderer-accessibility` |
| VS Code | Clipboard fallback | Same Electron caveat |
| Legacy Notepad | Clipboard fallback | Edit control doesn't raise selection-changed events |

**Clipboard fallback** is opt-in in **Settings → "Use clipboard fallback"**.
When enabled, EnViDict installs a low-level mouse hook. On a drag-select
mouse-up, it snapshots the current clipboard, synthesizes Ctrl+C, reads the
resulting text, and **restores** the original clipboard contents using a
sequence-number guard so your clipboard is never left dirty.

## Building

### Prerequisites
- Windows 10 21H2+ / Windows 11
- Visual Studio 2022 (or the Build Tools + MSVC toolchain)
- CMake ≥ 3.20
- Python 3.10+ (only needed once, to build `dictionary.db`)

### Build
```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

This produces `build\Release\EnViDict.exe` and, via the `dictionary_db` custom
target, `data\dictionary.db`.

### Dictionary data

`tools\build_dictionary.py` is the StarDict → SQLite converter. Without
arguments it uses `tools\seed_entries.tsv` (a tiny built-in seed) so the
project builds end-to-end without network access. To use a real EN→VI pack:

```bat
python tools\build_dictionary.py ^
    --stardict path\to\stardict-english-vietnamese ^
    --output   data\dictionary.db
```

Recommended source: the Open Vietnamese Dictionary Project (OVDP) StarDict
packs at <https://github.com/dynamotn/stardict-vi>. Check the pack's license
and ship `dictionary.LICENSE.txt` next to the binary.

## Running

Double-click `EnViDict.exe`. It installs a tray icon (blue "D") and starts
watching selections. Right-click the tray icon for Settings, Enable toggle,
About, and Quit. You can also pass `--hidden` to suppress any UI noise at
startup — this is what the "Run at Windows startup" option does.

## Resource-budget techniques

- Static CRT (`/MT`), `/OPT:REF /OPT:ICF /LTCG` in Release.
- Lazy D2D/DWrite factories; render targets released in `PopupWindow::Hide`.
- SQLite: `SQLITE_DEFAULT_MEMSTATUS=0`, `SQLITE_DQS=0`,
  `PRAGMA mmap_size = 16 MiB` so DB pages live in shared memory.
- `SetProcessWorkingSetSize(-1, -1)` after init and after hiding the popup.
- Zero polling timers — everything routes through a hidden message-only
  window that blocks in `GetMessageW` when idle.

## Project layout

```
src/                    C++ sources
third_party/sqlite/     SQLite amalgamation
tools/                  Python build scripts (build_dictionary.py)
data/                   Generated dictionary.db (build output)
resources/              Manifest, .rc, icons
cmake/                  Helper modules (StaticRuntime.cmake)
docs/                   Architecture and manual-test notes
```

See `docs/architecture.md` for the full component breakdown.

## License

Source code: MIT (see `LICENSE`).
Dictionary data: follows the upstream StarDict pack's license (e.g. the
OVDP pack's own license). Attribution must be preserved in
`dictionary.LICENSE.txt`.
