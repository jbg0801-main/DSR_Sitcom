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
- Area / World bytes at `BaseE+0xA22` / `+0xA23` (scene wipe proxy)

## Event flags (boss cheer / applause)

- **AOB (DSR-Gadget):** `48 8B 0D ?? ?? ?? ?? 99 33 C2 45 33 C0 2B C2 8D 50 F6`
- Bit packing matches DSR-Gadget `getEventFlagOffset`

**Cheer** = rising edge of a vanilla EMEVD “boss fight active” flag while the boss is not defeated:

- Prefer explicit bar toggles (e.g. Asylum `11815396` ON with Display Boss Health Bar, OFF when leaving)
- Otherwise the Event ID of the `Display Boss Health Bar` event that **Restarts on Bonfire Rest** (clears when you die/rest, sets again when you re-enter the fog)

**Applause** = rising edge of the vanilla boss **defeated** event flag (e.g. Asylum `16`, Quelaag `9`, …)

This is intentionally **not** lock-on based.

## Failure modes

If an AOB misses after a patch, set `log=true` and check `sitcom/sitcom.log`.
