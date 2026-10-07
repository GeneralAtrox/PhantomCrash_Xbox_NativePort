#include "desktop_coop.hpp"
#include "desktop_text.hpp"
#include "phantomcrash/split_screen.hpp"
#include <algorithm>
#include <stdexcept>
#include <cctype>

namespace phantomcrash {
namespace {
std::string screen_label(std::string name) {
  if (name == "rumbling_battle") {
    return "Rumble";
  }
  if (name == "match_result") {
    return "Results";
  }
  if (name.starts_with("event_")) {
    return "Story";
  }
  if (name.starts_with("dealer_")) {
    name.erase(0, 7);
  }
  bool capital = true;
  for (auto &c : name) {
    if (c == '_') {
      c = ' ';
    }
    if (capital) {
      c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    capital = c == ' ';
  }
  return name;
}
std::vector<SplitScreen> coop_panes(const int count, const int width, const int height) {
  auto panes = split_screens(count, width, height);
  // Side-by-side keeps town menus legible while either player is shopping.
  if (count == 2) {
    const float half = static_cast<float>(width / 2);
    panes[0].rect = {0, 0, half, static_cast<float>(height)};
    panes[1].rect = {half, 0, static_cast<float>(width) - half, static_cast<float>(height)};
  }
  return panes;
}
Viewport player_canvas(const int width, const int height, const bool playing) {
  const int bar = playing ? width / 16 : 0;
  return fit_canvas(width, height - bar);
}
} // namespace

void CoopScreen::text(const Viewport &v, const std::string &line, const float x, const float y,
                      const float size, const SDL_Color colour) {
  auto glyphs = layout_run(font_, glyph_run(font_, line), 0, 0);
  const float scale = size / font_.cell;
  for (auto &glyph : glyphs) {
    glyph.x = x + glyph.x * scale;
    glyph.y = y + glyph.y * scale;
    glyph.width *= scale;
    glyph.height *= scale;
  }
  const auto rgb = (static_cast<std::uint32_t>(colour.r) << 16) |
                   (static_cast<std::uint32_t>(colour.g) << 8) | colour.b;
  draw_glyphs(textures_, font_, glyphs, v, rgb, std::nullopt, colour.a / 255.0F);
}

InputFrame CoopScreen::player_input(const InputFrame &input, const int slot) const {
  InputFrame own = port_frame(input, players_[slot].port);
  if (!own.controllers) {
    return {};
  }
  // The keyboard/mouse belongs to its joined port. Controllers never inherit
  // another player's analog axes or a global mouse click.
  if (players_[slot].port == 0 && input.pointer) {
    const int count = state_->online ? 1
                      : playing_     ? static_cast<int>(std::ranges::count_if(
                                       players_, [](const Player &p) { return p.port >= 0; }))
                                     : 4;
    const auto panes = coop_panes(count, viewport_.pixel_width, viewport_.pixel_height);
    int pane = 0;
    for (int i = 0; i < slot; ++i) {
      if (!state_->online && (!playing_ || players_[i].port >= 0)) {
        ++pane;
      }
    }
    const auto &rect = panes.at(pane).rect;
    const float x = viewport_.to_pixel_x((*input.pointer)[0]) - viewport_.origin_x;
    const float y = viewport_.to_pixel_y((*input.pointer)[1]) - viewport_.origin_y;
    if (x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height) {
      const auto local =
          player_canvas(static_cast<int>(rect.width), static_cast<int>(rect.height), playing_);
      own.pointer = std::array{
          (x - rect.x - local.offset_x) / local.scale,
          (y - rect.y - (playing_ ? static_cast<int>(rect.width) / 16 : 0) - local.offset_y) /
              local.scale};
      own.pointer_moved = input.pointer_moved;
      own.clicked = input.clicked;
      own.right_clicked = input.right_clicked;
      own.mouse_left_held = input.mouse_left_held;
      own.mouse_wheel = input.mouse_wheel;
    }
  }
  return own;
}

void CoopScreen::draw_presence(const int slot, const Viewport &v) {
  const auto &self = players_[slot];
  if (!self.runner) {
    return;
  }
  auto header = v;
  header.scale = static_cast<float>(v.pixel_width) / 640.0F;
  header.offset_x = header.offset_y = header.origin_y = 0;
  SDL_Rect content;
  SDL_GetRenderViewport(renderer_, &content);
  SDL_SetRenderViewport(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, 10, 14, 22, 255);
  const SDL_FRect bar{0, 0, static_cast<float>(v.pixel_width), static_cast<float>(content.y)};
  SDL_RenderFillRect(renderer_, &bar);
  text(header, "P" + std::to_string(slot + 1) + (self.connected ? "" : " CONTROLLER DISCONNECTED"),
       8, 1, 10, {255, 230, 80, 255});
  float y = 14;
  if (state_->online) {
    draw_online_presence(header);
    return;
  }
  for (int other = 0; other < 4; ++other) {
    const auto &peer = players_[other];
    if (other == slot || !peer.runner) {
      continue;
    }
    const bool together = self.runner->screen() == peer.runner->screen();
    const auto &save = peer.runner->quest_save();
    const auto selection = peer.runner->active_screen()->menu_presence();
    std::optional<std::array<float, 2>> at;
    bool busy = !together;
    if (self.runner->screen() == "town_map") {
      if (peer.runner->screen() == "town_map" && selection) {
        at = selection->position;
      } else {
        for (const auto &place : map_.places) {
          if (place.next == peer.runner->coop_location()) {
            at = place.rider;
          }
        }
      }
    } else if (const auto presence = peer.runner->coop_presence(self.runner->screen())) {
      if (const auto placed = self.runner->active_screen()->peer_presence(*presence)) {
        at = placed->position;
        busy = placed->busy;
      }
    }
    if (at) {
      Sprite rider = map_.rider;
      rider.position = {(*at)[0] - 18.0F + 14.0F * other, (*at)[1] - 20.0F};
      rider.size = std::array{24.0F, 28.0F};
      rider.angle = 0;
      const std::uint32_t colours[] = {0x80ddff, 0xffb080, 0xb0ff80, 0xff80e0};
      SDL_SetRenderViewport(renderer_, &content);
      draw_sprite(textures_, rider, v, 0, 1, busy ? 0x888888 : colours[other]);
      text(v, "P" + std::to_string(other + 1), rider.position[0], rider.position[1] - 9, 9);
      SDL_SetRenderViewport(renderer_, nullptr);
    }
    text(header,
         "P" + std::to_string(other + 1) + " " + save.player_name + " - " +
             screen_label(peer.runner->screen()) +
             (together && selection ? ": " + screen_label(selection->selection) : "") + " / DAY " +
             std::to_string(save.day + 1),
         8, y, 8, together ? SDL_Color{100, 220, 255, 255} : SDL_Color{160, 160, 160, 255});
    y += 9;
  }
}

void CoopScreen::draw(const Viewport &viewport) {
  viewport_ = viewport;
  if (draw_online_wait(viewport)) {
    return;
  }
  const int count = state_->online ? 1
                    : playing_     ? static_cast<int>(std::ranges::count_if(
                                     players_, [](const Player &p) { return p.port >= 0; }))
                                   : 4;
  if (!count) {
    return;
  }
  const auto panes = coop_panes(count, viewport.pixel_width, viewport.pixel_height);
  SDL_Texture *previous = SDL_GetRenderTarget(renderer_);
  int pane = 0;
  for (int slot = 0; slot < 4; ++slot) {
    auto &player = players_[slot];
    if ((state_->online && slot != online_slot_) ||
        (!state_->online && playing_ && player.port < 0)) {
      continue;
    }
    const auto &rect = panes.at(pane++).rect;
    const int width = static_cast<int>(rect.width), height = static_cast<int>(rect.height);
    if (!player.target || player.width != width || player.height != height) {
      SDL_DestroyTexture(player.target);
      player.target = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, width, height);
      if (!player.target) {
        throw std::runtime_error(SDL_GetError());
      }
      SDL_SetTextureBlendMode(player.target, SDL_BLENDMODE_NONE);
      player.width = width;
      player.height = height;
    }
    SDL_SetRenderTarget(renderer_, player.target);
    SDL_SetRenderDrawColor(renderer_, 10, 14, 22, 255);
    SDL_RenderClear(renderer_);
    const Viewport local = player_canvas(width, height, playing_);
    const SDL_Rect content{0, height - local.pixel_height, width, local.pixel_height};
    SDL_SetRenderViewport(renderer_, &content);
    if (player.runner && !player.save_error && (playing_ || !player.ready)) {
      player.runner->draw(player.runner->placed(local));
      if (playing_) {
        draw_presence(slot, local);
      }
    } else {
      text(local,
           std::string(state_->online ? "ONLINE / PLAYER " : "CO-OP / PLAYER ") +
               std::to_string(slot + 1),
           48, 65, 22);
      if (player.port < 0) {
        text(local, "Press START to join", 48, 165);
        text(local, "BACK (Esc): main menu", 48, 230, 11);
      } else if (player.save_error) {
        text(local, "Unable to save this co-op profile.", 48, 165);
        text(local, "A: retry   B: change save", 48, 230, 11);
      } else if (player.ready) {
        const auto &save = player.runner->quest_save();
        text(local, save.player_name + " - DAY " + std::to_string(save.day + 1), 48, 165);
        text(local, "READY", 48, 210, 20, {100, 230, 150, 255});
        text(local,
             state_->online ? "START: enter city"
             : slot == 0    ? "START: start game when everyone is ready"
                            : "Waiting for Player 1",
             48, 270, 11);
        text(local, "B: change save", 48, 305, 11);
      } else {
        text(local, "< " + choice_label(slot) + " >", 48, 165);
        text(local, "Left/Right: choose   A: load   B: leave", 48, 230, 11);
        text(local, "Solo saves are imported as separate co-op copies.", 48, 280, 10);
      }
    }
    SDL_SetRenderViewport(renderer_, nullptr);
    SDL_SetRenderTarget(renderer_, previous);
    const SDL_FRect target{rect.x + viewport.origin_x, rect.y + viewport.origin_y, rect.width,
                           rect.height};
    SDL_RenderTexture(renderer_, player.target, nullptr, &target);
  }
}

} // namespace phantomcrash
