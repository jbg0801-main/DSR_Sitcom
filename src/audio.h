#pragma once

#include <string>

namespace sitcom {

enum class SoundCategory {
  Laugh,
  Cheer,
  Applause,
  SceneWipe,
};

class Audio {
 public:
  bool Init(const std::wstring& sounds_dir, float volume);
  void Shutdown();
  void SetVolume(float volume);
  void Play(SoundCategory category);

 private:
  struct Impl;
  Impl* impl_ = nullptr;
};

}  // namespace sitcom
