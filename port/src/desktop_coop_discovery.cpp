#include "desktop_coop.hpp"
#include "desktop_online_game.hpp"
#include "phantomcrash/online_lobby.hpp"
#include <algorithm>
#include <bit>
#include <chrono>

namespace phantomcrash {
namespace {
double discovery_time() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
constexpr float first_row = 132, row_height = 32;
constexpr int visible_rooms = 6;
} // namespace

void CoopScreen::open_online_game() {
  state_->online_game = std::make_shared<OnlineGame>(
      state_->online->room(), renderer_, roots_, state_->matches, state_->options, state_->robots);
  if (state_->online_host && !lan_advertiser_) {
    try {
      lan_advertiser_ =
          std::make_unique<online::LanAdvertiser>(state_->online->room().local_endpoint().port);
    } catch (const std::exception &error) {
      // A manually addressed host still works if the discovery port is occupied.
      SDL_Log("LAN discovery unavailable: %s", error.what());
    }
  }
}

void CoopScreen::advertise_lan() {
  if (lan_advertiser_) {
    const auto &presence = state_->online->presence()[0];
    const auto name = !presence.name.empty()   ? presence.name
                      : state_->online_profile ? state_->online_profile->player_name
                                               : "Phantom Crash";
    lan_advertiser_->update(
        name, static_cast<std::uint8_t>(std::popcount(state_->online->room().participants())), 3);
  }
}

void CoopScreen::begin_lan(bool host, online::Endpoint server) {
  selected_host_ = host;
  selected_server_ = server;
  discovery_error_.clear();
  if (!state_->pending_fingerprint.valid()) {
    state_->pending_fingerprint = std::async(std::launch::async, state_->fingerprint).share();
  }
  checking_content_ = true;
  SDL_Log("LAN: checking content before %s", host ? "hosting" : "joining");
}

bool CoopScreen::tick_connection_menu(const InputFrame &input) {
  if (!connection_menu_) {
    return false;
  }
  if (input.any_pressed(button::back | button::b) || input.right_clicked) {
    done_ = true;
    return true;
  }
  try {
    if (checking_content_) {
      if (state_->pending_fingerprint.wait_for(std::chrono::seconds(0)) !=
          std::future_status::ready) {
        return true;
      }
      online::RoomOptions options;
      options.host = selected_host_;
      options.bind = {0, selected_host_ ? online::lan_game_port : std::uint16_t{0}};
      options.server = selected_server_;
      options.compatibility = state_->pending_fingerprint.get();
      auto lobby = std::make_shared<online::Lobby>(options);
      auto advertiser =
          selected_host_
              ? std::make_unique<online::LanAdvertiser>(lobby->room().local_endpoint().port)
              : nullptr;
      local_profiles_ = state_->profiles;
      local_saved_profiles_ = state_->saved_profiles;
      state_->profiles.fill(std::nullopt);
      state_->saved_profiles.fill(std::nullopt);
      state_->online_mode = true;
      state_->online_host = selected_host_;
      state_->online = std::move(lobby);
      lan_advertiser_ = std::move(advertiser);
      open_online_game();
      lan_browser_.reset();
      checking_content_ = connection_menu_ = false;
      SDL_Log("LAN: %s game on %s", selected_host_ ? "hosting" : "joining",
              (selected_host_ ? state_->online->room().local_endpoint() : selected_server_)
                  .text()
                  .c_str());
      return true;
    }
    if (lan_browser_) {
      const auto &rooms = lan_browser_->rooms();
      const auto selected = connection_choice_ >= 2 && connection_choice_ - 2 < rooms.size()
                                ? rooms[connection_choice_ - 2].id
                                : 0;
      try {
        lan_browser_->update(discovery_time());
      } catch (const std::exception &error) {
        discovery_error_ = error.what();
      }
      if (selected) {
        const auto found = std::ranges::find(rooms, selected, &online::LanRoom::id);
        connection_choice_ = found == rooms.end() ? 0 : 2 + static_cast<int>(found - rooms.begin());
      }
    }
    const int count = 2 + (lan_browser_ ? static_cast<int>(lan_browser_->rooms().size()) : 0);
    connection_choice_ = std::clamp(connection_choice_, 0, count - 1);
    if (input.any_pressed(button::up)) {
      connection_choice_ = (connection_choice_ + count - 1) % count;
    }
    if (input.any_pressed(button::down)) {
      connection_choice_ = (connection_choice_ + 1) % count;
    }
    bool clicked = false;
    const int scroll = std::max(0, connection_choice_ - (visible_rooms + 1));
    if (input.pointer && ((*input.pointer)[0] >= 48 && (*input.pointer)[0] <= 592)) {
      const float y = (*input.pointer)[1];
      const int row = static_cast<int>((y - first_row + 8) / row_height);
      const int choice = row < 2 ? row : row + scroll;
      if (y >= first_row - 8 && row < visible_rooms + 2 && choice < count) {
        if (input.pointer_moved || input.clicked) {
          connection_choice_ = choice;
        }
        clicked = input.clicked;
      }
    }
    if (input.any_pressed(button::a | button::start) || clicked) {
      if (connection_choice_ == 0) {
        connection_menu_ = false;
        lan_browser_.reset();
      } else if (connection_choice_ == 1) {
        begin_lan(true);
      } else {
        const auto &room = lan_browser_->rooms().at(connection_choice_ - 2);
        if (room.players < room.capacity) {
          begin_lan(false, room.server);
        }
      }
    }
  } catch (const std::exception &error) {
    discovery_error_ = error.what();
    checking_content_ = false;
    SDL_Log("LAN: %s", discovery_error_.c_str());
  }
  return true;
}

bool CoopScreen::draw_connection_menu(const Viewport &viewport) {
  if (!connection_menu_) {
    return false;
  }
  SDL_SetRenderDrawColor(renderer_, 10, 14, 22, 255);
  SDL_RenderClear(renderer_);
  text(viewport, "CO-OP", 48, 52, 24);
  if (checking_content_) {
    text(viewport, "Checking game content...", 48, 140, 16);
    text(viewport, "This can take a moment on the first connection.", 48, 182, 11);
  } else {
    text(viewport, "Games hosted on your local network appear automatically.", 48, 96, 11);
    const auto row = [&](int choice, int position, const std::string &label, bool full = false) {
      text(viewport, (choice == connection_choice_ ? "> " : "  ") + label, 48,
           first_row + row_height * position, 13,
           full                           ? SDL_Color{140, 140, 140, 255}
           : choice == connection_choice_ ? SDL_Color{255, 230, 80, 255}
                                          : SDL_Color{255, 255, 255, 255});
    };
    row(0, 0, "Local split-screen");
    row(1, 1, "Host network game");
    if (lan_browser_) {
      const auto &rooms = lan_browser_->rooms();
      const int scroll = std::max(0, connection_choice_ - (visible_rooms + 1));
      for (int i = scroll; i < static_cast<int>(rooms.size()) && i < scroll + visible_rooms; ++i) {
        const auto &room = rooms[i];
        row(i + 2, i - scroll + 2,
            room.name + "  " + std::to_string(room.players) + "/" + std::to_string(room.capacity) +
                "  " + room.server.text(),
            room.players == room.capacity);
      }
      if (rooms.empty()) {
        text(viewport, "Searching for hosted games...", 48, first_row + 2 * row_height, 12);
      }
    }
    if (!discovery_error_.empty()) {
      text(viewport, discovery_error_, 48, 402, 10, {255, 120, 100, 255});
    }
  }
  text(viewport, "A / Enter: select    B / Esc: back", 48, 445, 11);
  return true;
}
} // namespace phantomcrash
