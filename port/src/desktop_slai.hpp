#pragma once
#include "phantomcrash/settings.hpp"
#include <filesystem>
namespace phantomcrash {
class DataRoots;
class SettingsMenu;
// The one settings file base game and S.L.A.I. share: the old per-mode
// files' games brought in once, and what the loaded content cannot show set
// aside (and what it can given back).
Settings load_shared_settings(const DataRoots &roots, const std::filesystem::path &configuration,
                              bool slai, bool smoke);
bool prepare_slai_data(const std::filesystem::path &root, bool interactive);
void update_slai_menu(SettingsMenu &menu, const std::filesystem::path &root,
                      const std::filesystem::path &configuration);
} // namespace phantomcrash
