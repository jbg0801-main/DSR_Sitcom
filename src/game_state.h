#pragma once

#include <cstdint>
#include <vector>

namespace sitcom {

// MenuMan+0x104 "Message" / Display Banner type (DSR-Gadget / RemasterCETable).
constexpr std::uint8_t kBannerNone = 0;
constexpr std::uint8_t kBannerCurrentLocation = 8;  // area title card ("Map Name")
constexpr std::uint8_t kBannerBonfireLit = 13;

struct GameSnapshot {
  bool player_valid = false;
  bool in_gameplay = false;  // valid vitals + real area (not 255 menu/load)
  std::int32_t player_hp = 0;
  std::int32_t player_max_hp = 0;
  std::int32_t deaths = 0;

  // Boss fight currently "up" (HP bar / room-entry event flag), keyed by defeat flag.
  bool boss_fight_active = false;
  std::int32_t boss_defeat_flag = 0;
  std::int32_t boss_cheer_flag = 0;

  std::vector<std::int32_t> defeat_flags_on;
  std::vector<std::int32_t> cheer_flags_on;

  bool area_valid = false;
  std::uint8_t area_number = 0;
  std::uint8_t world_number = 0;

  // Active Display Banner ID (0 = none). 8 = Current Location title card.
  bool banner_valid = false;
  std::uint8_t banner_message = 0;

  // Title / PRESS START style main menu (MenuMan+0x80 == 1).
  bool on_title_screen = false;
};

class GameState {
 public:
  bool Init();
  void Shutdown();
  GameSnapshot Read();

 private:
  bool ResolveBases();

  std::uintptr_t base_b_ptr_ = 0;
  std::uintptr_t base_e_ptr_ = 0;
  std::uintptr_t menu_man_ptr_ = 0;  // slot → *(slot) = MenuMan object
  bool ready_ = false;
};

}  // namespace sitcom
