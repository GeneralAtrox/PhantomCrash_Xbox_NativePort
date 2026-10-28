#include "desktop_model_zoo.hpp"

#include "desktop_world_renderer.hpp"
#include "phantomcrash/robot_library.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace phantomcrash {
namespace {

// The grid, and the big view confirm toggles to.
constexpr std::array<std::array<int, 2>, 2> layouts{{{6, 4}, {3, 2}}};
constexpr float title_height = 24.0F; // canvas pixels above the grid
constexpr float caption_height = 10.0F;
constexpr std::array<const char *, 4> kind_names{"BODIES", "LEGS", "ARMS", "SHOULDERS"};
constexpr std::array<PartKind, 4> kinds{PartKind::body, PartKind::legs, PartKind::arm,
                                        PartKind::shoulder};

// One camera for every cell, so sizes and offsets compare: in front of the
// port's forward (-z), from its +x side and above.
constexpr Vec3 eye{5.5F, 4.5F, -10.0F};
constexpr Vec3 target{0.0F, 1.5F, 0.0F};
constexpr Vec3 up{0.0F, 1.0F, 0.0F};
constexpr float fov_degrees = 40.0F;
constexpr float axis_length = 3.0F;

// The first part of each type: its id, -1 for none.
std::vector<int> first_parts(const std::vector<int> &parts, const std::size_t types) {
  std::vector<int> first(types, -1);
  for (std::size_t id = 0; id < parts.size(); ++id) {
    const int type = parts[id];
    if (type >= 0 && static_cast<std::size_t>(type) < types &&
        first[static_cast<std::size_t>(type)] < 0) {
      first[static_cast<std::size_t>(type)] = static_cast<int>(id);
    }
  }
  return first;
}

void debug_text(SDL_Renderer *renderer, const std::string &text, const float x, const float y,
                const float scale) {
  SDL_SetRenderScale(renderer, scale, scale);
  SDL_RenderDebugText(renderer, x / scale, y / scale, text.c_str());
  SDL_SetRenderScale(renderer, 1.0F, 1.0F);
}

} // namespace

ModelZooScreen::ModelZooScreen(SDL_Renderer *renderer, PreviewWorld world, const PartTables &parts,
                               const GarageTables &garage)
    : renderer_(renderer), world_(world), parts_(parts), garage_(garage) {
  if (world_.world == nullptr || !world_.world->available() || world_.robots == nullptr ||
      !world_.robots->available()) {
    world_ = {};
  }
  build(kinds[0]);
}

void ModelZooScreen::build(const PartKind kind) {
  exhibits_.clear();
  page_ = 0;
  if (world_.robots == nullptr) {
    return;
  }
  const RobotCatalogue &catalogue = world_.robots->catalogue();
  const auto add = [&](const char *tag, const std::size_t type, const int resource, const int part,
                       const std::string &side) {
    Exhibit exhibit;
    exhibit.caption = std::string(tag) + std::to_string(type) + side + " r" +
                      std::to_string(resource) + " " +
                      (part >= 0 ? part_name(parts_, garage_, kind, part) : std::string{});
    if (resource >= 0) {
      exhibit.model = world_.world->model(robot_model_path(resource));
    }
    exhibits_.push_back(std::move(exhibit));
  };
  switch (kind) {
  case PartKind::body: {
    const auto first = first_parts(catalogue.body_parts, catalogue.body_types.size());
    for (std::size_t type = 0; type < catalogue.body_types.size(); ++type) {
      add("B", type, catalogue.body_types[type].model, first[type], "");
    }
    break;
  }
  case PartKind::legs: {
    const auto first = first_parts(catalogue.leg_parts, catalogue.leg_types.size());
    for (std::size_t type = 0; type < catalogue.leg_types.size(); ++type) {
      add("L", type, catalogue.leg_types[type].model, first[type], "");
    }
    break;
  }
  case PartKind::arm: {
    const auto first = first_parts(catalogue.arm_parts, catalogue.arm_types.size());
    for (std::size_t type = 0; type < catalogue.arm_types.size(); ++type) {
      add("A", type, catalogue.arm_types[type].models[0], first[type], "R");
      add("A", type, catalogue.arm_types[type].models[1], first[type], "L");
    }
    break;
  }
  default: {
    const auto first = first_parts(catalogue.shoulder_parts, catalogue.shoulder_types.size());
    for (std::size_t type = 0; type < catalogue.shoulder_types.size(); ++type) {
      add("S", type, catalogue.shoulder_types[type].models[0], first[type], "R");
      add("S", type, catalogue.shoulder_types[type].models[1], first[type], "L");
    }
    break;
  }
  }
}

int ModelZooScreen::per_page() const {
  const auto &[columns, rows] = layouts[big_ ? 1 : 0];
  return columns * rows;
}

int ModelZooScreen::page_count() const {
  return std::max(1, (static_cast<int>(exhibits_.size()) + per_page() - 1) / per_page());
}

void ModelZooScreen::tick(const InputFrame &input, double /*tick_seconds*/) {
  if (input.any_pressed(button::cancel)) {
    done_ = true;
    return;
  }
  if (input.any_pressed(button::confirm)) {
    const int first = page_ * per_page();
    big_ = !big_;
    page_ = first / per_page();
    return;
  }
  const int kind_count = static_cast<int>(kinds.size());
  if (input.any_pressed(button::up | button::down)) {
    const int step = input.any_pressed(button::down) ? 1 : kind_count - 1;
    kind_ = (kind_ + step) % kind_count;
    build(kinds[static_cast<std::size_t>(kind_)]);
    return;
  }
  const int pages = page_count();
  if (input.any_pressed(button::left | button::right)) {
    const int step = input.any_pressed(button::right) ? 1 : pages - 1;
    page_ = (page_ + step) % pages;
  }
}

// The model at the world's origin under the zoo's camera, then its axes.
void ModelZooScreen::draw_cell(const PixelRect &cell, const Exhibit &exhibit) const {
  const SDL_Rect rect{static_cast<int>(std::lround(cell.x)), static_cast<int>(std::lround(cell.y)),
                      static_cast<int>(std::lround(cell.width)),
                      static_cast<int>(std::lround(cell.height))};
  if (rect.w <= 0 || rect.h <= 0) {
    return;
  }
  const Mat4 view = look_at(eye, target, up);
  const Mat4 projection = perspective(radians(fov_degrees), cell.width / cell.height, 0.5F, 500.0F);
  if (exhibit.model >= 0) {
    SDL_SetRenderViewport(renderer_, &rect);
    WorldFrame frame;
    frame.width = rect.w;
    frame.height = rect.h;
    frame.eye = eye;
    frame.view = view;
    frame.projection = projection;
    frame.lighting.ambient = {0.45F, 0.45F, 0.45F};
    frame.lighting.sun_colour = {0.8F, 0.8F, 0.8F};
    frame.lighting.sun_direction = normalize(Vec3{-0.4F, -1.0F, 0.5F});
    frame.lighting.fog = false;
    WorldItem item;
    item.model = exhibit.model;
    frame.items.push_back(std::move(item));
    frame.transparent = true;
    world_.world->draw(frame);
    SDL_SetRenderViewport(renderer_, nullptr);
  }
  const Mat4 to_clip = view * projection;
  const auto pixel = [&](const Vec3 p) {
    const Vec3 c = transform_point(p, to_clip);
    const float w =
        p.x * to_clip.at(0, 3) + p.y * to_clip.at(1, 3) + p.z * to_clip.at(2, 3) + to_clip.at(3, 3);
    return SDL_FPoint{cell.x + (c.x / w * 0.5F + 0.5F) * cell.width,
                      cell.y + (0.5F - c.y / w * 0.5F) * cell.height};
  };
  const SDL_FPoint origin = pixel({});
  const std::array<std::pair<Vec3, SDL_Color>, 3> axes{
      {{{axis_length, 0.0F, 0.0F}, {255, 60, 60, 255}},
       {{0.0F, axis_length, 0.0F}, {60, 255, 60, 255}},
       {{0.0F, 0.0F, -axis_length}, {80, 140, 255, 255}}}};
  for (const auto &[end, colour] : axes) {
    const SDL_FPoint to = pixel(end);
    SDL_SetRenderDrawColor(renderer_, colour.r, colour.g, colour.b, colour.a);
    SDL_RenderLine(renderer_, origin.x, origin.y, to.x, to.y);
  }
}

void ModelZooScreen::draw(const Viewport &viewport) {
  SDL_SetRenderDrawColor(renderer_, 40, 40, 48, 255);
  SDL_RenderClear(renderer_);
  const float scale = viewport.scale;
  SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
  const int pages = page_count();
  const std::string title = std::string("MODEL ZOO  ") +
                            kind_names[static_cast<std::size_t>(kind_)] + "  page " +
                            std::to_string(page_ + 1) + "/" + std::to_string(pages) +
                            "  (up/down kind, left/right page, A big view; blue = forward -z)";
  debug_text(renderer_, title, viewport.to_pixel_x(8.0F), viewport.to_pixel_y(8.0F), scale);
  if (world_.world == nullptr) {
    debug_text(renderer_, "No 3D world or robot library", viewport.to_pixel_x(8.0F),
               viewport.to_pixel_y(40.0F), scale);
    return;
  }
  const auto [columns, rows] = layouts[big_ ? 1 : 0];
  const float cell_width = 640.0F / static_cast<float>(columns);
  const float cell_height = (480.0F - title_height) / static_cast<float>(rows);
  for (int slot = 0; slot < per_page(); ++slot) {
    const auto index = static_cast<std::size_t>(page_ * per_page() + slot);
    if (index >= exhibits_.size()) {
      break;
    }
    const float x = cell_width * static_cast<float>(slot % columns);
    const float y = title_height + cell_height * static_cast<float>(slot / columns);
    const PixelRect cell{viewport.to_pixel_x(x + 1.0F), viewport.to_pixel_y(y + caption_height),
                         (cell_width - 2.0F) * scale,
                         (cell_height - caption_height - 1.0F) * scale};
    SDL_SetRenderDrawColor(renderer_, 24, 24, 30, 255);
    const SDL_FRect back{cell.x, cell.y, cell.width, cell.height};
    SDL_RenderFillRect(renderer_, &back);
    draw_cell(cell, exhibits_[index]);
    SDL_SetRenderDrawColor(renderer_, 230, 230, 230, 255);
    debug_text(renderer_, exhibits_[index].caption.substr(0, big_ ? 32 : 26),
               viewport.to_pixel_x(x + 2.0F), viewport.to_pixel_y(y + 1.0F),
               scale * (big_ ? 0.8F : 0.5F));
  }
}

} // namespace phantomcrash
