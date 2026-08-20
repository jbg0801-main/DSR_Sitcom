#include "game_state.h"

#include "log.h"
#include "target_hook.h"

#include <windows.h>
#include <psapi.h>

#include <cstring>
#include <string>
#include <unordered_set>

namespace sitcom {
namespace {

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

// RIP-relative: addr points at opcode start of `48 8B 05 xx xx xx xx` (or 48 8B 0D...)
std::uintptr_t ResolveRipMov(std::uintptr_t insn) {
  if (!insn) {
    return 0;
  }
  const auto rel = *reinterpret_cast<std::int32_t*>(insn + 3);
  return insn + 7 + rel;
}

const std::unordered_set<std::int32_t> kBossCharIds = {
    2230, 2231, 2232, 2240, 2250, 2320, 2360, 2730, 3230, 3320, 3471,
    4100, 4500, 4510, 5200, 5210, 5220, 5230, 5250, 5260, 5270, 5271,
    5280, 5290, 5320, 5350, 5351, 5370, 5390,
};

bool IsBossCharId(std::int32_t id) {
  // CharacterID field is often stored as the numeric cXXXX id.
  if (kBossCharIds.count(id) != 0) {
    return true;
  }
  // Sometimes stored as 4-digit with different packing; also accept cXXXX * 10 style rarely.
  return false;
}

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

  // GetB — BaseB (player game data / ChrStat)
  // 48 8B 05 ?? ?? ?? ?? 45 33 ED 48 8B F1 48 85 C0
  const std::uint8_t pat_b[] = {0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x45, 0x33, 0xED, 0x48, 0x8B, 0xF1,
                                0x48, 0x85, 0xC0};
  const char mask_b[] = "xxx????xxxxxxxxx";
  const auto hit_b = PatternScan(base, size, pat_b, mask_b);
  if (!hit_b) {
    LogWrite("game_state: BaseB pattern not found");
    return false;
  }
  base_b_ptr_ = ResolveRipMov(hit_b);

  // GetE — BaseE (area / world numbers)
  // 48 8B 05 ?? ?? ?? ?? 48 8B 88 98 0B 00 00 8B 41 3C C3
  const std::uint8_t pat_e[] = {0x48, 0x8B, 0x05, 0,    0,    0,    0,    0x48, 0x8B, 0x88,
                                0x98, 0x0B, 0x00, 0x00, 0x8B, 0x41, 0x3C, 0xC3};
  const char mask_e[] = "xxx????xxxxxxxxxxx";
  const auto hit_e = PatternScan(base, size, pat_e, mask_e);
  if (hit_e) {
    base_e_ptr_ = ResolveRipMov(hit_e);
  } else {
    LogWrite("game_state: BaseE pattern not found (area wipe disabled)");
    base_e_ptr_ = 0;
  }

  char buf[128];
  snprintf(buf, sizeof(buf), "game_state: BaseB*=0x%llX BaseE*=0x%llX",
           static_cast<unsigned long long>(base_b_ptr_),
           static_cast<unsigned long long>(base_e_ptr_));
  LogWrite(buf);
  return base_b_ptr_ != 0;
}

bool GameState::Init() {
  ready_ = ResolveBases();
  if (ready_) {
    InstallTargetHook();
  }
  return ready_;
}

void GameState::Shutdown() {
  RemoveTargetHook();
  ready_ = false;
  base_b_ptr_ = 0;
  base_e_ptr_ = 0;
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
  s.player_valid = (s.player_max_hp > 0 && s.player_max_hp <= 99999 && s.player_hp >= 0 &&
                    s.player_hp <= s.player_max_hp);

  if (base_e_ptr_ && IsReadable(base_e_ptr_)) {
    const auto base_e = ReadT<std::uintptr_t>(base_e_ptr_);
    if (base_e && IsReadable(base_e + 0xA22)) {
      s.area_number = ReadT<std::uint8_t>(base_e + 0xA22);
      s.world_number = ReadT<std::uint8_t>(base_e + 0xA23);
      s.area_valid = true;
    }
  }

  // Boss via last locked target (hook) — HP offsets from Phokz "Last Hit Entity".
  const auto target = GetLockedTargetPtr();
  if (target && IsReadable(target) && IsReadable(target + 0x3EC)) {
    const auto hp = ReadT<std::int32_t>(target + 0x3E8);
    const auto max_hp = ReadT<std::int32_t>(target + 0x3EC);
    const auto char_id = ReadT<std::int32_t>(target + 0xC8);
    if (max_hp > 0 && max_hp <= 999999 && hp >= 0 && hp <= max_hp && IsBossCharId(char_id)) {
      s.boss_bar_active = hp > 0;
      s.boss_hp = hp;
      s.boss_max_hp = max_hp;
      s.boss_char_id = char_id;
    } else if (max_hp >= 2000 && max_hp <= 999999 && hp >= 0 && hp <= max_hp) {
      // Fallback: very high HP locked target likely a boss if ID unknown.
      s.boss_bar_active = hp > 0;
      s.boss_hp = hp;
      s.boss_max_hp = max_hp;
      s.boss_char_id = char_id;
    }
  }

  return s;
}

}  // namespace sitcom
