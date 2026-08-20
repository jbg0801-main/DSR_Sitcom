#include "audio.h"
#include "config.h"
#include "dinput8_proxy.h"
#include "events.h"
#include "game_state.h"
#include "log.h"
#include "paths.h"

#include <windows.h>

#include <atomic>
#include <string>
#include <thread>

namespace {

HMODULE g_module = nullptr;
HMODULE g_real_dinput = nullptr;
std::atomic<bool> g_running{false};
std::thread g_worker;
HANDLE g_stop_event = nullptr;

void WorkerMain() {
  using namespace sitcom;

  // Let the game finish early init before touching patterns / audio.
  WaitForSingleObject(g_stop_event, 5000);
  if (!g_running) {
    return;
  }

  const std::wstring dll_dir = GetDllDirectory();
  const std::wstring sitcom_dir = JoinPath(dll_dir, L"sitcom");
  const std::wstring ini_path = JoinPath(sitcom_dir, L"config.ini");

  Config cfg;
  LoadConfig(ini_path, cfg);
  LogInit(JoinPath(sitcom_dir, L"sitcom.log"), cfg.log);
  LogWrite("worker: start");
  LogWrite("worker: dll_dir=" + WideToUtf8(dll_dir));

  if (!cfg.enabled) {
    LogWrite("worker: disabled via config");
    return;
  }

  Audio audio;
  const std::wstring sounds = JoinPath(sitcom_dir, Utf8ToWide(cfg.sounds_dir));
  if (!audio.Init(sounds, cfg.volume)) {
    LogWrite("worker: audio init failed");
    return;
  }

  GameState game;
  int failures = 0;
  while (g_running && !game.Init()) {
    LogWrite("worker: waiting for game patterns...");
    if (WaitForSingleObject(g_stop_event, 2000) != WAIT_TIMEOUT) {
      audio.Shutdown();
      return;
    }
    if (++failures > 60) {
      LogWrite("worker: giving up resolving patterns");
      audio.Shutdown();
      return;
    }
  }

  EventDetector events;
  const DWORD poll_ms = static_cast<DWORD>(1000 / cfg.poll_hz);
  LogWrite("worker: polling");

  while (g_running) {
    // Hot-reload volume / toggles lightly each second-ish is overkill; read snapshot only.
    const GameSnapshot snap = game.Read();
    events.Update(snap, cfg, audio);
    if (WaitForSingleObject(g_stop_event, poll_ms) != WAIT_TIMEOUT) {
      break;
    }
  }

  game.Shutdown();
  audio.Shutdown();
  LogWrite("worker: stop");
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
  switch (reason) {
    case DLL_PROCESS_ATTACH: {
      DisableThreadLibraryCalls(hModule);
      g_module = hModule;
      g_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);

      DirectInput8Create_t create = nullptr;
      if (!LoadRealDinput8(&g_real_dinput, &create)) {
        return FALSE;
      }

      g_running = true;
      g_worker = std::thread(WorkerMain);
      break;
    }
    case DLL_PROCESS_DETACH: {
      g_running = false;
      if (g_stop_event) {
        SetEvent(g_stop_event);
      }
      if (g_worker.joinable()) {
        g_worker.join();
      }
      UnloadRealDinput8(g_real_dinput);
      g_real_dinput = nullptr;
      if (g_stop_event) {
        CloseHandle(g_stop_event);
        g_stop_event = nullptr;
      }
      break;
    }
    default:
      break;
  }
  return TRUE;
}
