#include "fmod_volume.h"

#include "log.h"

namespace sitcom {

// FMOD inline hooks + early LoadLibrary/pointer probes crashed the game on boot
// under Proton. Volume sync is disabled until we have a non-invasive reader.
// Final gain falls back to config.ini `volume` only (game SFX factor = 1.0).

bool FmodVolumeInit() {
  static bool once = false;
  if (!once) {
    once = true;
    LogWrite("fmod: SE sync disabled for stability (config volume only)");
  }
  return false;
}

void FmodVolumeShutdown() {}

bool FmodTryGetSfxVolume(float* out_volume) {
  (void)out_volume;
  return false;
}

}  // namespace sitcom
