# RP Soundboard Ultimate

Native TeamSpeak 3 soundboard plugin for Windows. The new runtime target is a TS3 client plugin with direct captured-voice injection instead of the previous Electron plus virtual-cable workflow.

## Status

This repository now contains a native plugin source tree:
- `CMakeLists.txt` for the plugin build
- `pluginsdk/include` with vendored TeamSpeak SDK headers
- `src/` with plugin exports, JSON storage, migration logic, Qt UI shell, and captured-voice mixing

The old Electron files are still present as migration/reference material, but the plugin is the active target.

## Requirements

- Windows 10/11
- TeamSpeak 3 client
- CMake 3.21+
- MSVC toolchain
- Qt 5 or Qt 6 with `Core`, `Widgets`, and `Network`

## Default Starter Sounds

Bundled starter assets currently remain in `sounds/`:
- `cigarette-light.wav`
- `cigarette-inhale.wav`
- `bone-crack.wav`
- `handcuffs-click.wav`
- `medkit-open.wav`
- `police-radio-chatter.wav`
- `cash-count.wav`
- `car-lock-beep.wav`

Automatic first-run board assignment has not been ported yet.

## Build

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Expected output: `build/Release/rp_soundboard_ultimate_win64.dll`

## Implemented So Far

- TS3 plugin exports and autoload registration
- Plugin configure/menu entry that opens a Qt soundboard window
- JSON storage in `%APPDATA%/.../RP Soundboard Ultimate`
- First-run migration of legacy Electron `library.json`, `boards-config.json`, `app-config.json`, and `sounds/`
- Local sound import
- Board and library rendering in the Qt window
- Plugin hotkey export from stored board/cell hotkeys
- Direct audio injection through `ts3plugin_onEditCapturedVoiceDataEvent`
- WAV decoding and mixing path for local playback

## Current Gaps

- The UI shell is not yet feature-complete relative to the old Electron app.
- YouTube and Freesound native service wrappers are not wired in yet.
- The decoder currently supports PCM WAV files only.
- The plugin opens a TeamSpeak-owned Qt window rather than a true docked TS3 panel.

## Notes

- TeamSpeak plugin APIs do not provide the same UI surface as a full desktop app, so the plugin currently uses a TeamSpeak-owned Qt window.
- Legacy Electron code remains in the repo to preserve behavior references during migration.

## YouTube / Freesound Notes

- YouTube features require `yt-dlp` available either:
  - bundled as `resources/yt-dlp.exe` (for packaged app), or
  - installed on system PATH (fallback).
- Freesound features require your own API key from:
  - https://freesound.org/apiv2/apply

## Legal, Licensing, and Content Policy

- This repository does **not** ship third-party sound samples by default.
- You are responsible for ensuring you have the legal right to download, store, and play any audio.
- Respect YouTube Terms of Service and creator rights.
- Respect Freesound licenses and attribution requirements where applicable.

### Trademark Disclaimer

- **Grand Theft Auto** and **GTA** are trademarks of Take-Two Interactive / Rockstar Games.
- **TeamSpeak** is a trademark of TeamSpeak Systems GmbH.
- This project is unofficial and not affiliated with, endorsed by, or sponsored by those trademark owners.

## Open Source Docs

- License: [LICENSE](./LICENSE)
- Contributing: [CONTRIBUTING.md](./CONTRIBUTING.md)
- Code of Conduct: [CODE_OF_CONDUCT.md](./CODE_OF_CONDUCT.md)
- Security Policy: [SECURITY.md](./SECURITY.md)
- Changelog: [CHANGELOG.md](./CHANGELOG.md)
- Release Template: [RELEASE_TEMPLATE.md](./RELEASE_TEMPLATE.md)

## Accepted Audio Source Guidance

Recommended for repository examples and docs:
- Original recordings you created
- Public domain audio
- Explicitly licensed CC/royalty-free audio with redistribution rights

Avoid committing:
- Commercial copyrighted clips without permission
- Content with unclear or missing license terms

## Troubleshooting

### `yt-dlp not found`

- Put `yt-dlp.exe` in `resources/` before building installer, or
- Install `yt-dlp` globally and ensure it is on PATH.

### Freesound search returns no results

- Confirm API key is set in settings.
- Verify key validity on Freesound dashboard.

### No audio in TeamSpeak

- Re-check playback/capture device pairing.
- Ensure virtual cable is selected correctly in both apps.
