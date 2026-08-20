#pragma once

#include <string>

namespace sitcom {

struct Config {
  bool enabled = true;
  bool log = false;
  int poll_hz = 20;
  float volume = 0.7f;

  bool laugh_on_hit = true;
  bool laugh_on_death = true;
  bool cheer_on_boss_bar = true;
  bool applause_on_boss_death = true;
  bool wipe_on_area_title = true;

  float laugh_hit_seconds = 1.5f;
  std::string sounds_dir = "sounds";
};

bool LoadConfig(const std::wstring& ini_path, Config& out);

}  // namespace sitcom
