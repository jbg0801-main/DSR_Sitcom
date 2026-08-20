#include "config.h"

#include "paths.h"

#include <windows.h>

namespace sitcom {
namespace {

bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback,
              const std::wstring& path) {
  return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, path.c_str()) != 0;
}

int ReadInt(const wchar_t* section, const wchar_t* key, int fallback,
            const std::wstring& path) {
  return GetPrivateProfileIntW(section, key, fallback, path.c_str());
}

float ReadFloat(const wchar_t* section, const wchar_t* key, float fallback,
                const std::wstring& path) {
  wchar_t buf[64];
  GetPrivateProfileStringW(section, key, nullptr, buf, 64, path.c_str());
  if (buf[0] == L'\0') {
    return fallback;
  }
  return static_cast<float>(_wtof(buf));
}

std::string ReadString(const wchar_t* section, const wchar_t* key, const char* fallback,
                       const std::wstring& path) {
  wchar_t buf[260];
  GetPrivateProfileStringW(section, key, Utf8ToWide(fallback).c_str(), buf, 260, path.c_str());
  return WideToUtf8(buf);
}

}  // namespace

bool LoadConfig(const std::wstring& ini_path, Config& out) {
  Config cfg;
  cfg.enabled = ReadBool(L"sitcom", L"enabled", true, ini_path);
  cfg.log = ReadBool(L"sitcom", L"log", false, ini_path);
  cfg.poll_hz = ReadInt(L"sitcom", L"poll_hz", 20, ini_path);
  if (cfg.poll_hz < 1) {
    cfg.poll_hz = 1;
  }
  if (cfg.poll_hz > 60) {
    cfg.poll_hz = 60;
  }
  cfg.volume = ReadFloat(L"sitcom", L"volume", 0.7f, ini_path);
  if (cfg.volume < 0.f) {
    cfg.volume = 0.f;
  }
  if (cfg.volume > 1.f) {
    cfg.volume = 1.f;
  }

  cfg.laugh_on_hit = ReadBool(L"events", L"laugh_on_hit", true, ini_path);
  cfg.laugh_on_death = ReadBool(L"events", L"laugh_on_death", true, ini_path);
  cfg.cheer_on_boss_bar = ReadBool(L"events", L"cheer_on_boss_bar", true, ini_path);
  cfg.applause_on_boss_death = ReadBool(L"events", L"applause_on_boss_death", true, ini_path);
  cfg.wipe_on_area_title = ReadBool(L"events", L"wipe_on_area_title", true, ini_path);

  cfg.laugh_hit_seconds = ReadFloat(L"cooldowns", L"laugh_hit_seconds", 1.5f, ini_path);
  cfg.sounds_dir = ReadString(L"paths", L"sounds_dir", "sounds", ini_path);

  out = cfg;
  return true;
}

}  // namespace sitcom
