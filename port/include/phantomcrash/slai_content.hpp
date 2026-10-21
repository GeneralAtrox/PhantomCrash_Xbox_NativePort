#pragma once
#include "phantomcrash/settings.hpp"

#include <string>
#include <vector>

namespace phantomcrash {
[[nodiscard]] bool slai_installed(const std::filesystem::path &root);
[[nodiscard]] bool legacy_slai(const std::filesystem::path &root);

// The files older builds kept beside `base` for each content mode, base
// game first: settings.base.json and settings.slai.json. Base game and
// S.L.A.I. now share `base`.
[[nodiscard]] std::vector<std::filesystem::path>
legacy_content_profiles(const std::filesystem::path &base);

// What bringing the old content-mode files in did.
struct ProfileMerge {
  std::vector<std::string> moved; // one line per game brought in, for the log
  // Files with games that found no free slot, or that cannot be read; they
  // stay where they are.
  std::vector<std::filesystem::path> left;
};

// Brings the games of the old content-mode files into `settings` once:
// the current game (into a free save slot when `settings` has its own),
// the save slots and co-op profiles into free ones, the online profile when
// there is none and the kept VS Scoobees. The old files are never changed
// or deleted. Does nothing when `settings` has merged before.
ProfileMerge merge_content_profiles(Settings &settings, const std::filesystem::path &base);
} // namespace phantomcrash
