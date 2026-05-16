# Contributing

Thanks for contributing to RP Soundboard Ultimate.

## Maintainer Context

Project owner and public-facing maintainer:

- fALECX
- Twitch: https://twitch.tv/fALECX
- X: https://x.com/fALECX
- Discord: `falecx`

## Scope

This repository is centered on the native TeamSpeak 3 plugin release path.

Preferred contribution areas:

- plugin loading and TeamSpeak integration
- audio playback and captured-voice injection
- Qt UI and usability improvements
- packaging, install flow, and release validation
- documentation and release hygiene

Do not reintroduce Electron, browser UI, or unrelated desktop-app scaffolding.

## Workflow

1. Create a focused branch.
2. Keep changes small and reviewable.
3. Update docs when behavior or release flow changes.
4. Run the relevant checks before opening a PR.

## Local Checks

Repository-level checks:

```powershell
npm test
npm run validate:release
```

Native build:

```powershell
cmake -S . -B build_msvc_qt5 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build_msvc_qt5
cmake --install build_msvc_qt5 --prefix build_msvc_qt5/install
```

TeamSpeak smoke test:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test-teamspeak.ps1 -Build -StopTeamSpeak -StartTeamSpeak -BuildDir .\build_msvc_qt5
```

## Pull Request Expectations

Include:

- the problem being solved
- the implementation approach
- test evidence
- any TeamSpeak/runtime caveats

## Contribution Standards

- Do not commit generated build output.
- Do not commit temporary test files.
- Do not commit bundled audio unless you clearly own redistribution rights.
- Preserve Windows-native and TeamSpeak-native behavior.
- Keep release metadata aligned across `CMakeLists.txt`, `package.json`, docs, and packaging files.

## Pull Request Checklist

- [ ] Build succeeds from a clean configure
- [ ] `npm test` passes
- [ ] `npm run validate:release` passes
- [ ] TeamSpeak plugin load path still works if touched
- [ ] Docs/changelog updated when needed
- [ ] No local artifacts, logs, or generated outputs were committed
