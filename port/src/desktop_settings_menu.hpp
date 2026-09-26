#pragma once

#include "phantomcrash/settings.hpp"

#include <memory>

namespace phantomcrash {

// The window's Settings menu (after the Lodoss stage menu): Fullscreen,
// VSync, Frame Rate Cap..., Frame Rate Counter and Pause When Unfocused.
// Commands arrive through the window procedure; the game loop reads
// settings() each frame. Windows only; elsewhere it holds the settings.
class SettingsMenu final {
public:
  SettingsMenu(void *native_window, const Settings &initial);
  ~SettingsMenu();
  SettingsMenu(const SettingsMenu &) = delete;
  SettingsMenu &operator=(const SettingsMenu &) = delete;

  [[nodiscard]] const Settings &settings() const;
  // Keyboard shortcuts change settings through here so the menu stays in step.
  void set_fullscreen(bool enabled);
  // The menu bar is hidden in fullscreen.
  void set_visible(bool visible);
  // True once after the user interacts with the menu, so the loop can drop
  // the time spent inside the menu instead of catching up.
  bool take_interaction();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace phantomcrash
