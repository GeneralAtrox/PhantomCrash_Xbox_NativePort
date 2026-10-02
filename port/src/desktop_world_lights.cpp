#include "desktop_world_renderer.hpp"
#include <exception>

namespace phantomcrash {
void WorldRenderer::render_lights(const WorldFrame &frame) {
  if (!post_process_.dynamic_lighting || dynamic_failed_) {
    return;
  }
  try {
    if (!dynamic_lights_) {
      dynamic_lights_ = std::make_unique<DynamicLightPass>(device_.Get());
      SDL_Log("Dynamic lighting: eight 1024px shadow maps allocated");
    }
    const unsigned count = dynamic_lights_->prepare(context_.Get(), frame.lights);
    std::vector<Draw> draws;
    for (const auto &item : frame.items) {
      if (item.ground_shadow || item.rainbow || item.additive || item.alpha < 1 || item.optical) {
        continue;
      }
      item_draws(item, -1, frame.eye, draws);
    }
    for (unsigned i = 0; i < count; ++i) {
      dynamic_lights_->begin_shadow(context_.Get(), i);
      for (const auto &draw : draws) {
        // Authored transparent glow/surface layers aren't solid occluders.
        if (draw.sky_node || draw.monitor || draw.blend || draw.draw->pass != 0) {
          continue;
        }
        dynamic_lights_->caster(context_.Get(), *draw.model, *draw.draw, draw.world);
      }
    }
    context_->OMSetRenderTargets(0, nullptr, nullptr);
  } catch (const std::exception &error) {
    dynamic_failed_ = true;
    SDL_Log("Dynamic lighting disabled after failure: %s", error.what());
  }
}
} // namespace phantomcrash
