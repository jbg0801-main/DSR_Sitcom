# DSR Sitcom

Sitcom canned-audience overlay for **Dark Souls Remastered** (PC).

Plays local WAV clips when:

- the player takes damage or dies (laughter; random among `laugh*.wav`)
- a boss fight starts / HP bar comes up via vanilla event flags (cheering)
- that boss’s defeated flag flips (applause; **Sif → boo** instead)
- the area title card appears (scene wipe — MenuMan banner type 8)
- an item is granted via the game’s ItemGet path (**ooh**)
- you die during the **Pinwheel** fight (**sad trombone**)
- you die during the **Bed of Chaos** fight (`BoCDeath.wav`)
- you take a heavy hit during **Ornstein & Smough** (**bonk**, damage heuristic)

## Install

1. Copy `dinput8.dll` and the `sitcom/` folder into the game directory (next to `DarkSoulsRemastered.exe`).
2. Launch the game normally.
3. **Linux / Proton** launch options:

```text
WINEDLLOVERRIDES="dinput8.dll=n,b" %command%
```

4. Put your WAVs in `sitcom/sounds/` using the names below.
5. Tweak `sitcom/config.ini` as needed. Set `log=true` to write `sitcom/sitcom.log`.

**Offline / single-player recommended.** Any injected DLL can interact badly with Softs / online play.

If another mod already ships `dinput8.dll`, only one can own that filename unless you chain-load (not supported in v1).

## Sound files

Preferred format: **WAV, PCM signed 16-bit, 44100 Hz** (48000 OK). Stereo is fine.

The loader picks randomly among `name.wav` and `name_XX.wav` / `name-XX.wav` for each
category when multiple files are present (avoids immediate repeats).

| Prefix / name | Event |
| --- | --- |
| `laugh.wav` / `laugh_XX.wav` | Hit / death / (future fail laughs) |
| `cheer.wav` / `cheer_XX.wav` | Boss bar |
| `applause.wav` / `applause_XX.wav` | Boss death (non-Sif) |
| `boo.wav` / `boo_XX.wav` | Sif defeated |
| `ooh.wav` / `ooh_XX.wav` | Item get |
| `trombone.wav` / `trombone_XX.wav` | Death to Pinwheel |
| `BoCDeath.wav` / `BoCDeath_XX.wav` | Death to Bed of Chaos |
| `bonk.wav` / `bonk_XX.wav` | Heavy O&S hit (Smough heuristic) |
| `scene_wipe.wav` / `scene_wipe_XX.wav` | Area title card |

## Build (Linux → Windows DLL)

Requires MinGW-w64 (`x86_64-w64-mingw32-g++`) and CMake.

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake
cmake --build build -j
```

Or: `bash scripts/build-podman.sh` (builds + packs `dist/DSR_Sitcom.zip`).

## How it works

`dinput8.dll` proxies `DirectInput8Create` to the system DLL and starts a worker thread that:

1. Resolves `BaseB` / `BaseE` / MenuMan / event flags (and ItemGet) via AOB scans
2. Polls ~20 Hz for HP, boss flags, banners, and pending ItemGet counts
3. Plays WAVs through **winmm `PlaySound`** with PCM gain  
   (`config volume` × in-game **Sound Effect** / FMOD `SE` when captured)
4. Draws a title-menu credit (`Sitcom mod by jbg0801 2026` plus dedication)

See [tools/POINTERS.md](tools/POINTERS.md) for pointer notes.