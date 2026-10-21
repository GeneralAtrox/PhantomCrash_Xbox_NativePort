#include "phantomcrash/slai_content.hpp"
#include <algorithm>
#include <stdexcept>

namespace phantomcrash {
bool legacy_slai(const std::filesystem::path &root) {
  return std::filesystem::is_regular_file(root / "mods/slai/native_parts.json");
}

bool slai_installed(const std::filesystem::path &root) {
  const auto assets = root / "artifacts/slai/assets";
  try {
    const auto receipt = json::read_file(assets / "installed.json");
    if (receipt.number_or("version", 0) != 1 || receipt.string_or("disc", "") != "SLES_529.40") {
      return false;
    }
    for (const char *file : {"tables/parts.json", "tables/garage.json", "robots/catalogue.json",
                             "slai/native_parts.json", "slai/assets/manifest.json"}) {
      if (!std::filesystem::is_regular_file(assets / file)) {
        return false;
      }
    }
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

std::vector<std::filesystem::path> legacy_content_profiles(const std::filesystem::path &base) {
  std::vector<std::filesystem::path> files;
  for (const wchar_t *mode : {L".base", L".slai"}) {
    auto file = base;
    file.replace_filename(base.stem().wstring() + mode + base.extension().wstring());
    files.push_back(std::move(file));
  }
  return files;
}

namespace {

bool empty_game(const QuestSave &quest) {
  return quest.player_name.empty();
}

// Puts `quest` in the first free entry unless an entry holds it already.
template <std::size_t count>
std::optional<int> place(std::array<std::optional<QuestSave>, count> &slots,
                         const QuestSave &quest) {
  if (std::ranges::find(slots, std::optional{quest}) != slots.end()) {
    return -1;
  }
  const auto free = std::ranges::find_if(slots, [](const auto &slot) { return !slot; });
  if (free == slots.end()) {
    return std::nullopt;
  }
  *free = quest;
  return static_cast<int>(free - slots.begin());
}

std::string describe(const std::filesystem::path &file, const std::string &what, const int to,
                     const char *where) {
  const std::string name = file.filename().string();
  if (to < 0) {
    return what + " of " + name + " is already in the shared save";
  }
  return what + " of " + name + " -> " + where + " " + std::to_string(to + 1);
}

} // namespace

ProfileMerge merge_content_profiles(Settings &settings, const std::filesystem::path &base) {
  ProfileMerge merge;
  if (settings.content_profiles_merged) {
    return merge;
  }
  settings.content_profiles_merged = true;
  for (const auto &file : legacy_content_profiles(base)) {
    if (!std::filesystem::exists(file)) {
      continue;
    }
    const Settings old = load_settings(file);
    bool left = old.save_slots_unreadable;
    const auto bring = [&](const QuestSave &quest, const std::string &what) {
      if (const auto to = place(settings.save_slots, quest)) {
        merge.moved.push_back(describe(file, what, *to, "save slot"));
      } else {
        left = true;
      }
    };
    if (!empty_game(old.quest)) {
      if (empty_game(settings.quest)) {
        settings.quest = old.quest;
        merge.moved.push_back("the current game of " + file.filename().string() +
                              " -> the current game");
      } else if (!(old.quest == settings.quest)) {
        bring(old.quest, "the current game");
      }
    }
    for (std::size_t slot = 0; slot < old.save_slots.size(); ++slot) {
      if (old.save_slots[slot]) {
        bring(*old.save_slots[slot], "save slot " + std::to_string(slot + 1));
      }
    }
    for (std::size_t slot = 0; slot < old.coop_profiles.size(); ++slot) {
      if (!old.coop_profiles[slot]) {
        continue;
      }
      const std::string what = "co-op profile " + std::to_string(slot + 1);
      if (const auto to = place(settings.coop_profiles, *old.coop_profiles[slot])) {
        merge.moved.push_back(describe(file, what, *to, "co-op profile"));
      } else {
        left = true;
      }
    }
    if (old.online_profile && !settings.online_profile) {
      settings.online_profile = old.online_profile;
      merge.moved.push_back("the online profile of " + file.filename().string());
    } else if (old.online_profile && !(old.online_profile == settings.online_profile)) {
      left = true;
    }
    for (const VsScoobee &scoobee : old.vs_scoobees) {
      if (std::ranges::find(settings.vs_scoobees, scoobee) == settings.vs_scoobees.end()) {
        settings.vs_scoobees.push_back(scoobee);
        merge.moved.push_back("VS Scoobee " + scoobee.name + " of " + file.filename().string());
      }
    }
    if (left) {
      merge.left.push_back(file);
    }
  }
  return merge;
}
} // namespace phantomcrash
