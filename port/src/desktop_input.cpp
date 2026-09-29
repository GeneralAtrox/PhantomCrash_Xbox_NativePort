#include "desktop_input.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace phantomcrash {
namespace {

constexpr Sint16 axis_threshold = 16000;

constexpr std::array<std::pair<SDL_Scancode, Buttons>, 26> keys{{
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
    {SDL_SCANCODE_W, button::up},
    {SDL_SCANCODE_S, button::down},
    {SDL_SCANCODE_A, button::left},
    {SDL_SCANCODE_D, button::right},
    {SDL_SCANCODE_Q, button::left_trigger},
    {SDL_SCANCODE_R, button::white},
    {SDL_SCANCODE_F, button::black},
    {SDL_SCANCODE_LSHIFT, button::left_thumb},
    {SDL_SCANCODE_T, button::right_thumb},
    {SDL_SCANCODE_1, button::pad_up}, // the battle camera's views
    {SDL_SCANCODE_2, button::pad_down},
    {SDL_SCANCODE_3, button::pad_left},
    {SDL_SCANCODE_4, button::pad_right},
}};

// Past this, a stick counts as pushed; inside it reads zero.
constexpr float stick_dead_zone = 0.2F;

float stick_axis(const Sint16 raw) {
  const float value = std::clamp(static_cast<float>(raw) / 32767.0F, -1.0F, 1.0F);
  if (std::abs(value) < stick_dead_zone) {
    return 0.0F;
  }
  return (value - std::copysign(stick_dead_zone, value)) / (1.0F - stick_dead_zone);
}

constexpr std::array<std::pair<SDL_GamepadButton, Buttons>, 14> pad_buttons{{
    {SDL_GAMEPAD_BUTTON_SOUTH, button::a},
    {SDL_GAMEPAD_BUTTON_EAST, button::b},
    {SDL_GAMEPAD_BUTTON_WEST, button::x},
    {SDL_GAMEPAD_BUTTON_NORTH, button::y},
    {SDL_GAMEPAD_BUTTON_START, button::start},
    {SDL_GAMEPAD_BUTTON_BACK, button::back},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, button::white},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, button::black},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, button::up | button::pad_up},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, button::down | button::pad_down},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, button::left | button::pad_left},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, button::right | button::pad_right},
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

// A mouse event's window position in render pixels (the window may be
// scaled by the display).
std::array<float, 2> render_pixels(const SDL_WindowID window, const float x, const float y) {
  std::array<float, 2> pixels{x, y};
  if (SDL_Renderer *renderer = SDL_GetRenderer(SDL_GetWindowFromID(window))) {
    SDL_RenderCoordinatesFromWindow(renderer, x, y, &pixels[0], &pixels[1]);
  }
  return pixels;
}

} // namespace

DesktopInput::~DesktopInput() {
  for (auto *pad : gamepads_) {
    SDL_RumbleGamepad(pad, 0, 0, 0);
    SDL_CloseGamepad(pad);
  }
}

void DesktopInput::handle(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN:
    // Alt combinations belong to the window (Alt+Enter, Alt+F4).
    if (!event.key.repeat && !(event.key.mod & SDL_KMOD_ALT)) {
      latched_[0] |= key_button(event.key.scancode);
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    pointer_pixels_ = render_pixels(event.motion.windowID, event.motion.x, event.motion.y);
    pointer_moved_ = true;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (event.button.button == SDL_BUTTON_LEFT) {
      pointer_pixels_ = render_pixels(event.button.windowID, event.button.x, event.button.y);
      clicked_ = true;
    }
    break;
  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
    pointer_pixels_.reset();
    break;
  case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
    const auto found = std::ranges::find_if(
        gamepads_, [&](SDL_Gamepad *pad) { return SDL_GetGamepadID(pad) == event.gbutton.which; });
    const auto pad = static_cast<std::size_t>(found - gamepads_.begin());
    latched_[static_cast<std::size_t>(found != gamepads_.end() ? port_of(pad) : 0)] |=
        pad_button(event.gbutton.button);
    break;
  }
  case SDL_EVENT_GAMEPAD_ADDED:
    if (SDL_Gamepad *pad = SDL_OpenGamepad(event.gdevice.which)) {
      gamepads_.push_back(pad);
      rumbling_.emplace_back();
    }
    break;
  case SDL_EVENT_GAMEPAD_REMOVED: {
    const auto found = std::ranges::find_if(
        gamepads_, [&](SDL_Gamepad *pad) { return SDL_GetGamepadID(pad) == event.gdevice.which; });
    if (found != gamepads_.end()) {
      SDL_CloseGamepad(*found);
      rumbling_.erase(rumbling_.begin() + (found - gamepads_.begin()));
      gamepads_.erase(found);
    }
    break;
  }
  default:
    break;
  }
}

void DesktopInput::press(const Buttons buttons, const int port) {
  const auto at = static_cast<std::size_t>(std::clamp(port, 0, controller_ports - 1));
  injected_[at] |= buttons;
  scripted_ports_[at] = true;
}

void DesktopInput::hold(const Buttons buttons, const int port) {
  const auto at = static_cast<std::size_t>(std::clamp(port, 0, controller_ports - 1));
  injected_held_[at] |= buttons;
  scripted_ports_[at] = true;
}

void DesktopInput::rumble(const std::array<RumbleMotors, controller_ports> &motors) {
  // Each send lasts this long, and is renewed halfway, so a pad the game
  // stops talking to stops by itself.
  constexpr Uint32 rumble_ms = 1000;
  const Uint64 now = SDL_GetTicks();
  const auto speed = [](const float motor) {
    return static_cast<Uint16>(std::clamp(motor, 0.0F, 1.0F) * 65535.0F + 0.5F);
  };
  for (std::size_t pad = 0; pad < gamepads_.size(); ++pad) {
    const RumbleMotors &own = motors[static_cast<std::size_t>(port_of(pad))];
    const RumbleMotors wanted{std::max(own.low, motors.front().low),
                              std::max(own.high, motors.front().high)};
    PadRumble &last = rumbling_[pad];
    const bool running = wanted != RumbleMotors{};
    if (wanted == last.motors && !(running && now - last.sent_ms >= rumble_ms / 2)) {
      continue;
    }
    SDL_RumbleGamepad(gamepads_[pad], speed(wanted.low), speed(wanted.high), rumble_ms);
    last = {wanted, now};
  }
}

int DesktopInput::port_of(const std::size_t pad) const {
  const std::size_t port = gamepads_.size() >= controller_ports ? pad : pad + 1;
  return static_cast<int>(std::min<std::size_t>(port, controller_ports - 1));
}

DesktopInput::PortButtons DesktopInput::held_buttons() const {
  PortButtons held{};
  int count = 0;
  const bool *keyboard = SDL_GetKeyboardState(&count);
  if (SDL_GetModState() & SDL_KMOD_ALT) {
    keyboard = nullptr;
  }
  for (int scancode = 0; keyboard != nullptr && scancode < count; ++scancode) {
    if (keyboard[scancode]) {
      held[0] |= key_button(static_cast<SDL_Scancode>(scancode));
    }
  }
  for (std::size_t index = 0; index < gamepads_.size(); ++index) {
    Buttons &port = held[static_cast<std::size_t>(port_of(index))];
    for (const auto &[which, mask] : pad_buttons) {
      port |= SDL_GetGamepadButton(gamepads_[index], which) ? mask : 0;
    }
    port |= pad_axes(gamepads_[index]);
  }
  return held;
}

// Each port's buttons and presses: the keyboard's port is always connected,
// the others while a gamepad is on them (or a scripted run presses them).
void DesktopInput::fill_ports(const bool focused, const PortButtons &held, InputFrame &frame) {
  for (std::size_t port = 0; port < frame.ports.size(); ++port) {
    PortInput &own = frame.ports[port];
    const Buttons latched = (focused ? latched_[port] : 0) | std::exchange(injected_[port], 0);
    own.held = held[port];
    own.pressed = latched | (held[port] & ~previous_[port]);
    previous_[port] = held[port];
    own.connected = port == 0 || scripted_ports_[port];
  }
  for (std::size_t index = 0; index < gamepads_.size(); ++index) {
    frame.ports[static_cast<std::size_t>(port_of(index))].connected = true;
  }
  latched_ = {};
}

void DesktopInput::sticks(const Buttons held, InputFrame &frame) const {
  for (auto *pad : gamepads_) {
    const std::array<float, 2> left{stick_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX)),
                                    -stick_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY))};
    const std::array<float, 2> right{stick_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX)),
                                     -stick_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY))};
    if (std::hypot(left[0], left[1]) > std::hypot(frame.left_stick[0], frame.left_stick[1])) {
      frame.left_stick = left;
    }
    if (std::hypot(right[0], right[1]) > std::hypot(frame.right_stick[0], frame.right_stick[1])) {
      frame.right_stick = right;
    }
  }
  if (frame.left_stick == std::array<float, 2>{}) {
    // The keys' directions only: the controller's D-pad changes camera views.
    const Buttons moving = held & ~((held & button::pad) >> 8);
    const auto axis = [&](const Buttons minus, const Buttons plus) {
      return ((moving & plus) != 0 ? 1.0F : 0.0F) - ((moving & minus) != 0 ? 1.0F : 0.0F);
    };
    frame.left_stick = {axis(button::left, button::right), axis(button::down, button::up)};
  }
  int count = 0;
  const bool *keyboard = SDL_GetKeyboardState(&count);
  if (frame.right_stick == std::array<float, 2>{} && keyboard != nullptr &&
      count > SDL_SCANCODE_L) {
    const auto key = [&](const SDL_Scancode code) { return keyboard[code] ? 1.0F : 0.0F; };
    frame.right_stick = {key(SDL_SCANCODE_L) - key(SDL_SCANCODE_J),
                         key(SDL_SCANCODE_I) - key(SDL_SCANCODE_K)};
  }
}

InputFrame DesktopInput::sample(const bool focused, const Viewport &viewport) {
  PortButtons ports = focused ? held_buttons() : PortButtons{};
  Buttons held = 0;
  Buttons latched = 0;
  for (std::size_t port = 0; port < ports.size(); ++port) {
    ports[port] |= std::exchange(injected_held_[port], 0);
    held |= ports[port];
    // Injected presses do not need focus: smoke runs are hidden.
    latched |= (focused ? latched_[port] : 0) | injected_[port];
  }
  InputFrame frame = edges_.update(held, latched, controllers());
  fill_ports(focused, ports, frame);
  if (focused) {
    sticks(held, frame);
  } else {
    sticks(held & button::directions, frame); // scripted runs hold directions
    frame.right_stick = {};
  }
  if (scripted_pointer_) {
    frame.pointer = scripted_pointer_;
    frame.pointer_moved = std::exchange(scripted_moved_, false);
    frame.clicked = std::exchange(scripted_click_, false);
  } else if (focused && pointer_pixels_) {
    const auto [x, y] = *pointer_pixels_;
    frame.pointer = std::array<float, 2>{(x - viewport.offset_x) / viewport.scale,
                                         (y - viewport.offset_y) / viewport.scale};
    frame.pointer_moved = pointer_moved_;
    frame.clicked = clicked_;
  }
  pointer_moved_ = false;
  clicked_ = false;
  return frame;
}

} // namespace phantomcrash
