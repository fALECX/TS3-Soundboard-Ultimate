# RP Soundboard Ultimate

Native TeamSpeak 3 soundboard plugin for Windows.

## What This Ships

This repository now ships as a TeamSpeak 3 plugin package, not an Electron desktop app.

- Native plugin source in `src/`
- TeamSpeak SDK headers in `pluginsdk/include`
- CMake build and packaging flow in `CMakeLists.txt`
- Final installer artifact as a `.ts3_plugin` file for TeamSpeak's built-in package wizard

Legacy Electron source files remain in the tree for reference only. They are not part of the release path.

## Requirements

- Windows 10/11
- TeamSpeak 3 client
- CMake 3.21+
- A C++17 compiler toolchain for Windows
- Qt 5 or Qt 6 with `Core`, `Widgets`, and `Network`

## Build And Package

```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DRPSU_MAKE_PLUGIN_FILE=ON
cmake --build build --config Release
cmake --install build --config Release --prefix build/install
```

Outputs:
- Plugin DLL: `build/Release/rp_soundboard_ultimate_win64.dll`
- TeamSpeak installer package: `release/rp_soundboard_ultimate_<version>.ts3_plugin`

## Install

1. Double-click the `.ts3_plugin` file.
2. Or drag it onto `package_inst.exe` inside your TeamSpeak 3 installation folder.
3. Complete TeamSpeak's package installer wizard.

## Runtime

- Open the plugin from TeamSpeak's plugin menu or configure action.
- The plugin opens its own TeamSpeak-owned Qt window.
- Audio playback is injected directly into captured voice.
- Library and board state are stored in the user's application data folder and migrated on first launch from legacy files when present.

## Starter Sounds

Bundled starter assets live in `sounds/`:
- `cigarette-light.wav`
- `cigarette-inhale.wav`
- `bone-crack.wav`
- `handcuffs-click.wav`
- `medkit-open.wav`
- `police-radio-chatter.wav`
- `cash-count.wav`
- `car-lock-beep.wav`

## Verification

```bash
npm test
npm run validate:release
```

The release validation script checks that:
- `package.json` matches the CMake project version
- Electron release metadata is no longer present
- the repo is still aligned to the TS3 plugin package flow

## Legal, Licensing, and Content Policy

- This repository does **not** ship third-party sound samples by default.
- You are responsible for ensuring you have the legal right to download, store, and play any audio.

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
- Explicitly licensed CC or royalty-free audio with redistribution rights

Avoid committing:
- Commercial copyrighted clips without permission
- Content with unclear or missing license terms
