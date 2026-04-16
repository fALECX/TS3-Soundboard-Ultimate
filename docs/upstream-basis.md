# Upstream Basis

This repository is based on the public TeamSpeak 3 plugin project:

- Repository: https://github.com/MGraefe/RP-Soundboard
- Purpose: easy-to-use RP soundboard for TeamSpeak 3
- Install flow: `.ts3_plugin` package installed with TeamSpeak's built-in package installer

## What The Upstream Plugin Does

The upstream project is a native TeamSpeak plugin, not a desktop soundboard that routes audio through a virtual cable. Its core behavior is:

- Direct integration with TeamSpeak 3 plugin callbacks
- Audio playback mixed into TS3 voice processing
- Per-sound trimming, gain, hotkeys, and playback control
- Built-in package delivery through TeamSpeak's installer wizard

## What This Repository Must Stay Aligned With

For release and review work, treat the upstream repo as the behavior reference for:

- TeamSpeak plugin packaging and install flow
- Voice-routing behavior inside the TS3 client
- Hotkey and menu entry conventions
- Audio handling expectations for the soundboard runtime

## Current Parity Status

This repository now uses the upstream playback core directly for the TeamSpeak routing path.

Directly transplanted upstream playback pieces:

- `Sampler`
- `SampleBuffer`
- `SampleProducerThread`
- `SoundInfo`
- `TalkStateManager`
- callback flow around captured voice, mixed playback, talk-state, and active-server switching

Known local divergences:

- The local build still uses a loader/runtime split, while the upstream plugin ships as a single plugin binary.
- The decode entry point is adapted to call bundled `ffmpeg.exe` and stream PCM into the copied sampler pipeline, instead of linking libavcodec/libavformat directly in-process.
- The separate `rpsu_ui_preview.exe` is a local testing convenience and is not part of the upstream plugin architecture.

## Notes

- This file exists so the upstream reference does not need to be repeated in conversation.
- The upstream repository is referenced as documentation only and is not vendored inside this repo.
- If implementation changes diverge from the upstream behavior, update this document before release.
- The upstream source was checked on 2026-04-14.
