#include "events.h"

#include "log.h"

#include <windows.h>

#include <string>

namespace sitcom {

void EventDetector::Reset() {
  has_prev_ = false;
  prev_ = {};
  last_laugh_hit_ms_ = 0;
  applause_armed_ = false;
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

  // Boss bar / boss death
  if (snap.boss_bar_active && !prev_.boss_bar_active) {
    applause_armed_ = true;
    if (cfg.cheer_on_boss_bar) {
      LogWrite("event: boss bar appear id=" + std::to_string(snap.boss_char_id));
      audio.Play(SoundCategory::Cheer);
    }
  }

  if (applause_armed_ && prev_.boss_bar_active) {
    const bool died = (snap.boss_hp <= 0 && prev_.boss_hp > 0) ||
                      (!snap.boss_bar_active && prev_.boss_hp > 0);
    if (died) {
      if (cfg.applause_on_boss_death) {
        LogWrite("event: boss death");
        audio.Play(SoundCategory::Applause);
      }
      applause_armed_ = false;
    }
  }

  if (!snap.boss_bar_active && !prev_.boss_bar_active) {
    // Clear arm if we left combat without a kill signal for a while — keep armed only while
    // we had an active bar recently; if bar gone and hp unknown, disarm after lose-lock.
    if (!snap.boss_bar_active && prev_.boss_bar_active == false && snap.boss_hp == 0) {
      // no-op
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
