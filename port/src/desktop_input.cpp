#include "desktop_input.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace phantomcrash {
namespace {

constexpr Sint16 axis_threshold = 16000;

constexpr std::array<std::pair<SDL_Scancode, Buttons>, 14> keys{{
    {SDL_SCANCODE_RETURN, button::start},
    {SDL_SCANCODE_KP_ENTER, button::start},
    {SDL_SCANCODE_Z, button::a},
    {SDL_SCANCODE_SPACE, button::a},
    {SDL_SCANCODE_X, button::b},
    {SDL_SCANCODE_BACKSPACE, button::b},
    {SDL_SCANCODE_C, button::x},
    {SDL_SCANCODE_V, button::y},
    {SDL_SCANCODE_ESCAPE, button::back},
    {SDL_SCANCODE_UP, button::up},
    {SDL_SCANCODE_DOWN, button::down},
    {SDL_SCANCODE_LEFT, button::left},
    {SDL_SCANCODE_RIGHT, button::right},
    {SDL_SCANCODE_Q, button::left_trigger},
}};

constexpr std::array<std::pair<SDL_GamepadButton, Buttons>, 14> pad_buttons{{
    {SDL_GAMEPAD_BUTTON_SOUTH, button::a},
    {SDL_GAMEPAD_BUTTON_EAST, button::b},
    {SDL_GAMEPAD_BUTTON_WEST, button::x},
    {SDL_GAMEPAD_BUTTON_NORTH, button::y},
    {SDL_GAMEPAD_BUTTON_START, button::start},
    {SDL_GAMEPAD_BUTTON_BACK, button::back},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, button::white},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, button::black},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, button::up},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, button::down},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, button::left},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, button::right},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, button::left_thumb},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, button::right_thumb},
}};

Buttons key_button(const SDL_Scancode scancode) {
  if (scancode == SDL_SCANCODE_E) {
    return button::right_trigger;
  }
  for (const auto &[key, mask] : keys) {
    if (key == scancode) {
      return mask;
    }
  }
  return 0;
}

Buttons pad_button(const Uint8 value) {
  for (const auto &[pad, mask] : pad_buttons) {
    if (pad == value) {
      return mask;
    }
  }
  return 0;
}

// Triggers and the left stick as digital buttons.
Buttons pad_axes(SDL_Gamepad *pad) {
  Buttons held = 0;
  const auto axis = [&](SDL_GamepadAxis which) { return SDL_GetGamepadAxis(pad, which); };
  held |= axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > axis_threshold ? button::left_trigger : 0;
  held |= axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > axis_threshold ? button::right_trigger : 0;
  held |= axis(SDL_GAMEPAD_AXIS_LEFTY) < -axis_threshold ? button::up : 0;
  held |= axis(SDL_GAMEPAD_AXIS_LEFTY) > axis_threshold ? button::down : 0;
  held |= axis(SDL_GAMEPAD_AXIS_LEFTX) < -axis_threshold ? button::left : 0;
  held |= axis(SDL_GAMEPAD_AXIS_LEFTX) > axis_threshold ? button::right : 0;
  return held;
}

} // namespace

DesktopInput::~DesktopInput() {
  for (auto *pad : gamepads_) {
    SDL_CloseGamepad(pad);
  }
}

void DesktopInput::handle(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
    // Alt combinations belong to the window (Alt+Enter, Alt+F4).
    if (!event.key.repeat && !(event.key.mod & SDL_KMOD_ALT)) {
      latched_ |= key_button(event.key.scancode);
    }
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    latched_ |= pad_button(event.gbutton.button);
    break;
  case SDL_EVENT_GAMEPAD_ADDED:
    if (SDL_Gamepad *pad = SDL_OpenGamepad(event.gdevice.which)) {
      gamepads_.push_back(pad);
    }
    break;
  case SDL_EVENT_GAMEPAD_REMOVED: {
    const auto found = std::ranges::find_if(
        gamepads_, [&](SDL_Gamepad *pad) { return SDL_GetGamepadID(pad) == event.gdevice.which; });
    if (found != gamepads_.end()) {
      SDL_CloseGamepad(*found);
      gamepads_.erase(found);
    }
    break;
  }
  default:
    break;
  }
}

Buttons DesktopInput::held_buttons() const {
  Buttons held = 0;
  int count = 0;
  const bool *keyboard = SDL_GetKeyboardState(&count);
  if (SDL_GetModState() & SDL_KMOD_ALT) {
    keyboard = nullptr;
  }
  for (int scancode = 0; keyboard != nullptr && scancode < count; ++scancode) {
    if (keyboard[scancode]) {
      held |= key_button(static_cast<SDL_Scancode>(scancode));
    }
  }
  for (auto *pad : gamepads_) {
    for (const auto &[which, mask] : pad_buttons) {
      held |= SDL_GetGamepadButton(pad, which) ? mask : 0;
    }
    held |= pad_axes(pad);
  }
  return held;
}

InputFrame DesktopInput::sample(const bool focused) {
  const Buttons held = focused ? held_buttons() : 0;
  // Injected presses do not need focus: smoke runs are hidden.
  const Buttons latched = (focused ? latched_ : 0) | std::exchange(injected_, 0);
  latched_ = 0;
  return edges_.update(held, latched, controllers());
}

} // namespace phantomcrash
