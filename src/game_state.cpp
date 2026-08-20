#include "game_state.h"

#include "event_flags.h"
#include "log.h"

#include <windows.h>
#include <psapi.h>

#include <cstring>
#include <string>
#include <unordered_set>

namespace sitcom {
namespace {

EventFlags g_event_flags;

bool IsReadable(std::uintptr_t ptr) {
  if (!ptr) {
    return false;
  }
  MEMORY_BASIC_INFORMATION mbi{};
  if (!VirtualQuery(reinterpret_cast<LPCVOID>(ptr), &mbi, sizeof(mbi))) {
    return false;
  }
  if (mbi.State != MEM_COMMIT) {
    return false;
  }
  const DWORD p = mbi.Protect & 0xFF;
  return p == PAGE_READONLY || p == PAGE_READWRITE || p == PAGE_EXECUTE_READ ||
         p == PAGE_EXECUTE_READWRITE || p == PAGE_WRITECOPY || p == PAGE_EXECUTE_WRITECOPY;
}

template <typename T>
T ReadT(std::uintptr_t ptr, T fallback = T{}) {
  return IsReadable(ptr) ? *reinterpret_cast<T*>(ptr) : fallback;
}

std::uintptr_t PatternScan(std::uintptr_t base, std::size_t size, const std::uint8_t* pat,
                           const char* mask) {
  const std::size_t len = std::strlen(mask);
  for (std::size_t i = 0; i + len <= size; ++i) {
    bool ok = true;
    for (std::size_t j = 0; j < len; ++j) {
      if (mask[j] == 'x' && *reinterpret_cast<const std::uint8_t*>(base + i + j) != pat[j]) {
        ok = false;
        break;
      }
    }
    if (ok) {
      return base + i;
    }
  }
  return 0;
}

std::uintptr_t ResolveRipMov(std::uintptr_t insn) {
  const auto rel = *reinterpret_cast<std::int32_t*>(insn + 3);
  return insn + 7 + rel;
}

// cheer_flag → defeat_flag. Cheer = vanilla EMEVD "boss bar up" flag
// (e.g. Asylum SetEventFlag 11815396 when Display Boss Health Bar enables).
struct BossPair {
  std::int32_t cheer_flag;
  std::int32_t defeat_flag;
};

constexpr BossPair kBossPairs[] = {
    {11815396, 16},         // Asylum Demon
    {11815382, 11810900},   // Stray Demon
    {11805392, 15},         // Gwyn
    {11705392, 14},         // Seath
    {11705383, 14},         // Seath (crystal cave approach)
    {11605392, 13},         // Four Kings
    {11605382, 13},         // Four Kings (alt)
    {11515382, 11510900},   // Gwyndolin
    {11515392, 12},         // Ornstein & Smough
    {11515396, 12},         // O&S phase / super
    {11505392, 11},         // Iron Golem
    {11415392, 10},         // Bed of Chaos
    {11415372, 11410900},   // Ceaseless Discharge
    {11415382, 11410901},   // Centipede Demon
    {11415342, 11410410},   // Demon Firesage
    {11405392, 9},          // Quelaag
    {11315392, 7},          // Nito
    {11305392, 6},          // Pinwheel
    {11215003, 11210000},   // Sanctuary Guardian
    {11215013, 11210001},   // Artorias
    {11215023, 11210002},   // Manus
    {11215063, 11210005},   // Kalameet
    {11205392, 5},          // Sif
    {11205382, 11200900},   // Moonlight Butterfly
    {11105392, 4},          // Priscilla
    {11105398, 4},          // Priscilla (alt)
    {11015392, 3},          // Bell Gargoyles
    {11015396, 3},          // Bell Gargoyles (2nd)
    {11015384, 11010901},   // Taurus Demon
    {11015372, 11010902},   // Capra Demon
    {11005392, 2},          // Gaping Dragon
};

}  // namespace

bool GameState::ResolveBases() {
  HMODULE game = GetModuleHandleW(L"DarkSoulsRemastered.exe");
  if (!game) {
    return false;
  }
  MODULEINFO info{};
  if (!GetModuleInformation(GetCurrentProcess(), game, &info, sizeof(info))) {
    return false;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
  const auto size = static_cast<std::size_t>(info.SizeOfImage);

  const std::uint8_t pat_b[] = {0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x45, 0x33, 0xED, 0x48, 0x8B, 0xF1,
                                0x48, 0x85, 0xC0};
  const char mask_b[] = "xxx????xxxxxxxxx";
  const auto hit_b = PatternScan(base, size, pat_b, mask_b);
  if (!hit_b) {
    LogWrite("game_state: BaseB pattern not found");
    return false;
  }
  base_b_ptr_ = ResolveRipMov(hit_b);

  const std::uint8_t pat_e[] = {0x48, 0x8B, 0x05, 0,    0,    0,    0,    0x48, 0x8B, 0x88,
                                0x98, 0x0B, 0x00, 0x00, 0x8B, 0x41, 0x3C, 0xC3};
  const char mask_e[] = "xxx????xxxxxxxxxxx";
  const auto hit_e = PatternScan(base, size, pat_e, mask_e);
  if (hit_e) {
    base_e_ptr_ = ResolveRipMov(hit_e);
  } else {
    LogWrite("game_state: BaseE pattern not found (area context disabled)");
    base_e_ptr_ = 0;
  }

  // MenuMan — Display Banner / "Message" at +0x104 (8 = Current Location title card)
  // Prefer RemasterCETable short AOB (works across more builds); fall back to DSR-Gadget long form.
  // 48 8B 05 ?? ?? ?? ?? 89 88 28 08 00 00 85 C9
  const std::uint8_t pat_m_short[] = {0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x89, 0x88, 0x28,
                                      0x08, 0x00, 0x00, 0x85, 0xC9};
  const char mask_m_short[] = "xxx????xxxxxxxx";
  auto hit_m = PatternScan(base, size, pat_m_short, mask_m_short);
  if (!hit_m) {
    const std::uint8_t pat_m_long[] = {
        0x48, 0x8B, 0x05, 0,    0,    0,    0,    0x89, 0x88, 0x28, 0x08, 0x00, 0x00, 0x85,
        0xC9, 0,    0,    0xC7, 0x80, 0x34, 0x08, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xC3};
    const char mask_m_long[] = "xxx????xxxxxxx??xxxxxxxxxxxx";
    hit_m = PatternScan(base, size, pat_m_long, mask_m_long);
  }
  if (hit_m) {
    menu_man_ptr_ = ResolveRipMov(hit_m);
  } else {
    LogWrite("game_state: MenuMan pattern not found (title-card wipe disabled)");
    menu_man_ptr_ = 0;
  }

  char buf[160];
  snprintf(buf, sizeof(buf), "game_state: BaseB*=0x%llX BaseE*=0x%llX MenuMan*=0x%llX",
           static_cast<unsigned long long>(base_b_ptr_),
           static_cast<unsigned long long>(base_e_ptr_),
           static_cast<unsigned long long>(menu_man_ptr_));
  LogWrite(buf);
  return base_b_ptr_ != 0;
}

bool GameState::Init() {
  ready_ = ResolveBases();
  if (ready_) {
    if (!g_event_flags.Init()) {
      LogWrite("game_state: event flags unavailable (boss cheer/applause disabled)");
    }
  }
  return ready_;
}

void GameState::Shutdown() {
  g_event_flags.Shutdown();
  ready_ = false;
  base_b_ptr_ = 0;
  base_e_ptr_ = 0;
  menu_man_ptr_ = 0;
}

GameSnapshot GameState::Read() {
  GameSnapshot s{};
  if (!ready_ || !base_b_ptr_) {
    return s;
  }

  if (!IsReadable(base_b_ptr_)) {
    return s;
  }
  const auto base_b = ReadT<std::uintptr_t>(base_b_ptr_);
  if (!base_b || !IsReadable(base_b)) {
    return s;
  }

  const auto chr_stat = ReadT<std::uintptr_t>(base_b + 0x10);
  if (!chr_stat || !IsReadable(chr_stat)) {
    return s;
  }

  s.player_hp = ReadT<std::int32_t>(chr_stat + 0x14);
  s.player_max_hp = ReadT<std::int32_t>(chr_stat + 0x1C);
  s.deaths = ReadT<std::int32_t>(base_b + 0x98);
  // Allow hp > max (load glitches / buffs); reject obvious garbage.
  s.player_valid = (s.player_max_hp > 0 && s.player_max_hp <= 99999 && s.player_hp >= 0 &&
                    s.player_hp <= 99999);

  if (base_e_ptr_ && IsReadable(base_e_ptr_)) {
    const auto base_e = ReadT<std::uintptr_t>(base_e_ptr_);
    if (base_e && IsReadable(base_e + 0xA22)) {
      s.area_number = ReadT<std::uint8_t>(base_e + 0xA22);
      s.world_number = ReadT<std::uint8_t>(base_e + 0xA23);
      // 255 = unloaded / menu / load screen sentinel in practice.
      s.area_valid = (s.area_number != 255 && s.world_number != 255);
    }
  }

  // In-world: real map loaded. Player vitals may be briefly invalid during loads.
  s.in_gameplay = s.area_valid && s.player_valid;

  // Display Banner ID (area title card = 8 Current Location).
  // Title screen flag at MenuMan+0x80 (DS1 Overhaul / community).
  if (menu_man_ptr_ && IsReadable(menu_man_ptr_)) {
    const auto menu_man = ReadT<std::uintptr_t>(menu_man_ptr_);
    if (menu_man && IsReadable(menu_man + 0x104)) {
      s.banner_message = ReadT<std::uint8_t>(menu_man + 0x104);
      s.banner_valid = true;
    }
    if (menu_man && IsReadable(menu_man + 0x80)) {
      s.on_title_screen = (ReadT<std::int32_t>(menu_man + 0x80) == 1);
    }
  }

  // Boss fights via event flags (EMEVD), not lock-on.
  if (g_event_flags.Ready() && s.in_gameplay) {
    std::unordered_set<std::int32_t> cheer_seen;
    std::unordered_set<std::int32_t> defeat_seen;
    for (const auto& pair : kBossPairs) {
      bool cheer = false;
      bool defeated = false;
      if (!g_event_flags.ReadFlag(pair.cheer_flag, &cheer)) {
        continue;
      }
      if (!g_event_flags.ReadFlag(pair.defeat_flag, &defeated)) {
        continue;
      }
      if (cheer && cheer_seen.insert(pair.cheer_flag).second) {
        s.cheer_flags_on.push_back(pair.cheer_flag);
      }
      if (defeated && defeat_seen.insert(pair.defeat_flag).second) {
        s.defeat_flags_on.push_back(pair.defeat_flag);
      }
      // Active fight: bar/entry flag on and boss not yet defeated.
      if (cheer && !defeated && !s.boss_fight_active) {
        s.boss_fight_active = true;
        s.boss_defeat_flag = pair.defeat_flag;
        s.boss_cheer_flag = pair.cheer_flag;
      }
    }
  }

  return s;
}

}  // namespace sitcom
