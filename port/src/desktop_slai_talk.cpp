#include "desktop_slai_talk.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace phantomcrash {
namespace {

// The PS2 screen the scenes are laid out in, stretched over the canvas as
// the original's output fills a 4:3 display.
constexpr float ps2_width = 512.0F, ps2_height = 448.0F;
constexpr double scene_rate = 60.0;
// A tick never runs more than this many scene frames (after a stall).
constexpr int max_frames_per_tick = 4;
// The port's talk buttons: A or B pages (the original's X and circle),
// both triggers held hurry the typing, START skips the scene.
constexpr Buttons advance_buttons = button::a | button::b;
constexpr Buttons hurry_buttons = button::left_trigger | button::right_trigger;
// Keys for the ambient loops' voices ("SLAI" over the loop's id).
constexpr std::uint64_t loop_key_base = 0x534C414900000000ULL;

SDL_FColor colour(const std::array<float, 4> &rgba) {
  return {std::clamp(rgba[0], 0.0F, 1.0F), std::clamp(rgba[1], 0.0F, 1.0F),
          std::clamp(rgba[2], 0.0F, 1.0F), std::clamp(rgba[3], 0.0F, 1.0F)};
}

} // namespace

SlaiTalkScreen::SlaiTalkScreen(TextureCache &textures, std::unique_ptr<SlaiTalkScene> scene,
                               std::string next, std::unique_ptr<Screen> behind)
    : textures_(textures), scene_(std::move(scene)), next_(std::move(next)),
      behind_(std::move(behind)) {}

void SlaiTalkScreen::tick(const InputFrame &input, const double tick_seconds) {
  if (behind_) {
    behind_->tick(InputFrame{}, tick_seconds); // the town goes on under the talk
  }
  pending_frames_ += tick_seconds * scene_rate;
  bool first = true;
  for (int frame = 0; pending_frames_ >= 1.0 && frame < max_frames_per_tick; ++frame) {
    pending_frames_ -= 1.0;
    SlaiTalkInput talk;
    talk.advance = first && (input.pressed & advance_buttons) != 0;
    talk.hurry = (input.held & hurry_buttons) == hurry_buttons;
    talk.skip_all = first && (input.pressed & button::start) != 0;
    first = false;
    scene_->tick(talk);
    if (scene_->done()) {
      break;
    }
  }
  pending_frames_ = std::min(pending_frames_, 1.0);
}

void SlaiTalkScreen::draw(const Viewport &viewport) {
  if (behind_) {
    behind_->draw(viewport);
  }
  SDL_Renderer *renderer = textures_.renderer();
  // A talk scene is the PS2 screen's picture, cut at its edges; a town
  // scene's black cells and text bar reach a wide window's edges.
  const bool clipped = scene_->mode() == SlaiTalkMode::talk;
  if (clipped) {
    const PixelRect content = viewport.content();
    const SDL_Rect clip{static_cast<int>(content.x), static_cast<int>(content.y),
                        static_cast<int>(content.width), static_cast<int>(content.height)};
    SDL_SetRenderClipRect(renderer, &clip);
  }
  const float sx = canvas_width / ps2_width;
  const float sy = canvas_height / ps2_height;
  for (const auto &quad : scene_->draw()) {
    SDL_Texture *texture = quad.texture.empty() ? nullptr : textures_.get(quad.texture);
    if (!quad.texture.empty() && texture == nullptr) {
      continue;
    }
    float width = 1.0F, height = 1.0F;
    if (texture != nullptr) {
      SDL_GetTextureSize(texture, &width, &height);
      SDL_SetTextureBlendMode(texture, quad.blend == AnbBlend::additive ? SDL_BLENDMODE_ADD
                                                                        : SDL_BLENDMODE_BLEND);
      SDL_SetTextureColorModFloat(texture, 1.0F, 1.0F, 1.0F);
      SDL_SetTextureAlphaModFloat(texture, 1.0F);
    }
    std::array<SDL_Vertex, 4> vertices{};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      const auto &corner = quad.corners[i];
      vertices[i].position = {viewport.to_pixel_x(corner.x * sx),
                              viewport.to_pixel_y(corner.y * sy)};
      vertices[i].color = colour(corner.rgba);
      vertices[i].tex_coord = {corner.u / width, corner.v / height};
    }
    // Corners are top left, top right, bottom left, bottom right.
    constexpr std::array<int, 6> indices{0, 1, 2, 1, 3, 2};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, texture, vertices.data(), static_cast<int>(vertices.size()),
                       indices.data(), static_cast<int>(indices.size()));
    if (texture != nullptr) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
  }
  if (clipped) {
    SDL_SetRenderClipRect(renderer, nullptr);
  }
}

std::string SlaiTalkScreen::music() const {
  // Before the script's first BGM op the music playing goes on.
  return scene_->music().empty() ? "keep" : scene_->music();
}

std::vector<std::string> SlaiTalkScreen::take_sounds() {
  return scene_->take_sounds();
}

std::vector<SoundCue> SlaiTalkScreen::take_sound_cues() {
  std::vector<SoundCue> cues;
  const auto &loops = scene_->loops();
  for (auto sent = loop_gains_.begin(); sent != loop_gains_.end();) {
    const bool kept = std::ranges::any_of(
        loops, [&sent](const SlaiTalkLoop &loop) { return loop.id == sent->first; });
    if (kept) {
      ++sent;
      continue;
    }
    cues.push_back({SoundCue::Action::stop,
                    {},
                    {},
                    false,
                    loop_key_base + static_cast<std::uint64_t>(sent->first)});
    sent = loop_gains_.erase(sent);
  }
  for (const auto &loop : loops) {
    const auto key = loop_key_base + static_cast<std::uint64_t>(loop.id);
    const auto sent = loop_gains_.find(loop.id);
    if (sent != loop_gains_.end() && sent->second == loop.gain()) {
      continue;
    }
    SoundCue cue{sent == loop_gains_.end() ? SoundCue::Action::play : SoundCue::Action::move,
                 loop.sound, SoundMix{loop.gain()}, true, key};
    cue.bus = sound_bus();
    cues.push_back(std::move(cue));
    loop_gains_[loop.id] = loop.gain();
  }
  return cues;
}

std::optional<SceneProgress> SlaiTalkScreen::scene_progress() const {
  return SceneProgress{"slai talk", scene_->pc(), scene_->script_size(), "op"};
}

} // namespace phantomcrash
