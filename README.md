# nexus-gamesync

Native game-lighting capture shims for Nexus's **Game Sync** mode. These DLLs
impersonate vendor RGB SDKs so that games hand their lighting to Nexus, which
then renders it on the user's own (non-Razer) hardware.

Windows-only. GSI (game-state integration) is a separate, service-side mechanism
and does **not** live here.

## What it does

A Chroma game loads `CChromaEditorLibrary`, which in turn `LoadLibrary`s the
Razer SDK DLL (`RzChromaSDK64.dll`, or `RzChromatic64.dll` on newer editor-lib
builds). We ship a drop-in replacement under both names that implements the SDK
surface, captures the per-key keyboard effect grids, and forwards the active
grid to the local Nexus service.

Capture model: games create a few keyboard effects once (colours baked in) then
animate by `SetEffect(id)` cycling between them. So grids are stored by effect id
at create time, and the active grid is forwarded on `SetEffect`. Forwarding runs
on a worker thread (never the game thread): `SetEffect` publishes the active grid
and signals an event; the worker coalesces and POSTs to
`http://127.0.0.1:9400/lighting/game-sync/frame` (token fetched from `/pair`).
Colours are COLORREF (`0x00BBGGRR`).

## Layout

- `src/rzchroma.c` — the shim (capture + forwarding worker).
- `src/rzchroma.def` — the 18 exports the editor lib resolves.
- `src/rzchroma.rc` — version resource (must be >= 1.0.0.36 or the editor lib rejects it).
- `build.bat` — MSVC build → `dist/RzChromaSDK64.dll` + `dist/RzChromatic64.dll`.

## Build

Requires MSVC (VS 2022 Build Tools). Run `build.bat`. Output is two identically
built DLLs (same exports/version) under `dist/`; ship both names because which
one a game's editor lib loads is build-specific.

## Install models

- **System-wide (Model A, target):** the Nexus installer places the DLLs in
  `System32`/`SysWOW64` so any Chroma game loads them with zero per-game setup.
- **Per-game (fallback):** drop the DLLs next to a game's executable.

## Legal

This is a clean-room reimplementation of the SDK surface; it links no Razer code.
The Razer Chroma SDK developer EULA has not been reviewed — read it before any
public/commercial distribution.
