#pragma once

#include "desktop_robot_preview.hpp"
#include "desktop_screen.hpp"
#include "phantomcrash/garage_tables.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

namespace phantomcrash {

// A port tool (--start model_zoo): every robot part type's model as it
// loads, with no placement, all under one camera, a page of cells at a
// time. Each cell draws the world's axes through the model's origin: +x
// red, +y green, and the port's forward (-z) blue. A model that stands
// upright on its origin facing the blue line is in the port's frame. The
// cell's caption names the part kind, type, model resource and the first
// part of that type.
//
// up/down choose the part kind, left/right the page, confirm toggles a big
// view of six; cancel leaves for the title. The camera looks at the robots' fronts from the front
// right.
class ModelZooScreen final : public Screen {
public:
  ModelZooScreen(SDL_Renderer *renderer, PreviewWorld world, const PartTables &parts,
                 const GarageTables &garage);

  void tick(const InputFrame &input, double tick_seconds) override;
  void draw(const Viewport &viewport) override;
  [[nodiscard]] bool done() const override {
    return done_;
  }
  [[nodiscard]] std::string next() const override {
    return "title";
  }

private:
  struct Exhibit {
    std::string caption;
    int model{-1}; // the renderer's, -1 when it does not load
  };

  void build(PartKind kind);
  [[nodiscard]] int per_page() const;
  [[nodiscard]] int page_count() const;
  void draw_cell(const PixelRect &cell, const Exhibit &exhibit) const;

  SDL_Renderer *renderer_{};
  PreviewWorld world_;
  const PartTables &parts_;
  const GarageTables &garage_;
  int kind_{};
  int page_{};
  bool big_{};
  std::vector<Exhibit> exhibits_;
  bool done_{};
};

} // namespace phantomcrash
