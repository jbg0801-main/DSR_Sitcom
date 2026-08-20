#include "events.h"

#include "log.h"

#include <windows.h>

#include <string>
#include <unordered_set>

namespace sitcom {

void EventDetector::Reset() {
  has_prev_ = false;
  prev_ = {};
  last_laugh_hit_ms_ = 0;
  applause_armed_ = false;
  armed_defeat_flag_ = 0;
  flags_seeded_ = false;
}

void EventDetector::Update(const GameSnapshot& snap, const Config& cfg, Audio& audio) {
  if (!cfg.enabled) {
    return;
  }

  const auto now = static_cast<std::uint64_t>(GetTickCount64());

  if (!has_prev_) {
    prev_ = snap;
    has_prev_ = true;
    if (snap.in_gameplay) {
      flags_seeded_ = true;
    }
    return;
  }

  // Area title card: MenuMan banner → 8 (Current Location).
  // Must NOT require player_valid — title cards often show during loads when HP reads are junk.
  if (cfg.wipe_on_area_title && snap.banner_valid && prev_.banner_valid && snap.area_valid) {
    if (snap.banner_message == kBannerCurrentLocation &&
        prev_.banner_message != kBannerCurrentLocation) {
      LogWrite("event: area title card (banner=Current_Location)");
      audio.Play(SoundCategory::SceneWipe);
    }
  }

  // Boss / laugh logic needs a stable in-world player.
  if (!snap.in_gameplay) {
    if (prev_.in_gameplay) {
      flags_seeded_ = false;
      applause_armed_ = false;
      armed_defeat_flag_ = 0;
    }
    prev_ = snap;
    return;
  }

  if (!flags_seeded_) {
    flags_seeded_ = true;
    prev_ = snap;
    LogWrite("event: seeded flag baselines in gameplay");
    return;
  }

  // Player hit / death — ignore max-HP reshuffles (class/stat edits).
  if (snap.player_valid && prev_.player_valid && prev_.in_gameplay) {
    const bool max_hp_changed = snap.player_max_hp != prev_.player_max_hp;
    if (!max_hp_changed && snap.player_hp < prev_.player_hp) {
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

  // Boss appear: rising edge of cheer flag while that boss is undefeated.
  if (cfg.cheer_on_boss_bar && snap.boss_fight_active && snap.boss_cheer_flag != 0) {
    std::unordered_set<std::int32_t> prev_cheer(prev_.cheer_flags_on.begin(),
                                                prev_.cheer_flags_on.end());
    if (prev_cheer.count(snap.boss_cheer_flag) == 0) {
      LogWrite("event: boss fight start cheer_flag=" + std::to_string(snap.boss_cheer_flag) +
               " defeat_flag=" + std::to_string(snap.boss_defeat_flag));
      audio.Play(SoundCategory::Cheer);
      applause_armed_ = true;
      armed_defeat_flag_ = snap.boss_defeat_flag;
    }
  }

  // Boss death: defeat event flag OFF → ON.
  if (cfg.applause_on_boss_death) {
    std::unordered_set<std::int32_t> prev_def(prev_.defeat_flags_on.begin(),
                                              prev_.defeat_flags_on.end());
    for (const auto id : snap.defeat_flags_on) {
      if (prev_def.count(id) != 0) {
        continue;
      }
      LogWrite("event: boss defeated flag=" + std::to_string(id));
      audio.Play(SoundCategory::Applause);
      if (armed_defeat_flag_ == id) {
        applause_armed_ = false;
        armed_defeat_flag_ = 0;
      }
    }
  }

  prev_ = snap;
}

}  // namespace sitcom
