# Project: TS3 Soundboard Ultimate

## Critical: Do Not Touch
- **Sound routing to microphone logic** — this is fickle. Do NOT change it. If you see a better way or need to modify it, **ask first** before touching anything.

## General Rules
- This is a **TeamSpeak 3 plugin**. All code changes must respect TS3 plugin boundaries and constraints (thread safety with TS3 callbacks, no blocking the audio thread, plugin API limitations, etc.).
- After every code change, **recheck the changed code for problems** — logic errors, thread safety issues, TS3-specific pitfalls, regressions.
