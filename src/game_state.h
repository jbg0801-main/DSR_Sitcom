#pragma once

#include <cstdint>
#include <vector>

namespace sitcom {

struct GameSnapshot {
  bool player_valid = false;
  std::int32_t player_hp = 0;
  std::int32_t player_max_hp = 0;
  std::int32_t deaths = 0;

  // Boss fight currently "up" (HP bar / room-entry event flag), keyed by defeat flag.
  bool boss_fight_active = false;
  std::int32_t boss_defeat_flag = 0;  // which fight, if active

  // Defeat flags that flipped ON this frame are reported via events using prev snapshot.
  std::vector<std::int32_t> defeat_flags_on;  // all currently ON (for edge detect)
  std::vector<std::int32_t> cheer_flags_on;   // debug/optional

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

  std::uintptr_t base_b_ptr_ = 0;
  std::uintptr_t base_e_ptr_ = 0;
  bool ready_ = false;
};

}  // namespace sitcom
