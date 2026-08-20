#pragma once

#include <string>

namespace sitcom {

void LogInit(const std::wstring& log_path, bool enabled);
void LogWrite(const std::string& line);
bool LogEnabled();

}  // namespace sitcom
