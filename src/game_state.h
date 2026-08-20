#pragma once

#include <cstdint>

namespace sitcom {

struct GameSnapshot {
  bool player_valid = false;
  std::int32_t player_hp = 0;
  std::int32_t player_max_hp = 0;
  std::int32_t deaths = 0;

  bool boss_bar_active = false;
  std::int32_t boss_hp = 0;
  std::int32_t boss_max_hp = 0;
  std::int32_t boss_char_id = 0;

  bool area_valid = false;
  std::uint8_t area_number = 0;
  std::uint8_t world_number = 0;
};

class GameState {
 public:
  bool Init();
  void Shutdown();
  GameSnapshot Read();

 private:
  bool ResolveBases();

  std::uintptr_t base_b_ptr_ = 0;  // pointer-to-pointer (RIP slot)
  std::uintptr_t base_e_ptr_ = 0;
  bool ready_ = false;
};

}  // namespace sitcom
