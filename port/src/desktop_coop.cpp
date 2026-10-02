#include "desktop_coop.hpp"
#include "desktop_coop_matches.hpp"
#include "phantomcrash/saved_time.hpp"
#include <algorithm>

namespace phantomcrash {

CoopScreen::CoopScreen(SDL_Renderer *renderer, const DataRoots &roots, const GameOptions options,
                       ScreenRunner::SaveSlots imports, QuestSave current,
                       std::shared_ptr<CoopState> state, const GlyphFont &font)
    : renderer_(renderer), textures_(renderer, roots), font_(font),
      map_(parse_town_map(roots.document("ui/town_map.json"))), roots_(roots), options_(options),
      imports_(std::move(imports)), current_(std::move(current)), state_(std::move(state)) {
  state_->matches = std::make_shared<CoopMatches>();
  SDL_Log("Co-op: lobby opened; independent profiles, solo saves preserved");
}

CoopScreen::~CoopScreen() {
  for (int slot = 0; slot < 4; ++slot) {
    persist(slot);
    SDL_DestroyTexture(players_[slot].target);
  }
  state_->matches.reset();
}

void CoopScreen::persist(const int slot) {
  const auto &player = players_[slot];
  if (player.runner && player.ready) {
    auto save = player.runner->quest_save();
    save.saved = local_time_text();
    state_->profiles[slot] = std::move(save);
  }
}

std::string CoopScreen::choice_label(const int slot) const {
  switch (players_[slot].choice) {
  case 0:
    return state_->profiles[slot] ? "Continue: " + state_->profiles[slot]->player_name
                                  : "No co-op save";
  case 1:
    return "New game";
  case 2:
    return current_.player_name.empty() ? "No current solo save"
                                        : "Import: " + current_.player_name;
  default: {
    const auto &save = imports_[players_[slot].choice - 3];
    return save ? "Import slot: " + save->player_name : "Empty solo slot";
  }
  }
}

void CoopScreen::load_player(const int slot) {
  auto &player = players_[slot];
  std::optional<QuestSave> save;
  if (player.choice == 0) {
    save = state_->profiles[slot];
  }
  if (player.choice == 2 && !current_.player_name.empty()) {
    save = current_;
  }
  if (player.choice >= 3) {
    save = imports_[player.choice - 3];
  }
  if (player.choice != 1 && !save) {
    return;
  }
  // Each slot imports into its own campaign. No shared save file is writable
  // from a player pane, so two imported copies cannot overwrite one another.
  AudioOptions audio = audio_;
  if (slot != 0) {
    audio.levels[static_cast<int>(AudioBus::music)] = 0;
  }
  player.runner = std::make_unique<ScreenRunner>(
      renderer_, roots_, player.choice == 1 ? "coop_name" : "garage", options_,
      save.value_or(QuestSave{}), std::vector<VsScoobee>{}, audio, state_, slot);
  player.runner->set_rumble_robots(std::max(4, state_->robots));
  if (player.choice != 1) {
    ready_player(slot);
  }
  SDL_Log("Co-op: player=%d port=%d loaded=%s day=%d", slot + 1, player.port,
          player.ready ? player.runner->quest_save().player_name.c_str() : "new/name entry",
          player.runner->quest_save().day + 1);
}

void CoopScreen::ready_player(const int slot) {
  auto &player = players_[slot];
  // Continue retains the last committed checkpoint, even if the live campaign
  // has advanced. Imports and newly named campaigns need their own first save.
  if (player.choice != 0 || !state_->saved_profiles[slot]) {
    auto checkpoint = player.runner->quest_save();
    checkpoint.saved = local_time_text();
    player.save_error = !state_->save_profile || !state_->save_profile(slot, checkpoint);
    if (player.save_error) {
      return;
    }
    state_->profiles[slot] = checkpoint;
    state_->saved_profiles[slot] = std::move(checkpoint);
    SDL_Log("Co-op starting checkpoint: player=%d day=%d", slot + 1,
            state_->saved_profiles[slot]->day + 1);
  }
  player.save_error = false;
  player.ready = true;
}

void CoopScreen::lobby_input(const int slot, const InputFrame &input) {
  auto &player = players_[slot];
  if (player.runner) {
    if (player.save_error) {
      if (input.any_pressed(button::b)) {
        player.runner.reset();
        player.save_error = false;
      } else if (input.any_pressed(button::a)) {
        ready_player(slot);
      }
    } else if (!player.ready) {
      player.runner->tick(input, 1.0 / 60.0);
      if (player.runner->screen() == "garage") {
        ready_player(slot);
      }
    } else if (input.any_pressed(button::b)) {
      persist(slot);
      player.runner.reset();
      player.ready = false;
    }
    return;
  }
  const int direction = input.any_pressed(button::left | button::up)      ? -1
                        : input.any_pressed(button::right | button::down) ? 1
                                                                          : 0;
  player.choice = (player.choice + direction + 6) % 6;
  if (input.any_pressed(button::a)) {
    load_player(slot);
  }
  if (input.any_pressed(button::b)) {
    player.port = -1;
  }
}

void CoopScreen::tick(const InputFrame &input, const double seconds) {
  if (!playing_) {
    // BACK (Escape) leaves the lobby for the main menu whoever has joined;
    // the destructor keeps the ready players' checkpoints.
    if (input.any_pressed(button::back)) {
      done_ = true;
      return;
    }
    for (int port = 0; port < controller_ports; ++port) {
      if (!(input.ports[port].pressed & button::start)) {
        continue;
      }
      if (std::ranges::any_of(players_, [&](const Player &p) { return p.port == port; })) {
        continue;
      }
      const auto vacant = std::ranges::find(players_, -1, &Player::port);
      if (vacant != players_.end()) {
        const int slot = static_cast<int>(std::distance(players_.begin(), vacant));
        vacant->port = port;
        vacant->choice = state_->profiles[slot] ? 0 : 1;
        SDL_Log("Co-op: controller port=%d joined player=%d", port, slot + 1);
      }
    }
    const bool ready =
        std::ranges::all_of(players_, [](const Player &p) { return p.port < 0 || p.ready; });
    if (players_[0].ready && ready && input.ports[players_[0].port].pressed & button::start) {
      playing_ = true;
      SDL_Log("Co-op: started with %lld players",
              static_cast<long long>(
                  std::ranges::count_if(players_, [](const Player &p) { return p.port >= 0; })));
      return;
    }
    if (std::ranges::none_of(players_, [](const Player &p) { return p.port >= 0; }) &&
        input.any_pressed(button::b)) {
      done_ = true;
    }
  }
  for (int slot = 0; slot < 4; ++slot) {
    auto &player = players_[slot];
    if (player.port < 0) {
      continue;
    }
    player.connected = input.ports[player.port].connected;
    const InputFrame own = player_input(input, slot);
    if (!playing_) {
      lobby_input(slot, own);
    } else if (player.runner) {
      if (player.runner->screen() != "title") {
        player.runner->tick(own, seconds);
      }
      persist(slot);
    }
  }
  if (state_->matches && playing_) {
    state_->matches->tick(static_cast<float>(seconds));
  }
  if (playing_ && std::ranges::all_of(players_, [](const Player &p) {
        return !p.runner || p.runner->screen() == "title";
      })) {
    done_ = true;
  }
}

void CoopScreen::advance(const double seconds) {
  for (auto &player : players_) {
    if (player.runner) {
      player.runner->advance(seconds);
    }
  }
}

void CoopScreen::set_paused(const bool paused) {
  for (auto &player : players_) {
    if (player.runner) {
      player.runner->set_paused(paused);
    }
  }
}

std::array<RumbleMotors, controller_ports> CoopScreen::session_rumble() const {
  std::array<RumbleMotors, controller_ports> motors{};
  for (const auto &player : players_) {
    if (player.port >= 0 && player.runner) {
      motors[player.port] = player.runner->rumble()[0];
    }
  }
  return motors;
}

std::optional<SceneProgress> CoopScreen::scene_progress() const {
  for (int slot = 0; slot < 4; ++slot) {
    if (!players_[slot].runner) {
      continue;
    }
    if (auto progress = players_[slot].runner->scene_progress()) {
      progress->label = "P" + std::to_string(slot + 1) + " " + progress->label;
      return progress;
    }
  }
  return std::nullopt;
}

json::JsonValue CoopScreen::scene_details() const {
  json::JsonValue::Array players;
  json::JsonValue::Array screens;
  json::JsonValue::Array kinds;
  std::string summary = playing_ ? "co-op playing" : "co-op lobby";
  for (int slot = 0; slot < 4; ++slot) {
    const Player &player = players_[slot];
    if (player.port < 0) {
      continue;
    }
    json::JsonValue::Object entry{{"player", {std::int64_t{slot + 1}}},
                                  {"port", {std::int64_t{player.port}}},
                                  {"ready", {player.ready}},
                                  {"connected", {player.connected}}};
    if (player.runner) {
      json::JsonValue state = player.runner->scene_state();
      screens.push_back({player.runner->screen()});
      summary += ", P" + std::to_string(slot + 1) + " at " + player.runner->screen();
      if (const json::JsonValue *details = state.find("details")) {
        if (const json::JsonValue *found = details->find("kinds"); found && found->is_array()) {
          kinds.insert(kinds.end(), found->array().begin(), found->array().end());
        }
      }
      entry.push_back({"runner", std::move(state)});
    }
    players.push_back({std::move(entry)});
  }
  return {json::JsonValue::Object{{"summary", {std::move(summary)}},
                                  {"kinds", {std::move(kinds)}},
                                  {"screens", {std::move(screens)}},
                                  {"players", {std::move(players)}}}};
}

void CoopScreen::set_presentation(const AudioOptions &audio, const PostProcessOptions &post) {
  audio_ = audio;
  post_ = post;
  int music_owner = -1;
  for (int slot = 0; slot < 4; ++slot) {
    if (!players_[slot].runner) {
      continue;
    }
    if (music_owner < 0) {
      music_owner = slot;
    }
    if (players_[slot].runner->screen() == "rumbling_battle") {
      music_owner = slot;
      break;
    }
  }
  for (int slot = 0; slot < 4; ++slot) {
    if (players_[slot].runner) {
      auto own = audio;
      if (slot != music_owner) {
        own.levels[static_cast<int>(AudioBus::music)] = 0;
      }
      if (state_->matches) {
        for (const auto &[key, match] : state_->matches->matches) {
          if (match->players[slot].actor < 0) {
            continue;
          }
          const auto first =
              std::ranges::find_if(match->players, [](const auto &p) { return p.actor >= 0; });
          if (std::distance(match->players.begin(), first) != slot) {
            for (const auto bus : {AudioBus::weapons, AudioBus::robots, AudioBus::environment}) {
              own.levels[static_cast<int>(bus)] = 0;
            }
          }
        }
      }
      players_[slot].runner->set_audio_options(own);
      players_[slot].runner->set_post_process(post);
    }
  }
}

} // namespace phantomcrash
