#pragma once
#include <filesystem>

namespace phantomcrash {
// The settings file, which also holds the game in progress and the save
// slots. It lives in `<root>/Saves`, beside the game data; the S.L.A.I.
// profile, the log and scene dumps sit next to it. The first run there copies
// the settings files from the old per-user folder (%APPDATA%\PhantomCrash\
// PhantomCrash Port), which is left as it was. If Saves cannot be created,
// the per-user folder is used.
std::filesystem::path default_settings_path(const std::filesystem::path &root);
} // namespace phantomcrash
