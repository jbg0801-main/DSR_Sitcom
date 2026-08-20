#include "audio.h"

#include "log.h"
#include "paths.h"

#include <algorithm>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace sitcom {
namespace fs = std::filesystem;

struct Audio::Impl {
  ma_engine engine{};
  bool engine_ok = false;
  float volume = 0.7f;
  std::mt19937 rng{std::random_device{}()};

  std::vector<std::wstring> laugh;
  std::vector<std::wstring> cheer;
  std::vector<std::wstring> applause;
  std::vector<std::wstring> wipe;
  int last_laugh = -1;
  int last_cheer = -1;
  int last_applause = -1;
  int last_wipe = -1;
};

namespace {

void CollectPrefix(const fs::path& dir, const std::string& prefix, std::vector<std::wstring>& out) {
  // Matches laugh.wav, laugh_01.wav, laugh_foo.wav, etc.
  if (!fs::exists(dir)) {
    return;
  }
  for (const auto& entry : fs::directory_iterator(dir)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext != ".wav" && ext != ".WAV") {
      continue;
    }
    const auto stem = entry.path().stem().string();
    if (stem == prefix ||
        (stem.size() > prefix.size() && stem.compare(0, prefix.size(), prefix) == 0 &&
         (stem[prefix.size()] == '_' || stem[prefix.size()] == '-'))) {
      out.push_back(entry.path().wstring());
    }
  }
  std::sort(out.begin(), out.end());
}

int PickIndex(std::mt19937& rng, int count, int last) {
  if (count <= 0) {
    return -1;
  }
  if (count == 1) {
    return 0;
  }
  std::uniform_int_distribution<int> dist(0, count - 1);
  int idx = dist(rng);
  if (idx == last) {
    idx = (idx + 1) % count;
  }
  return idx;
}

}  // namespace

bool Audio::Init(const std::wstring& sounds_dir, float volume) {
  Shutdown();
  impl_ = new Impl();
  impl_->volume = volume;

  if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
    LogWrite("audio: ma_engine_init failed");
    delete impl_;
    impl_ = nullptr;
    return false;
  }
  impl_->engine_ok = true;
  ma_engine_set_volume(&impl_->engine, volume);

  const fs::path dir(sounds_dir);
  CollectPrefix(dir, "laugh", impl_->laugh);
  CollectPrefix(dir, "cheer", impl_->cheer);
  CollectPrefix(dir, "applause", impl_->applause);
  CollectPrefix(dir, "scene_wipe", impl_->wipe);

  LogWrite("audio: loaded laugh=" + std::to_string(impl_->laugh.size()) +
           " cheer=" + std::to_string(impl_->cheer.size()) +
           " applause=" + std::to_string(impl_->applause.size()) +
           " wipe=" + std::to_string(impl_->wipe.size()));
  return true;
}

void Audio::Shutdown() {
  if (!impl_) {
    return;
  }
  if (impl_->engine_ok) {
    ma_engine_uninit(&impl_->engine);
  }
  delete impl_;
  impl_ = nullptr;
}

void Audio::SetVolume(float volume) {
  if (!impl_ || !impl_->engine_ok) {
    return;
  }
  impl_->volume = volume;
  ma_engine_set_volume(&impl_->engine, volume);
}

void Audio::Play(SoundCategory category) {
  if (!impl_ || !impl_->engine_ok) {
    return;
  }

  std::vector<std::wstring>* list = nullptr;
  int* last = nullptr;
  const char* label = "";
  switch (category) {
    case SoundCategory::Laugh:
      list = &impl_->laugh;
      last = &impl_->last_laugh;
      label = "laugh";
      break;
    case SoundCategory::Cheer:
      list = &impl_->cheer;
      last = &impl_->last_cheer;
      label = "cheer";
      break;
    case SoundCategory::Applause:
      list = &impl_->applause;
      last = &impl_->last_applause;
      label = "applause";
      break;
    case SoundCategory::SceneWipe:
      list = &impl_->wipe;
      last = &impl_->last_wipe;
      label = "scene_wipe";
      break;
  }

  const int idx = PickIndex(impl_->rng, static_cast<int>(list->size()), *last);
  if (idx < 0) {
    LogWrite(std::string("audio: no clips for ") + label);
    return;
  }
  *last = idx;
  const std::wstring& path = (*list)[idx];
  const ma_result r = ma_engine_play_sound(&impl_->engine, WideToUtf8(path).c_str(), nullptr);
  if (r != MA_SUCCESS) {
    LogWrite(std::string("audio: play failed for ") + label);
  } else {
    LogWrite(std::string("audio: play ") + label + " #" + std::to_string(idx));
  }
}

}  // namespace sitcom
