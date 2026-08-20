# Pointer notes — DSR Sitcom

Offsets and AOBs are sourced from community research (notably Phokz's DSR cheat tables and tools such as DSR-Gadget / DSR stream trackers). Credit to those authors.

Record the game build you tested against here after first successful run:

| Field | Value |
| --- | --- |
| Steam build / app ver | _(fill in)_ |
| `DarkSoulsRemastered.exe` size | _(fill in)_ |
| SHA256 | _(fill in)_ |
| Date tested | _(fill in)_ |

## BaseB (player vitals)

- **AOB:** `48 8B 05 ?? ?? ?? ?? 45 33 ED 48 8B F1 48 85 C0`
- **Resolve:** RIP-relative `mov rax, [rip+disp]` → pointer slot
- **Chains:**
  - `BaseB = *slot`
  - `ChrStat = *(BaseB + 0x10)`
  - HP = `*(ChrStat + 0x14)` (int32)
  - MaxHP = `*(ChrStat + 0x1C)` (int32)
  - Deaths = `*(BaseB + 0x98)` (int32)

## BaseE (area / world)

- **AOB:** `48 8B 05 ?? ?? ?? ?? 48 8B 88 98 0B 00 00 8B 41 3C C3`
- **Chains:**
  - `BaseE = *slot`
  - Area Number = `*(BaseE + 0xA22)` (uint8)
  - World Number = `*(BaseE + 0xA23)` (uint8)

Scene wipe fires when these change while a character is loaded. This approximates the area title-card moment (not a direct UI-visibility flag).

## Boss (lock-on capture)

- **Pattern (LastLockedTarget2):**  
  `48 8B 03 48 8B CB E9 ?? ?? ?? ?? 48 8D 64 24 08 48 8D 64 24 08`
- Hook captures `RBX` (ChrIns*) when the game resolves a locked target.
- On that pointer (Phokz "Last Hit Entity" layout):
  - HP = `+0x3E8`
  - MaxHP = `+0x3EC`
  - CharacterID = `+0xC8`
- Boss filter: known boss character IDs, with a high-MaxHP fallback (≥ 2000).

Cheer on rising edge of “boss target alive”; applause when that boss HP hits 0 / bar clears after being armed.

## Failure modes

If an AOB misses after a game patch, enable `log=true` and check `sitcom/sitcom.log`. Player events need BaseB; area wipe needs BaseE; boss events need the lock-on hook pattern.
