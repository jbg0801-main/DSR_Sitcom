# Pointer notes — DSR Sitcom

Offsets and AOBs are sourced from community research (Phokz cheat tables, DSR-Gadget, vanilla EMEVD). Credit to those authors.

Record the game build you tested against here after first successful run:

| Field | Value |
| --- | --- |
| Steam build / app ver | _(fill in)_ |
| `DarkSoulsRemastered.exe` size | _(fill in)_ |
| SHA256 | _(fill in)_ |
| Date tested | _(fill in)_ |

## BaseB (player vitals)

- **AOB:** `48 8B 05 ?? ?? ?? ?? 45 33 ED 48 8B F1 48 85 C0`
- `ChrStat = *(BaseB + 0x10)`
- HP / MaxHP / Deaths at `ChrStat+0x14`, `+0x1C`, `BaseB+0x98`

## BaseE (area / world)

- **AOB:** `48 8B 05 ?? ?? ?? ?? 48 8B 88 98 0B 00 00 8B 41 3C C3`
- Area / World bytes at `BaseE+0xA22` / `+0xA23`
- `255/255` treated as menu/load (not gameplay)
- **Not used for scene wipe** — those bytes are load-zone IDs and thrash at chunk boundaries

## MenuMan (area title card / Display Banner)

- **AOB (RemasterCETable, preferred):** `48 8B 05 ?? ?? ?? ?? 89 88 28 08 00 00 85 C9`
- **Fallback (DSR-Gadget long):** `48 8B 05 ?? ?? ?? ?? 89 88 28 08 00 00 85 C9 ?? ?? C7 80 34 08 00 00 FF FF FF FF C3`
- Pointer: `MenuMan = *(slot)` (one deref)
- **Message** byte at `MenuMan + 0x104` — same IDs as EMEVD `Display Banner` / RemasterCETable dropdown:
  - `8` = **Current Location** (area title card / Map Name) ← scene wipe trigger
  - `13` = Bonfire Lit, `2` = You Died, etc.
- Scene wipe = rising edge of Message → `8`
- **Title screen:** `int32` at `MenuMan + 0x80` == `1` (DS1 Overhaul) → credit overlay

## Sound Effect volume

- Live from FMOD Event category **`SE`** in `fmod_event64.dll`
- Hooks: C `GetCategory` / `SetVolume` / `Update` / `Create`, plus C++ `EventSystemI::update` and
  `EventCategoryI::setVolume` when exported
- Fallback probe: Overhaul chain `fmod_event64+0x77278` → `+0x470` → `+0x40` (same root as game-time)
- Final gain = `config.ini volume` × FMOD SE category volume (0–1)

## Title credit

- When `MenuMan+0x80 == 1`, hook `IDXGISwapChain::Present` and blit a GDI-rendered text texture
  onto the backbuffer with `CopySubresourceRegion` (no `d3dcompiler`, works under DXVK/Proton)

## Event flags (boss cheer / applause)

- **AOB (DSR-Gadget / EventPocket):** `48 8B 0D ?? ?? ?? ?? 99 33 C2 45 33 C0 2B C2 8D 50 F6`
- Pointer chain: `bitfield = **(slot)` (two derefs), then DSR-Gadget `getEventFlagOffset` bit packing
- Missing the second deref reads garbage → sticky “boss fight” and mass false defeat edges

**Cheer** = rising edge of the vanilla EMEVD bar-up flag (e.g. Asylum `11815396` set with Display Boss Health Bar)

**Applause** = rising edge of the vanilla boss **defeated** flag (e.g. Asylum `16`)

Baselines are re-seeded whenever you leave/re-enter gameplay so already-on defeat flags don’t fire on load.

## Failure modes

If an AOB misses after a patch, set `log=true` and check `sitcom/sitcom.log`.
