#pragma once
#include <filesystem>

namespace phantomcrash {
// Modal "Save Data Location" dialog: shows the folder that holds the settings,
// the game in progress and the save slots, with a button that opens it in
// Explorer. `owner` is the game window's HWND.
void show_save_location(void *owner, const std::filesystem::path &folder);
} // namespace phantomcrash
