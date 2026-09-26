#pragma once

#include "phantomcrash/input.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phantomcrash {

struct Options {
  // Folder holding data/, mods/ and artifacts/; found automatically when absent.
  std::optional<std::filesystem::path> root;
  // Overrides the saved setting for this run.
  std::optional<bool> fullscreen;
  // Smoke test: load everything, run this many frames hidden, then exit.
  int smoke_frames{};
  // Smoke test only: save the last rendered frame as a BMP.
  std::optional<std::filesystem::path> capture;
  // Window client size in pixels for this run (e.g. to check other aspects).
  std::optional<std::pair<int, int>> size;
  // Screen to start on: intro (default), title, main_menu or a placeholder.
  std::string start{"intro"};
  // Smoke test only: button presses injected at given frames, to drive menus.
  struct Press {
    int frame{};
    Buttons buttons{};
  };
  std::vector<Press> presses;
  bool help{};
};

// Throws std::runtime_error on an unknown or incomplete argument.
[[nodiscard]] Options parse_options(int argc, char **argv);
[[nodiscard]] const char *usage();

} // namespace phantomcrash
