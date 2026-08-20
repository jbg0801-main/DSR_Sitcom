#include "log.h"

#include <windows.h>

#include <fstream>
#include <mutex>

namespace sitcom {
namespace {

std::mutex g_mu;
std::ofstream g_file;
bool g_enabled = false;

}  // namespace

void LogInit(const std::wstring& log_path, bool enabled) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_enabled = enabled;
  g_file.close();
  if (!enabled) {
    return;
  }
  g_file.open(log_path.c_str(), std::ios::out | std::ios::app);
  if (g_file) {
    g_file << "---- sitcom log start ----\n";
    g_file.flush();
  }
}

void LogWrite(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_enabled || !g_file) {
    return;
  }
  SYSTEMTIME st{};
  GetLocalTime(&st);
  char prefix[64];
  snprintf(prefix, sizeof(prefix), "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond,
           st.wMilliseconds);
  g_file << prefix << line << '\n';
  g_file.flush();
}

bool LogEnabled() {
  return g_enabled;
}

}  // namespace sitcom
