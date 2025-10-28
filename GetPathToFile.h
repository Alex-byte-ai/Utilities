#pragma once

#include <filesystem>
#include <optional>

std::optional<std::filesystem::path> SavePath();
std::optional<std::filesystem::path> OpenPath();
