#pragma once

#include "phantomcrash/json.hpp"

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

namespace phantomcrash {

// Where the port finds its files. Data documents come from `data/`, with a
// same-named file under `mods/data/` merged on top. Asset paths named by the
// data resolve against the asset roots in order, so a file under `mods/`
// replaces the converted or extracted original.
class DataRoots final {
public:
  DataRoots(std::filesystem::path data, std::filesystem::path mods,
            std::vector<std::filesystem::path> asset_roots);

  // The layout of a source checkout or install rooted at `root`:
  // data/, mods/, artifacts/europe/assets/, artifacts/europe/files/Media/.
  [[nodiscard]] static DataRoots standard(const std::filesystem::path &root);
  [[nodiscard]] static DataRoots configured(const std::filesystem::path &root, bool slai);
  // As standard, without mods/: the shipped data and the converted assets.
  [[nodiscard]] static DataRoots shipped(const std::filesystem::path &root);

  // First directory at or above one of `starts` that holds data/boot/intro.json.
  [[nodiscard]] static std::optional<std::filesystem::path>
  find_root(const std::vector<std::filesystem::path> &starts);

  // The file `relative` names in the first asset root that has it. File and
  // folder names match without regard to case, as they did on the disc.
  [[nodiscard]] std::optional<std::filesystem::path> find_asset(std::string_view relative) const;
  // As find_asset, but throws naming the searched roots when it is missing.
  [[nodiscard]] std::filesystem::path asset(std::string_view relative) const;

  // data/<relative> with mods/data/<relative> merged on top (json::merge),
  // unless there is no mods folder.
  [[nodiscard]] json::JsonValue document(std::string_view relative) const;

  [[nodiscard]] const std::vector<std::filesystem::path> &asset_roots() const {
    return asset_roots_;
  }

private:
  std::filesystem::path data_;
  std::filesystem::path mods_;
  std::filesystem::path content_;
  std::vector<std::filesystem::path> asset_roots_;
};

} // namespace phantomcrash
