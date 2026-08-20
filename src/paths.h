#pragma once

#include <string>

namespace sitcom {

std::wstring GetDllDirectory();
std::wstring JoinPath(const std::wstring& a, const std::wstring& b);
std::string WideToUtf8(const std::wstring& w);
std::wstring Utf8ToWide(const std::string& s);

}  // namespace sitcom
