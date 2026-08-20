#pragma once

#include "audio.h"
#include "config.h"
#include "game_state.h"

#include <cstdint>

namespace sitcom {

class EventDetector {
 public:
  void Reset();
  void Update(const GameSnapshot& snap, const Config& cfg, Audio& audio);

 private:
  bool has_prev_ = false;
  GameSnapshot prev_{};
  std::uint64_t last_laugh_hit_ms_ = 0;
  bool applause_armed_ = false;
  std::int32_t armed_defeat_flag_ = 0;

  // After leaving gameplay (load/menu), re-seed baselines so existing flags don't "rise".
  bool flags_seeded_ = false;
};

}  // namespace sitcom
