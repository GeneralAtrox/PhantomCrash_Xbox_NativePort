// Desktop entry point: finds the game data, opens the window and runs the
// fixed-tick loop with paced presentation.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

#include "desktop_host.hpp"
#include "desktop_frame_counter.hpp"
#include "desktop_input.hpp"
#include "desktop_options.hpp"
#include "desktop_screens.hpp"
#include "desktop_settings_menu.hpp"
#include "phantomcrash/data_roots.hpp"
#include "phantomcrash/fixed_step.hpp"
#include "phantomcrash/frame_rate.hpp"
#include "phantomcrash/settings.hpp"

namespace phantomcrash {
namespace {

// The original's logic runs at 60 Hz (0xac73b8 frames per second).
constexpr double logic_tick_seconds = 1.0 / 60.0;

// The data documents the boot flow needs; loading them checks the data and
// mod overlay before the window opens.
constexpr const char *documents[] = {
    "boot/intro.json",     "ui/title.json",        "ui/main_menu.json", "ui/option.json",
    "ui/message_box.json", "ui/placeholders.json", "audio/music.json",  "audio/sfx.json"};

std::filesystem::path find_root(const Options &options) {
  if (options.root) {
    return *options.root;
  }
  std::vector<std::filesystem::path> starts{std::filesystem::current_path()};
  if (const char *base = SDL_GetBasePath()) {
    starts.emplace_back(std::filesystem::path(reinterpret_cast<const char8_t *>(base)));
  }
  if (auto root = DataRoots::find_root(starts)) {
    return *root;
  }
  throw std::runtime_error("Unable to find the data folder (data/boot/intro.json). "
                           "Run from the checkout or pass --root.");
}

std::filesystem::path settings_path() {
  char *folder = SDL_GetPrefPath("PhantomCrash", "PhantomCrash Port");
  if (folder == nullptr) {
    return "settings.json";
  }
  std::filesystem::path path = std::filesystem::path(reinterpret_cast<const char8_t *>(folder));
  SDL_free(folder);
  return path / "settings.json";
}

// Saves what has been drawn so far this frame (before it is presented).
void save_frame(SDL_Renderer *renderer, const std::filesystem::path &path) {
  SDL_Surface *surface = SDL_RenderReadPixels(renderer, nullptr);
  const bool saved = surface != nullptr && SDL_SaveBMP(surface, path.string().c_str());
  SDL_DestroySurface(surface);
  if (!saved) {
    throw std::runtime_error("Unable to save " + path.string() + ": " + SDL_GetError());
  }
}

void apply_fullscreen(DesktopHost &host, SettingsMenu &menu) {
  const bool wanted = menu.settings().fullscreen;
  if (wanted == host.fullscreen()) {
    return;
  }
  if (wanted) {
    menu.set_visible(false);
    host.set_fullscreen(true);
  } else {
    host.set_fullscreen(false);
    menu.set_visible(true);
  }
}

int run(const Options &options) {
  const auto root = find_root(options);
  const auto roots = DataRoots::standard(root);
  for (const char *document : documents) {
    static_cast<void>(roots.document(document));
  }
  const bool smoke = options.smoke_frames > 0;
  const auto preferences = settings_path();
  const Settings saved = load_settings(preferences);
  Settings initial = saved;
  if (options.fullscreen) {
    initial.fullscreen = *options.fullscreen;
  }

  DesktopHost host(initial.fullscreen, smoke, options.size);
  SettingsMenu menu(smoke ? nullptr : host.native_window(), initial);
  menu.set_visible(!initial.fullscreen);
  DesktopInput input;
  FixedStep step(logic_tick_seconds);
  FramePacer pacer;
  FrameCounter counter;
  SDL_Log("Data root: %s", root.string().c_str());
  ScreenRunner screens(host.renderer(), roots, options.start, saved.game);
  bool paused = false;

  using Clock = std::chrono::steady_clock;
  auto previous = Clock::now();
  int frames = 0;
  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                 (event.key.scancode == SDL_SCANCODE_F11 ||
                  (event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
        menu.set_fullscreen(!menu.settings().fullscreen);
      }
      input.handle(event);
    }
    const Settings &settings = menu.settings();
    apply_fullscreen(host, menu);
    host.set_vsync(settings.frame_rate.vsync);
    if (menu.take_interaction()) {
      // Time spent in the menu or the dialog is not game time.
      step.reset();
      pacer.reset();
      previous = Clock::now();
    }
    const bool pause = !smoke && settings.pause_unfocused && !host.focused();
    if (pause != paused) {
      paused = pause;
      screens.set_paused(paused);
    }
    if (paused) {
      SDL_WaitEventTimeout(nullptr, 100);
      step.reset();
      pacer.reset();
      previous = Clock::now();
      continue;
    }

    const auto now = Clock::now();
    double elapsed = std::chrono::duration<double>(now - previous).count();
    previous = now;
    // Smoke runs step one logic tick a frame, so --press frames are game
    // ticks (60 a second) however fast the machine presents.
    if (smoke) {
      elapsed = step.tick_seconds();
    }
    const int ticks = smoke ? 1 : step.add(elapsed);
    for (const auto &press : options.presses) {
      if (press.frame == frames) {
        input.press(press.buttons);
      }
    }
    for (int tick = 0; tick < ticks; ++tick) {
      screens.tick(input.sample(host.focused()), step.tick_seconds());
    }
    screens.advance(elapsed);
    screens.draw(host.viewport());
    counter.record(elapsed);
    if (settings.frame_counter) {
      counter.draw(host.renderer());
    }
    if (smoke && ++frames >= options.smoke_frames) {
      running = false;
      if (options.capture) {
        save_frame(host.renderer(), *options.capture);
      }
    }
    SDL_RenderPresent(host.renderer());
    pacer.wait(settings.frame_rate);
  }

  Settings to_save = menu.settings();
  to_save.game = screens.game_options();
  if (options.fullscreen) {
    to_save.fullscreen = saved.fullscreen;
  }
  if (!smoke && !(to_save == saved)) {
    save_settings(preferences, to_save);
  }
  return 0;
}

} // namespace
} // namespace phantomcrash

int main(int argc, char *argv[]) {
  // Automated runs must never wait on a modal error box.
  bool interactive = true;
  for (int index = 1; index < argc; ++index) {
    interactive = interactive && std::string(argv[index]) != "--smoke-test";
  }
  try {
    const auto options = phantomcrash::parse_options(argc, argv);
    if (options.help) {
      std::fputs(phantomcrash::usage(), stdout);
      return 0;
    }
    return phantomcrash::run(options);
  } catch (const std::exception &error) {
    SDL_Log("%s", error.what());
    if (interactive) {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "PhantomCrash port", error.what(), nullptr);
    }
    return 1;
  }
}
