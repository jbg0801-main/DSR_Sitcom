#include "events.h"

#include "log.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace sitcom {

void EventDetector::Reset() {
  has_prev_ = false;
  prev_ = {};
  last_laugh_hit_ms_ = 0;
  applause_armed_ = false;
  armed_defeat_flag_ = 0;
  seen_area_ = false;
  last_area_ = 0;
  last_world_ = 0;
}

void EventDetector::Update(const GameSnapshot& snap, const Config& cfg, Audio& audio) {
  if (!cfg.enabled) {
    return;
  }

  const auto now = static_cast<std::uint64_t>(GetTickCount64());

  if (!has_prev_) {
    prev_ = snap;
    has_prev_ = true;
    if (snap.area_valid) {
      seen_area_ = true;
      last_area_ = snap.area_number;
      last_world_ = snap.world_number;
    }
    return;
  }

  // Player hit / death
  if (snap.player_valid && prev_.player_valid) {
    if (snap.player_hp < prev_.player_hp) {
      if (snap.player_hp <= 0) {
        if (cfg.laugh_on_death) {
          LogWrite("event: player death");
          audio.Play(SoundCategory::Laugh);
        }
      } else if (cfg.laugh_on_hit) {
        const auto cooldown_ms = static_cast<std::uint64_t>(cfg.laugh_hit_seconds * 1000.0f);
        if (now - last_laugh_hit_ms_ >= cooldown_ms) {
          LogWrite("event: player hit");
          audio.Play(SoundCategory::Laugh);
          last_laugh_hit_ms_ = now;
        }
      }
    } else if (cfg.laugh_on_death && snap.deaths > prev_.deaths) {
      LogWrite("event: death counter increased");
      audio.Play(SoundCategory::Laugh);
    }
  }

  // Boss appear: rising edge of "fight active" (EMEVD bar/entry flag while undefeated).
  // One cheer per engagement; leaving the arena / dying clears the flag so fog re-entry cheers again.
  if (cfg.cheer_on_boss_bar && snap.boss_fight_active && !prev_.boss_fight_active) {
    LogWrite("event: boss fight start defeat_flag=" + std::to_string(snap.boss_defeat_flag));
    audio.Play(SoundCategory::Cheer);
    applause_armed_ = true;
    armed_defeat_flag_ = snap.boss_defeat_flag;
  }

  if (!snap.boss_fight_active && prev_.boss_fight_active && !applause_armed_) {
    // left without kill — ready for next fog entry
  }

  // Boss death: defeat event flag OFF → ON (vanilla EMEVD).
  if (cfg.applause_on_boss_death) {
    std::unordered_set<std::int32_t> prev_def(prev_.defeat_flags_on.begin(),
                                              prev_.defeat_flags_on.end());
    for (const auto id : snap.defeat_flags_on) {
      if (prev_def.count(id) == 0) {
        LogWrite("event: boss defeated flag=" + std::to_string(id));
        audio.Play(SoundCategory::Applause);
        if (armed_defeat_flag_ == id) {
          applause_armed_ = false;
          armed_defeat_flag_ = 0;
        }
      }
    }
  }

  if (!snap.boss_fight_active) {
    // If we left the fight without a defeat flag edge, drop arm so we don't applaud later wrongly.
    if (!applause_armed_) {
      armed_defeat_flag_ = 0;
    }
  }

  // Area title card proxy: area/world number change while in a loaded character.
  if (cfg.wipe_on_area_title && snap.player_valid && snap.area_valid) {
    if (!seen_area_) {
      seen_area_ = true;
      last_area_ = snap.area_number;
      last_world_ = snap.world_number;
    } else if (snap.area_number != last_area_ || snap.world_number != last_world_) {
      LogWrite("event: area change " + std::to_string(last_area_) + "/" +
               std::to_string(last_world_) + " -> " + std::to_string(snap.area_number) + "/" +
               std::to_string(snap.world_number));
      audio.Play(SoundCategory::SceneWipe);
      last_area_ = snap.area_number;
      last_world_ = snap.world_number;
    }
  }

  prev_ = snap;
}

}  // namespace sitcom
