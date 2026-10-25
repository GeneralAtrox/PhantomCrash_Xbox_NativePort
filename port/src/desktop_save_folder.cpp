#include "desktop_save_folder.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <string>
#include <system_error>

namespace phantomcrash {
namespace {

constexpr const char *settings_name = "settings.json";

// The folder saves used before they moved beside the game data.
std::optional<std::filesystem::path> per_user_folder(const bool create) {
#ifdef _WIN32
  if (!create) {
    const char *appdata = SDL_getenv("APPDATA");
    if (appdata == nullptr) {
      return std::nullopt;
    }
    return std::filesystem::path(reinterpret_cast<const char8_t *>(appdata)) / "PhantomCrash" /
           "PhantomCrash Port";
  }
#endif
  // SDL creates the folder it names.
  char *folder = SDL_GetPrefPath("PhantomCrash", "PhantomCrash Port");
  if (folder == nullptr) {
    return std::nullopt;
  }
  std::filesystem::path path(reinterpret_cast<const char8_t *>(folder));
  SDL_free(folder);
  return path;
}

// Copies settings.json and its companions (settings.slai.json and the
// backups beside them) without replacing anything already in `to`.
void copy_old_saves(const std::filesystem::path &from, const std::filesystem::path &to) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(from / settings_name, error)) {
    return;
  }
  for (const auto &entry : std::filesystem::directory_iterator(from, error)) {
    const std::string name = entry.path().filename().string();
    if (!entry.is_regular_file(error) || !name.starts_with("settings") ||
        entry.path().extension() != ".json") {
      continue;
    }
    std::filesystem::copy_file(entry.path(), to / entry.path().filename(),
                               std::filesystem::copy_options::skip_existing, error);
    if (error) {
      SDL_Log("Unable to copy %s to %s: %s", entry.path().string().c_str(), to.string().c_str(),
              error.message().c_str());
    }
  }
  SDL_Log("Copied saves from %s to %s", from.string().c_str(), to.string().c_str());
}

} // namespace

std::filesystem::path default_settings_path(const std::filesystem::path &root) {
  const std::filesystem::path folder = root / "Saves";
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  if (error) {
    if (const auto fallback = per_user_folder(true)) {
      return *fallback / settings_name;
    }
    return settings_name;
  }
  const std::filesystem::path path = folder / settings_name;
  if (!std::filesystem::exists(path, error)) {
    if (const auto old = per_user_folder(false)) {
      copy_old_saves(*old, folder);
    }
  }
  return path;
}

} // namespace phantomcrash
