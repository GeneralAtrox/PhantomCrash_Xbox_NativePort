#include "desktop_options.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

namespace phantomcrash {

const char *usage() {
  return "Usage: phantomcrash_port [--root <folder>] [--fullscreen | --windowed] "
         "[--size <width>x<height>] [--smoke-test [frames] [--capture <file.bmp>]]\n"
         "\n"
         "The root folder holds data/, mods/ and artifacts/europe (the extracted and\n"
         "converted game files). No game files are bundled.\n";
}

Options parse_options(const int argc, char **argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--root") {
      if (++index >= argc) {
        throw std::runtime_error("--root requires a folder");
      }
      options.root = std::filesystem::path(argv[index]);
    } else if (argument == "--fullscreen") {
      options.fullscreen = true;
    } else if (argument == "--windowed") {
      options.fullscreen = false;
    } else if (argument == "--smoke-test") {
      options.smoke_frames = 120;
      if (index + 1 < argc && argv[index + 1][0] != '-') {
        const std::string value = argv[++index];
        std::size_t used = 0;
        options.smoke_frames = std::stoi(value, &used);
        if (used != value.size() || options.smoke_frames <= 0) {
          throw std::runtime_error("Invalid smoke-test frame count: " + value);
        }
      }
    } else if (argument == "--capture") {
      if (++index >= argc) {
        throw std::runtime_error("--capture requires a file");
      }
      options.capture = std::filesystem::path(argv[index]);
    } else if (argument == "--size") {
      if (++index >= argc) {
        throw std::runtime_error("--size requires <width>x<height>");
      }
      const std::string_view text = argv[index];
      const auto separator = text.find('x');
      int width = 0;
      int height = 0;
      const auto parse = [](std::string_view part, int &value) {
        const auto result = std::from_chars(part.data(), part.data() + part.size(), value);
        return result.ec == std::errc{} && result.ptr == part.data() + part.size();
      };
      if (separator == std::string_view::npos || !parse(text.substr(0, separator), width) ||
          !parse(text.substr(separator + 1), height) || width < 320 || height < 240) {
        throw std::runtime_error("Invalid --size: " + std::string(argv[index]));
      }
      options.size = std::pair(width, height);
    } else if (argument == "--start") {
      if (++index >= argc) {
        throw std::runtime_error("--start requires a screen name");
      }
      options.start = argv[index];
    } else if (argument == "--press") {
      if (++index >= argc) {
        throw std::runtime_error("--press requires <frame>:<button>");
      }
      const std::string_view text = argv[index];
      const auto separator = text.find(':');
      int frame = 0;
      const auto frame_text = text.substr(0, separator);
      const auto parsed =
          std::from_chars(frame_text.data(), frame_text.data() + frame_text.size(), frame);
      if (separator == std::string_view::npos || parsed.ec != std::errc{} ||
          parsed.ptr != frame_text.data() + frame_text.size() || frame < 0) {
        throw std::runtime_error("Invalid --press: " + std::string(text));
      }
      const auto buttons = button_from_name(text.substr(separator + 1));
      if (!buttons) {
        throw std::runtime_error("Unknown button in --press: " + std::string(text));
      }
      options.presses.push_back({frame, *buttons});
    } else if (argument == "--help" || argument == "-h") {
      options.help = true;
    } else {
      throw std::runtime_error("Unknown argument: " + std::string(argument));
    }
  }
  if ((options.capture || !options.presses.empty()) && options.smoke_frames == 0) {
    throw std::runtime_error("--capture and --press require --smoke-test");
  }
  return options;
}

} // namespace phantomcrash
