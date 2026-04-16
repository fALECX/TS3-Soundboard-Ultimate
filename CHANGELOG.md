# Changelog

All notable changes to this project will be documented in this file.

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
