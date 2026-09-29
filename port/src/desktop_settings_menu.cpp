#include "desktop_settings_menu.hpp"

#include "desktop_frame_rate_dialog.hpp"
#include "desktop_controls_dialog.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <commctrl.h>
#endif

namespace phantomcrash {

#ifdef _WIN32
struct SettingsMenu::Impl {
  static constexpr UINT fullscreen_command = 0x7200;
  static constexpr UINT vsync_command = 0x7201;
  static constexpr UINT frame_cap_command = 0x7202;
  static constexpr UINT frame_counter_command = 0x7203;
  static constexpr UINT pause_unfocused_command = 0x7204;
  static constexpr UINT controls_command = 0x7205;
  static constexpr UINT first_level_command = 0x7300;
  static constexpr UINT last_level_command = 0x73ff;

  Settings settings;
  bool interacted{};
  bool visible{};
  std::optional<int> level;
  HWND window{};
  HMENU bar{}, menu{}, levels{};

  void check(UINT command, bool on) const {
    CheckMenuItem(menu, command, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED));
  }
  void refresh() const {
    check(fullscreen_command, settings.fullscreen);
    check(vsync_command, settings.frame_rate.vsync);
    check(frame_counter_command, settings.frame_counter);
    check(pause_unfocused_command, settings.pause_unfocused);
    const std::wstring label =
        settings.frame_rate.cap_enabled
            ? L"Frame &Rate Cap: " + std::to_wstring(settings.frame_rate.cap) + L" FPS..."
            : std::wstring(L"Frame &Rate Cap: Off...");
    ModifyMenuW(menu, frame_cap_command, MF_BYCOMMAND | MF_STRING, frame_cap_command,
                label.c_str());
  }
  bool command(UINT id) {
    if (id >= first_level_command && id <= last_level_command) {
      level = static_cast<int>(id - first_level_command);
      interacted = true;
      return true;
    }
    switch (id) {
    case fullscreen_command:
      settings.fullscreen = !settings.fullscreen;
      break;
    case vsync_command:
      settings.frame_rate.vsync = !settings.frame_rate.vsync;
      break;
    case frame_counter_command:
      settings.frame_counter = !settings.frame_counter;
      break;
    case pause_unfocused_command:
      settings.pause_unfocused = !settings.pause_unfocused;
      break;
    case frame_cap_command:
      edit_frame_rate_cap(window, settings.frame_rate);
      break;
    case controls_command:
      edit_host_controls(window, settings.host_controls);
      break;
    default:
      return false;
    }
    refresh();
    interacted = true;
    return true;
  }
  // Shows or hides the bar without changing the size of the game's client area.
  void show_bar(bool show) {
    if (show == visible) {
      return;
    }
    visible = show;
    RECT client{};
    GetClientRect(window, &client);
    SetMenu(window, show ? bar : nullptr);
    if (IsZoomed(window) || (GetWindowLongPtrW(window, GWL_STYLE) & WS_POPUP)) {
      return;
    }
    AdjustWindowRectEx(&client, static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)), show,
                       static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)));
    SetWindowPos(window, nullptr, 0, 0, client.right - client.left, client.bottom - client.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
  }
  static LRESULT CALLBACK message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                  UINT_PTR id, DWORD_PTR data) {
    auto &self = *reinterpret_cast<Impl *>(data);
    if (message == WM_EXITMENULOOP) {
      self.interacted = true;
    }
    if (message == WM_COMMAND && HIWORD(wparam) == 0 && lparam == 0 &&
        self.command(LOWORD(wparam))) {
      return 0;
    }
    if (message == WM_NCDESTROY) {
      RemoveWindowSubclass(hwnd, &Impl::message, id);
      self.window = nullptr;
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
  }
  ~Impl() {
    if (window) {
      RemoveWindowSubclass(window, &Impl::message, 1);
      SetMenu(window, nullptr);
    }
    if (bar) {
      DestroyMenu(bar);
    }
  }
};

SettingsMenu::SettingsMenu(void *native_window, const Settings &initial)
    : impl_(std::make_unique<Impl>()) {
  auto &state = *impl_;
  state.settings = initial;
  auto *hwnd = static_cast<HWND>(native_window);
  if (hwnd == nullptr) {
    return;
  }
  state.bar = CreateMenu();
  state.menu = CreatePopupMenu();
  if (!state.bar || !state.menu ||
      !AppendMenuW(state.bar, MF_POPUP, reinterpret_cast<UINT_PTR>(state.menu), L"&Settings")) {
    if (state.menu) {
      DestroyMenu(state.menu);
    }
    throw std::runtime_error("Unable to create the Settings menu");
  }
  if (!AppendMenuW(state.menu, MF_STRING, Impl::fullscreen_command, L"&Fullscreen\tF11") ||
      !AppendMenuW(state.menu, MF_SEPARATOR, 0, nullptr) ||
      !AppendMenuW(state.menu, MF_STRING, Impl::vsync_command, L"&VSync") ||
      !AppendMenuW(state.menu, MF_STRING, Impl::frame_cap_command, L"Frame &Rate Cap...") ||
      !AppendMenuW(state.menu, MF_STRING, Impl::frame_counter_command, L"Frame Rate &Counter") ||
      !AppendMenuW(state.menu, MF_SEPARATOR, 0, nullptr) ||
      !AppendMenuW(state.menu, MF_STRING, Impl::controls_command, L"&Keyboard / Controller Controls...") ||
      !AppendMenuW(state.menu, MF_STRING, Impl::pause_unfocused_command,
                   L"&Pause When Unfocused")) {
    throw std::runtime_error("Unable to fill the Settings menu");
  }
  HMENU tools = CreatePopupMenu();
  state.levels = CreatePopupMenu();
  if (!tools || !state.levels ||
      !AppendMenuW(tools, MF_POPUP, reinterpret_cast<UINT_PTR>(state.levels), L"&Level Select") ||
      !AppendMenuW(state.bar, MF_POPUP, reinterpret_cast<UINT_PTR>(tools), L"&Tools")) {
    throw std::runtime_error("Unable to create the Tools menu");
  }
  state.refresh();
  if (!SetWindowSubclass(hwnd, &Impl::message, 1, reinterpret_cast<DWORD_PTR>(&state))) {
    throw std::runtime_error("Unable to connect the Settings menu");
  }
  state.window = hwnd;
}

SettingsMenu::~SettingsMenu() = default;

const Settings &SettingsMenu::settings() const {
  return impl_->settings;
}

void SettingsMenu::set_fullscreen(const bool enabled) {
  impl_->settings.fullscreen = enabled;
  if (impl_->menu) {
    impl_->refresh();
  }
}

void SettingsMenu::set_visible(const bool visible) {
  if (impl_->window) {
    impl_->show_bar(visible);
  }
}

bool SettingsMenu::take_interaction() {
  return std::exchange(impl_->interacted, false);
}

void SettingsMenu::set_levels(const std::vector<std::string> &labels) {
  HMENU levels = impl_->levels;
  if (levels == nullptr) {
    return;
  }
  while (GetMenuItemCount(levels) > 0) {
    DeleteMenu(levels, 0, MF_BYPOSITION);
  }
  const auto count = std::min<std::size_t>(labels.size(), Impl::last_level_command -
                                                              Impl::first_level_command + 1);
  for (std::size_t index = 0; index < count; ++index) {
    const std::string &label = labels[index];
    const int size =
        MultiByteToWideChar(CP_UTF8, 0, label.data(), static_cast<int>(label.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, label.data(), static_cast<int>(label.size()), wide.data(),
                        size);
    AppendMenuW(levels, MF_STRING, Impl::first_level_command + static_cast<UINT>(index),
                wide.c_str());
  }
  if (count == 0) {
    AppendMenuW(levels, MF_STRING | MF_GRAYED, 0, L"(none)");
  }
  if (impl_->window != nullptr) {
    DrawMenuBar(impl_->window);
  }
}

std::optional<int> SettingsMenu::take_level() {
  return std::exchange(impl_->level, std::nullopt);
}
#else
struct SettingsMenu::Impl {
  Settings settings;
};

SettingsMenu::SettingsMenu(void *, const Settings &initial) : impl_(std::make_unique<Impl>()) {
  impl_->settings = initial;
}
SettingsMenu::~SettingsMenu() = default;
const Settings &SettingsMenu::settings() const {
  return impl_->settings;
}
void SettingsMenu::set_fullscreen(const bool enabled) {
  impl_->settings.fullscreen = enabled;
}
void SettingsMenu::set_visible(bool) {}
bool SettingsMenu::take_interaction() {
  return false;
}
void SettingsMenu::set_levels(const std::vector<std::string> &) {}
std::optional<int> SettingsMenu::take_level() {
  return std::nullopt;
}
#endif

} // namespace phantomcrash
