// Desktop entry point: finds the game data, opens the window and runs the
// fixed-tick loop with paced presentation.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
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
#include "phantomcrash/frame_stats.hpp"
#include "phantomcrash/saved_time.hpp"
#include "phantomcrash/settings.hpp"

namespace phantomcrash {
namespace {

// The original's logic runs at 60 Hz (0xac73b8 frames per second).
constexpr double logic_tick_seconds = 1.0 / 60.0;
// A benchmark leaves out its first seconds (loading, the first textures)
// and measures against 144 fps, the frame cap's top.
constexpr double benchmark_warmup_seconds = 2.0;
constexpr double benchmark_target_rate = 144.0;

// The data documents the boot flow needs; loading them checks the data and
// mod overlay before the window opens.
constexpr const char *documents[] = {
    "boot/intro.json",    "ui/title.json",         "ui/main_menu.json",
    "ui/option.json",     "ui/message_box.json",   "ui/placeholders.json",
    "audio/music.json",   "audio/sfx.json",        "story/new_game.json",
    "ui/name_entry.json", "story/talk.json",       "story/scenes.json",
    "ui/load_game.json",  "ui/garage.json",        "ui/town_map.json",
    "ui/battle_hud.json", "ui/battle_panels.json", "audio/battle_sounds.json",
    "world/rumble.json",  "story/staff_roll.json"};

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
  const bool benchmark = options.benchmark_seconds > 0.0;
  const auto preferences = options.settings.value_or(settings_path());
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
  ScreenRunner screens(host.renderer(), roots, options.start, saved.game, saved.quest,
                       saved.vs_scoobees);
  screens.set_save_slots(saved.save_slots, saved.save_slots_unreadable);
  screens.set_rumble_robots(options.rumble_robots.value_or(saved.rumble_robots));
  const std::vector<ScreenRunner::Level> levels = screens.levels();
  {
    std::vector<std::string> labels;
    for (const ScreenRunner::Level &level : levels) {
      labels.push_back(level.label);
    }
    menu.set_levels(labels);
  }
  bool paused = false;

  using Clock = std::chrono::steady_clock;
  auto previous = Clock::now();
  int frames = 0;
  long long ticks_run = 0; // logic ticks so far: when --press presses land
  FrameStats stats;
  FrameStats work; // each frame's time before it is presented
  double benchmark_clock = 0.0;
  bool running = true;
  while (running) {
    input.configure(menu.settings().host_controls);
    input.capture_mouse(host.window(), screens.mouse_aim() && host.focused() && !smoke);
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
    FrameRateSettings rate = settings.frame_rate;
    if (benchmark) {
      rate.vsync = false; // as fast as it goes
      rate.cap_enabled = false;
    }
    host.set_vsync(rate.vsync);
    if (menu.take_interaction()) {
      // Time spent in the menu or the dialog is not game time.
      step.reset();
      pacer.reset();
      previous = Clock::now();
    }
    if (const auto level = menu.take_level();
        level && *level >= 0 && *level < static_cast<int>(levels.size())) {
      screens.jump(levels[static_cast<std::size_t>(*level)].screen);
    }
    const bool pause = !smoke && !benchmark && settings.pause_unfocused && !host.focused();
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
    const long long first = ticks_run;
    const long long last = ticks_run + std::max(ticks, 1) - 1;
    for (const auto &press : options.presses) {
      if (press.frame == press.last && first <= press.frame && press.frame <= last) {
        input.press(press.buttons, press.port);
      } else if (press.frame <= last && first <= press.last) {
        input.hold(press.buttons, press.port);
      }
    }
    ticks_run += ticks;
    for (const auto &point : options.points) {
      if (first <= point.frame && point.frame <= last) {
        input.point(point.canvas, point.click);
      }
    }
    const Viewport viewport = screens.placed(host.viewport());
    for (int tick = 0; tick < ticks; ++tick) {
      screens.tick(input.sample(host.focused(), viewport), step.tick_seconds());
    }
    input.rumble(screens.rumble());
    screens.advance(elapsed);
    screens.draw(viewport);
    counter.record(elapsed);
    if (benchmark) {
      benchmark_clock += elapsed;
      if (benchmark_clock > benchmark_warmup_seconds) {
        stats.add(elapsed);
      }
      running = benchmark_clock < benchmark_warmup_seconds + options.benchmark_seconds;
    }
    if (settings.frame_counter) {
      counter.draw(host.renderer());
    }
    if (smoke && ++frames >= options.smoke_frames) {
      running = false;
      if (options.capture) {
        save_frame(host.renderer(), *options.capture);
      }
    }
    if (benchmark && benchmark_clock > benchmark_warmup_seconds) {
      work.add(std::chrono::duration<double>(Clock::now() - now).count());
    }
    SDL_RenderPresent(host.renderer());
    pacer.wait(rate);
  }

  if (benchmark) {
    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(host.renderer(), &width, &height);
    const double budget = 1.0 / benchmark_target_rate;
    const std::string report = std::to_string(width) + "x" + std::to_string(height) +
                               "\n  frames: " + stats.report(budget) +
                               "\n  work:   " + work.report(budget);
    SDL_Log("Benchmark %s", report.c_str());
    std::printf("Benchmark %s\n", report.c_str());
    return 0;
  }
  Settings to_save = menu.settings();
  to_save.game = screens.game_options();
  to_save.quest = screens.quest_save();
  to_save.vs_scoobees = screens.vs_scoobees();
  to_save.save_slots = screens.save_slots();
  if (options.fullscreen) {
    to_save.fullscreen = saved.fullscreen;
  }
  if (!smoke && !(to_save == saved)) {
    // A changed game is stamped with the time it is written, for LOAD GAME.
    if (!to_save.quest.player_name.empty() && !(to_save.quest == saved.quest)) {
      to_save.quest.saved = local_time_text();
    }
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
    interactive = interactive && std::string(argv[index]) != "--smoke-test" &&
                  std::string(argv[index]) != "--benchmark";
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
