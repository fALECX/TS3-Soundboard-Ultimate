# Changelog

All notable changes to this project will be documented in this file.

## [0.5.1] - 2026-05-23

### Added
- Trim feature: right-click a sound in the library and pick **Trim...** to open a timeline-style dialog with two draggable handles for start and end. Includes "Play Selection" audition, numeric `mm:ss.mmm` readouts, and a Reset button. Trim values are non-destructive metadata stored on the sound record and applied at playback time via FFmpeg `-ss` / `-t`.

### Fixed
- TeamSpeak could crash when starting a new sound while another was paused. The synchronous FFmpeg decode now runs outside the Sampler's audio mutex so the TS3 audio callback is no longer blocked long enough to trigger TS3's watchdog.
- Long YouTube downloads (1h+ videos) were aborted after 3 minutes by a fixed wall-clock timeout. Replaced with a stall-based timeout that only kills the download when yt-dlp produces no new output for 2 minutes — so legitimately long downloads finish while hung child processes are still caught.
- Renaming a sound while it was playing or paused left the old name in the preview bar. The active title now updates live.
- Trimming a sound that was already playing let the stale region finish out. Playback now stops on save so the next play decodes a fresh PCM segment with the new trim.

## [Unreleased]

### Changed
- Repository cleanup for open-source release readiness.
- Documentation rewritten around the native TeamSpeak plugin path.
- Maintainer contact and attribution added to project docs.

### Removed
- Tracked build artifacts and temporary test outputs from version control.
- Obsolete web/Electron leftovers and release-risk bundled sample audio.
- Vendored upstream checkout from the repository tree.

## [0.2.0] - 2026-04-04

### Added
- Native TeamSpeak 3 plugin runtime for Windows.
- CMake-based plugin build and `.ts3_plugin` packaging flow.
- TeamSpeak package installer wizard distribution path.
- JSON state storage and first-run migration from legacy Electron data.
- Direct captured-voice audio injection path.
- TeamSpeak-owned Qt soundboard window and plugin menu entry.

### Changed
- Release metadata now points to the TS3 plugin package instead of Electron.
- Repo documentation now matches the real TeamSpeak installer flow.
- Versioning is aligned across CMake, package metadata, and release validation.

### Removed
- Electron builder release surface from the publish path.
- NSIS installer references from the release story.

## [0.1.0] - 2026-03-14

### Added
- Initial RP soundboard feature set.
