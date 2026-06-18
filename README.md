# nexus-gamesync

Native game-lighting capture shims for Nexus's **Game Sync** mode. These DLLs
impersonate vendor RGB SDKs so that games hand their lighting to Nexus, which
then renders it on the user's own hardware.

Windows-only. GSI (game-state integration) is a separate, service-side mechanism
and does **not** live here.

## What it does

Three shims impersonate three vendor SDKs. Each is a drop-in replacement loaded
by bare name (from System32 / SysWOW64) in place of the real vendor DLL; it
implements the SDK surface, captures the game's lighting, and forwards it to the
local Nexus service. All three share one forwarding worker (`src/forward.c`):
a thread + WinHTTP that fetches the pair token from `/pair`, then POSTs frame
JSON to `http://127.0.0.1:9400/lighting/game-sync/frame` (coalesced to the
service render rate). Colours go on the wire as COLORREF `0x00BBGGRR`.

| Shim | DLL(s) | Vendor SDK | ABI |
|------|--------|------------|-----|
| Razer Chroma | `RzChromaSDK64.dll` / `RzChromatic64.dll` (x64), `RzChromaSDK.dll` / `RzChromatic.dll` (x86) | Razer Chroma | undecorated, GUID effect ids |
| Alienware/Dell LightFX | `LightFX.dll` | Dell LightFX 2.0 | `__stdcall`, `LFX_RESULT` |
| Logitech | `LogitechLedEnginesWrapper.dll` / `LogitechLed.dll` | Logitech Gaming LED | `__cdecl`, `bool` |

### Capture / commit model (differs per SDK)

- **Chroma** - games either create a few keyboard effects once (colours baked
  in) then animate by `SetEffect(id)` cycling between them (Cyberpunk), or
  re-create the effect every frame and never `SetEffect` (Dead Cells). Grids are
  stored by effect id at create time and the active grid is forwarded on BOTH
  `CreateKeyboardEffect` and `SetEffect`. Both the 6x22 and 8x24 keyboard grids
  forward on the wire as effect `CHROMA_CUSTOM` (the service treats both sizes
  identically; there is no distinct `CHROMA_CUSTOM2` wire effect).
- **LightFX** - staged state since the last `LFX_Reset` is committed on
  `LFX_Update`. `LFX_SetLightColor` / `LFX_Light(mask,rgb)` only stage;
  `LFX_Update` forwards one representative colour (`CHROMA_STATIC` 1x1).
- **Logitech** - no commit call; each set-call IS the frame. `LogiLedSetLighting`
  forwards a solid (`CHROMA_STATIC` 1x1); `LogiLedSetLightingFromBitmap` forwards
  the 21x6 BGRA bitmap as a `CHROMA_CUSTOM` 6x21 grid. Per-key set-calls no-op.

## Layout

- `src/forward.c` / `src/forward.h` - shared forwarding worker (token, WinHTTP,
  coalescing, app-title sanitize, frame JSON). Each shim links it.
- `src/rzchroma.c` / `.def` / `.rc` - Razer Chroma shim (18 exports).
- `src/lightfx.c` / `.def` / `.rc` - LightFX shim (19 exports).
- `src/logiled.c` / `.def` / `.rc` - Logitech shim (30 exports, the official set).
- `build.bat` - x64 MSVC build of all three shims -> `dist/`.
- `build32.bat` - x86 MSVC build of all three shims -> `dist/x86/`.
- `build32dbg.bat` - x86 debug Chroma build with `/DSHIM_LOG`.
- `install/deploy-system32.ps1` - dev helper to stage both arches into
  System32/SysWOW64 (production install is service-managed by nexus-service,
  which claims the DLLs by version-resource `CompanyName == "Nexus"`).
- `test/` - dev harnesses (`loadtest.c` LightFX+Logitech load/forward;
  `selftest_fwd.c` / `editortest.c` Chroma; `modlist.c` 32-bit module enumerator).
- `THIRD-PARTY.md` - MIT attribution for Aurora-Wrappers + vendor-header notes.

## Build

Requires MSVC (VS 2022 Build Tools). Run `build.bat` (x64) and `build32.bat`
(x86) - 32-bit games load the x86 DLLs from SysWOW64. x64 DLLs land in `dist/`;
x86 DLLs land in `dist/x86/` (LightFX/Logitech share their names across arches,
so the arches are kept in separate dirs). The Chroma DLLs ship under two names
each because which one a game's editor lib loads is build-specific.

## Install models

- **System-wide (Model A, target):** the Nexus installer places the DLLs in
  `System32`/`SysWOW64` so any game loads them with zero per-game setup.
- **Per-game (fallback):** drop the DLLs next to a game's executable.

## Legal

Clean-room reimplementations of the SDK surfaces; they link no vendor code. See
`THIRD-PARTY.md`. The vendor SDK EULAs have not been reviewed - read them before
any public/commercial distribution.
