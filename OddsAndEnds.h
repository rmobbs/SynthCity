#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>
#include <string>

std::shared_ptr<WCHAR[]> StringToWChar(const std::string& sourceString);
std::shared_ptr<WCHAR[]> StringToWChar(const std::string_view& sourceString);

