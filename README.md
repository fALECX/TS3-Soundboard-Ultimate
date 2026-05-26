# RP Soundboard Ultimate

Created and maintained by fALECX - twitch.tv/fALECX.

- Twitch: https://twitch.tv/fALECX
- X: https://x.com/fALECX
- Discord: `falecx`

---

# 🇩🇪 Deutsch

RP Soundboard Ultimate ist ein quelloffenes TeamSpeak-3-Soundboard-Plugin für Windows. Es wird als natives TS3-Plugin-Paket ausgeliefert, nicht als separate Electron-App.

## 🇩🇪 Installation

1. Gehe zu [**Releases**](https://github.com/fALECX/TS3-Soundboard-Ultimate/releases/latest) und lade `rp_soundboard_ultimate_*.ts3_plugin` herunter
2. Doppelklick auf die Datei — TeamSpeak öffnet den Plugin-Installer automatisch
3. TeamSpeak neu starten
4. Das Soundboard ist danach unter **Plugins → RP Soundboard Ultimate** im TS3-Menü verfügbar

## 🇩🇪 Funktionen

- Spielt Sounds direkt in die TeamSpeak-Stimme ein
- Öffnet ein Qt-Steuerungsfenster über das TS3-Plugin-Menü
- Importiert lokale Audiodateien in eine persistente Soundbibliothek
- Unterstützt YouTube-Suche und -Download über mitgelieferte Hilfsprogramme
- Speichert Boards, Bibliothek und Konfiguration im AppData-Verzeichnis
- Migriert Daten aus älteren Desktop-Installationen automatisch

## 🇩🇪 Projektstatus

Dieses Repository ist ausschließlich für die Veröffentlichung als natives Plugin strukturiert.

- Behalten: Quellcode des nativen Plugins, TeamSpeak-SDK-Header, Build-/Paket-Skripte, Release-Dokumentation
- Entfernt: erfasste Build-Ausgaben, veraltete Electron-/Web-App-Dateien, mitgelieferte Beispiel-Audiodateien, temporäre Test-Artefakte, eingebettetes Upstream-Checkout

Die Upstream-Referenzimplementierung ist in [docs/upstream-basis.md](docs/upstream-basis.md) dokumentiert.

## 🇩🇪 Voraussetzungen

- Windows 10 oder 11
- TeamSpeak 3 64-Bit-Client
- CMake 3.21+
- Visual Studio 2022 oder eine andere Windows-C++17-Toolchain
- Qt 5 oder Qt 6 mit `Core`, `Widgets` und `Network`

## 🇩🇪 Repository-Struktur

- `src/`: natives Plugin, Laufzeit, Speicherung, Wiedergabe und Qt-UI-Code
- `pluginsdk/include/`: TeamSpeak-3-Plugin-SDK-Header
- `scripts/test-teamspeak.ps1`: lokaler Build, Deployment und TeamSpeak-Smoke-Test
- `scripts/validate-release.js`: Validierung der Release-Metadaten
- `tests/release.test.js`: leichte Release-Checks auf Repository-Ebene
- `sounds/`: ignorierter Platzhalter für Laufzeit-Sound-Speicher
- `resources/`: ignorierter Platzhalter für Hilfsprogramm-Ablage

## 🇩🇪 Build

```powershell
cmake -S . -B build_msvc_qt5 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_msvc_qt5
cmake --install build_msvc_qt5 --prefix build_msvc_qt5/install
```

Für Visual-Studio-Generatoren stattdessen die übliche Form `--config Release` verwenden.

## 🇩🇪 Paketierung

So wird das finale TeamSpeak-Plugin-Paket erzeugt:

```powershell
cmake -S . -B build_msvc_qt5 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DRPSU_MAKE_PLUGIN_FILE=ON
cmake --build build_msvc_qt5
cmake --install build_msvc_qt5 --prefix build_msvc_qt5/install
```

Wichtigste Ausgaben:

- `build_msvc_qt5/install/plugins/rp_soundboard_ultimate_win64.dll`
- `build_msvc_qt5/install/plugins/rp_soundboard_ultimate/rp_soundboard_ultimate_runtime.bin`
- `release/rp_soundboard_ultimate_<version>.ts3_plugin`

## 🇩🇪 In TeamSpeak installieren

1. Erzeuge das `.ts3_plugin`-Paket.
2. Doppelklick darauf oder Drag-and-Drop auf TeamSpeaks `package_inst.exe`.
3. TeamSpeak bei Bedarf neu starten.
4. `Extras -> Optionen -> Addons` öffnen und prüfen, ob `RP Soundboard Ultimate` geladen ist.

## 🇩🇪 Lokale Überprüfung

Repository-Checks:

```powershell
npm test
npm run validate:release
```

Vollständiger TeamSpeak-Smoke-Test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-teamspeak.ps1 -Build -StopTeamSpeak -StartTeamSpeak -BuildDir .\build_msvc_qt5
```

Der Smoke-Test prüft das Deployment, den TeamSpeak-Start, das Laden des Plugins und ob die Laufzeit korrekt initialisiert wurde, statt in den Safe-Mode zu wechseln.

## 🇩🇪 Laufzeitdaten

Benutzerdaten werden am Plattform-AppData-Pfad gespeichert in:

- `RP Soundboard Ultimate/library.json`
- `RP Soundboard Ultimate/boards-config.json`
- `RP Soundboard Ultimate/plugin-config.json`
- `RP Soundboard Ultimate/sounds/`

## 🇩🇪 Audio- und Inhaltsrichtlinie

- Im Repository werden keine Sound-Samples mitgeliefert.
- Keine urheberrechtlich geschützten Audiodateien committen, für die keine Weiterverbreitungsrechte vorliegen.
- Du bist selbst verantwortlich für die Rechtmäßigkeit aller Audiodateien, die du importierst, herunterlädst, speicherst oder überträgst.

Empfohlene Quellen:

- selbst erstellte Originalaufnahmen
- gemeinfreie Audiodateien
- ausdrücklich lizenzierte, lizenzfreie oder Creative-Commons-Audiodateien mit Weiterverbreitungsrechten

## 🇩🇪 Markenrechtlicher Hinweis

- TeamSpeak ist eine Marke der TeamSpeak Systems GmbH.
- Grand Theft Auto und GTA sind Marken von Take-Two Interactive / Rockstar Games.
- Dieses Projekt ist inoffiziell und steht in keiner Verbindung zu den jeweiligen Markeninhabern, wird von ihnen weder unterstützt noch gesponsert.

## 🇩🇪 Open-Source-Dateien

- [LICENSE](LICENSE)
- [CHANGELOG.md](CHANGELOG.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [SECURITY.md](SECURITY.md)
- [RELEASE_TEMPLATE.md](RELEASE_TEMPLATE.md)

---

# 🇬🇧 English

RP Soundboard Ultimate is an open-source TeamSpeak 3 soundboard plugin for Windows. It ships as a native TS3 plugin package, not as a separate Electron app.

## 🇬🇧 Install

1. Go to [**Releases**](https://github.com/fALECX/TS3-Soundboard-Ultimate/releases/latest) and download `rp_soundboard_ultimate_*.ts3_plugin`
2. Double-click the downloaded file — TeamSpeak opens the plugin installer automatically
3. Restart TeamSpeak
4. The soundboard is available under **Plugins → RP Soundboard Ultimate** in the TS3 menu

## 🇬🇧 What It Does

- Injects sound playback directly into TeamSpeak captured voice
- Opens a TeamSpeak-owned Qt control window from the plugin menu
- Imports local audio files into a persistent sound library
- Supports YouTube search/download through bundled helper tools
- Stores boards, library metadata, and runtime config in the user's app data directory
- Migrates legacy data from older RP Soundboard Ultimate desktop installs when present

## 🇬🇧 Project Status

This repository is structured for open-source release around the native plugin path only.

- Kept: native plugin source, TeamSpeak SDK headers, build/package scripts, release docs
- Removed: tracked build outputs, obsolete Electron/web app files, bundled sample audio, temporary test artifacts, embedded upstream checkout

The upstream implementation reference is documented in [docs/upstream-basis.md](docs/upstream-basis.md).

## 🇬🇧 Requirements

- Windows 10 or 11
- TeamSpeak 3 64-bit client
- CMake 3.21+
- Visual Studio 2022 or another Windows C++17 toolchain
- Qt 5 or Qt 6 with `Core`, `Widgets`, and `Network`

## 🇬🇧 Repository Layout

- `src/`: native plugin, runtime, storage, playback, and Qt UI code
- `pluginsdk/include/`: TeamSpeak 3 plugin SDK headers
- `scripts/test-teamspeak.ps1`: local build, deploy, and TeamSpeak smoke test
- `scripts/validate-release.js`: release metadata validation
- `tests/release.test.js`: lightweight repository-level release checks
- `sounds/`: ignored runtime sound storage placeholder only
- `resources/`: ignored helper-tool drop location placeholder only

## 🇬🇧 Build

```powershell
cmake -S . -B build_msvc_qt5 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_msvc_qt5
cmake --install build_msvc_qt5 --prefix build_msvc_qt5/install
```

For Visual Studio generators, use the usual `--config Release` form instead.

## 🇬🇧 Package

To produce the final TeamSpeak plugin package:

```powershell
cmake -S . -B build_msvc_qt5 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DRPSU_MAKE_PLUGIN_FILE=ON
cmake --build build_msvc_qt5
cmake --install build_msvc_qt5 --prefix build_msvc_qt5/install
```

Primary outputs:

- `build_msvc_qt5/install/plugins/rp_soundboard_ultimate_win64.dll`
- `build_msvc_qt5/install/plugins/rp_soundboard_ultimate/rp_soundboard_ultimate_runtime.bin`
- `release/rp_soundboard_ultimate_<version>.ts3_plugin`

## 🇬🇧 Install In TeamSpeak

1. Build the `.ts3_plugin` package.
2. Double-click it, or drag it onto TeamSpeak's `package_inst.exe`.
3. Restart TeamSpeak if needed.
4. Open `Tools -> Options -> Addons` and confirm `RP Soundboard Ultimate` is loaded.

## 🇬🇧 Local Verification

Repository checks:

```powershell
npm test
npm run validate:release
```

Full TeamSpeak smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-teamspeak.ps1 -Build -StopTeamSpeak -StartTeamSpeak -BuildDir .\build_msvc_qt5
```

The smoke test verifies deployment, TeamSpeak launch, plugin load, and whether the runtime initialized instead of falling back to safe mode.

## 🇬🇧 Runtime Data

User data is stored under the platform app-data location in:

- `RP Soundboard Ultimate/library.json`
- `RP Soundboard Ultimate/boards-config.json`
- `RP Soundboard Ultimate/plugin-config.json`
- `RP Soundboard Ultimate/sounds/`

## 🇬🇧 Audio And Content Policy

- No bundled sound samples are shipped in the repository.
- Do not commit copyrighted audio you do not have redistribution rights for.
- You are responsible for the legality of any audio you import, download, store, or transmit.

Recommended sources:

- original recordings you created
- public-domain audio
- explicitly licensed royalty-free or Creative Commons audio with redistribution rights

## 🇬🇧 Trademark Disclaimer

- TeamSpeak is a trademark of TeamSpeak Systems GmbH.
- Grand Theft Auto and GTA are trademarks of Take-Two Interactive / Rockstar Games.
- This project is unofficial and not affiliated with, endorsed by, or sponsored by those trademark owners.

## 🇬🇧 Open Source Files

- [LICENSE](LICENSE)
- [CHANGELOG.md](CHANGELOG.md)
- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [SECURITY.md](SECURITY.md)
- [RELEASE_TEMPLATE.md](RELEASE_TEMPLATE.md)
