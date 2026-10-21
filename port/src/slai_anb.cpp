#include "phantomcrash/slai_anb.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace phantomcrash {
namespace {

// A step never runs more commands than this, so a bank whose commands all
// last zero ticks cannot hang the scene.
constexpr int max_commands_per_step = 256;
constexpr float gs_one = 128.0F;
constexpr float byte_one = 255.0F;

float number(const json::JsonValue &value, std::string_view key, float fallback = 0.0F) {
  return static_cast<float>(value.number_or(key, fallback));
}

AnbBlend blend_from(const std::string &name) {
  if (name == "opaque") {
    return AnbBlend::opaque;
  }
  return name == "additive" ? AnbBlend::additive : AnbBlend::alpha;
}

AnbCell cell_from(const json::JsonValue &value) {
  AnbCell cell;
  cell.texture = static_cast<int>(value.number_or("texture", 0));
  cell.u0 = number(value, "u0");
  cell.v0 = number(value, "v0");
  cell.u1 = number(value, "u1");
  cell.v1 = number(value, "v1");
  cell.x = number(value, "x");
  cell.y = number(value, "y");
  cell.half_width = number(value, "half_width");
  cell.half_height = number(value, "half_height");
  cell.scale_x = number(value, "scale_x", 1.0F);
  cell.scale_y = number(value, "scale_y", 1.0F);
  cell.rotation = number(value, "rotation");
  cell.origin_x = number(value, "origin_x");
  cell.origin_y = number(value, "origin_y");
  cell.scale2_x = number(value, "scale2_x", 1.0F);
  cell.scale2_y = number(value, "scale2_y", 1.0F);
  cell.rotation2 = number(value, "rotation2");
  cell.blend = blend_from(value.string_or("blend", "alpha"));
  if (const auto *rgba = value.find("rgba"); rgba != nullptr && rgba->is_array()) {
    const auto &items = rgba->array();
    for (std::size_t i = 0; i < cell.rgba.size() && i < items.size(); ++i) {
      cell.rgba[i] = static_cast<float>(items[i].number());
    }
  }
  return cell;
}

AnbCommand command_from(const json::JsonValue &value) {
  AnbCommand command;
  const auto op = value.string_or("op", "end");
  if (op == "frame") {
    command.op = AnbOp::frame;
  } else if (op == "tween") {
    command.op = AnbOp::tween;
  } else if (op == "loop") {
    command.op = AnbOp::loop;
  } else if (op == "jump") {
    command.op = AnbOp::jump;
  }
  command.a = number(value, op == "jump" ? "anim" : "a");
  command.b = number(value, "b");
  command.c = number(value, "c");
  return command;
}

float lerp(float next, float current, float t) {
  return next + t * (current - next);
}

AnbCell blend_cells(const AnbCell &current, const AnbCell &next, float t) {
  AnbCell out = current;
  if (current.blend != AnbBlend::additive) {
    out.u0 = lerp(next.u0, current.u0, t);
    out.v0 = lerp(next.v0, current.v0, t);
    out.u1 = lerp(next.u1, current.u1, t);
    out.v1 = lerp(next.v1, current.v1, t);
  }
  out.rotation = lerp(next.rotation, current.rotation, t);
  out.rotation2 = lerp(next.rotation2, current.rotation2, t);
  out.origin_x = lerp(next.origin_x, current.origin_x, t);
  out.origin_y = lerp(next.origin_y, current.origin_y, t);
  out.scale2_x = lerp(next.scale2_x, current.scale2_x, t);
  out.scale2_y = lerp(next.scale2_y, current.scale2_y, t);
  out.x = lerp(next.x, current.x, t);
  out.y = lerp(next.y, current.y, t);
  out.half_width = lerp(next.half_width, current.half_width, t);
  out.half_height = lerp(next.half_height, current.half_height, t);
  out.scale_x = lerp(next.scale_x, current.scale_x, t);
  out.scale_y = lerp(next.scale_y, current.scale_y, t);
  for (std::size_t i = 0; i < out.rgba.size(); ++i) {
    out.rgba[i] = lerp(next.rgba[i], current.rgba[i], t);
  }
  return out;
}

struct Point {
  float x{}, y{};
};

Point rotate(Point p, float degrees) {
  const float radians = degrees * std::numbers::pi_v<float> / 180.0F;
  const float c = std::cos(radians);
  const float s = std::sin(radians);
  return {p.x * c - p.y * s, p.x * s + p.y * c};
}

// 0x1CDD30's corner transform: scale and rotate about the pivot, place it,
// then scale and rotate again about the origin. Cell y is up; the result
// is y down, relative to the object.
Point transform(const AnbCell &cell, Point corner) {
  Point p = rotate({corner.x * cell.scale_x, corner.y * cell.scale_y}, cell.rotation);
  p = {p.x + cell.x - cell.origin_x, p.y + cell.origin_y - cell.y};
  p = rotate({p.x * cell.scale2_x, p.y * cell.scale2_y}, cell.rotation2);
  return {p.x + cell.origin_x, p.y - cell.origin_y};
}

const std::vector<int> &frame_cells(const AnbBank &bank, int frame) {
  static const std::vector<int> none;
  if (frame < 0 || frame >= static_cast<int>(bank.frames.size())) {
    return none;
  }
  return bank.frames[static_cast<std::size_t>(frame)];
}

} // namespace

AnbBank anb_from_json(const json::JsonValue &value) {
  AnbBank bank;
  bank.textures = static_cast<int>(value.number_or("textures", 0));
  const auto *cells = value.find("cells");
  const auto *frames = value.find("frames");
  const auto *anims = value.find("anims");
  if (cells == nullptr || frames == nullptr || anims == nullptr) {
    throw std::runtime_error("ANB JSON lacks cells, frames or anims");
  }
  for (const auto &cell : cells->array()) {
    bank.cells.push_back(cell_from(cell));
  }
  for (const auto &frame : frames->array()) {
    auto &indices = bank.frames.emplace_back();
    if (frame.is_array()) {
      for (const auto &index : frame.array()) {
        const int cell = static_cast<int>(index.number());
        if (cell >= 0 && cell < static_cast<int>(bank.cells.size())) {
          indices.push_back(cell);
        }
      }
    }
  }
  for (const auto &anim : anims->array()) {
    auto &commands = bank.anims.emplace_back();
    for (const auto &command : anim.array()) {
      commands.push_back(command_from(command));
    }
  }
  return bank;
}

void AnbObject::reset(bool add) {
  command = 0;
  tween = false;
  const float first = bank != nullptr && anim < static_cast<int>(bank->anims.size()) &&
                              !bank->anims[static_cast<std::size_t>(anim)].empty()
                          ? bank->anims[static_cast<std::size_t>(anim)].front().b
                          : 0.0F;
  timer = add ? timer + first : first;
}

void AnbObject::set_anim(int value) {
  anim = value;
  reset(false);
}

void AnbObject::play(int value, unsigned set, unsigned clear) {
  reset(false);
  set_anim(value);
  flags = (flags | set) & ~clear;
}

void AnbObject::step(float ticks) {
  if (bank == nullptr || has(anb_flag::hidden | anb_flag::paused) || anim < 0 ||
      anim >= static_cast<int>(bank->anims.size())) {
    return;
  }
  if (timer > 0.0F) {
    timer -= speed * ticks;
  }
  for (int guard = 0; timer <= 0.0F && guard < max_commands_per_step; ++guard) {
    const auto &commands = bank->anims[static_cast<std::size_t>(anim)];
    const int count = static_cast<int>(commands.size());
    command = std::min(command + 1, count - 1);
    tween = false;
    const AnbCommand &now = commands[static_cast<std::size_t>(command)];
    const auto hold_last = [&] {
      command = std::max(command - 1, 0);
      const float last = commands[static_cast<std::size_t>(command)].b;
      timer += last > 0.0F ? last : 1.0F;
    };
    switch (now.op) {
    case AnbOp::end:
      flags |= anb_flag::ended;
      if (!has(anb_flag::hold)) {
        reset(false);
        flags |= anb_flag::hidden;
        return;
      }
      hold_last();
      break;
    case AnbOp::jump:
      anim = std::clamp(static_cast<int>(now.a), 0, static_cast<int>(bank->anims.size()) - 1);
      reset(true);
      break;
    case AnbOp::loop:
      flags |= anb_flag::ended;
      if (!has(anb_flag::no_loop)) {
        reset(true);
      } else if (!has(anb_flag::hold)) {
        reset(false);
        flags |= anb_flag::hidden;
        return;
      } else {
        hold_last();
      }
      break;
    case AnbOp::tween: {
      tween = true;
      tween_ticks = now.a * now.b;
      tween_ease = now.c;
      for (int i = 1; i <= count; ++i) {
        const auto &next = commands[static_cast<std::size_t>((command + i) % count)];
        if (next.op == AnbOp::frame) {
          tween_target = static_cast<int>(next.a);
          break;
        }
      }
      timer += tween_ticks;
      break;
    }
    case AnbOp::frame:
      timer += now.b;
      break;
    }
  }
}

std::array<float, 4> gs_rgba(std::uint32_t colour) {
  std::array<float, 4> out{};
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<float>((colour >> (8 * i)) & 0xFFU) / gs_one;
  }
  return out;
}

std::vector<AnbQuad> anb_quads(const AnbObject &object) {
  std::vector<AnbQuad> quads;
  if (!object.visible() || object.anim < 0 ||
      object.anim >= static_cast<int>(object.bank->anims.size())) {
    return quads;
  }
  const AnbBank &bank = *object.bank;
  const auto &commands = bank.anims[static_cast<std::size_t>(object.anim)];
  if (commands.empty()) {
    return quads;
  }
  const int shown =
      std::clamp(object.command - (object.tween ? 1 : 0), 0, static_cast<int>(commands.size()) - 1);
  const auto &cells = frame_cells(bank, static_cast<int>(commands[shown].a));
  const auto &next = object.tween ? frame_cells(bank, object.tween_target) : cells;
  float t = object.tween && object.tween_ticks > 0.0F
                ? std::clamp(object.timer / object.tween_ticks, 0.0F, 1.0F)
                : 1.0F;
  const float quarter = t * std::numbers::pi_v<float> / 2.0F;
  if (object.tween && object.tween_ease == 1.0F) {
    t = std::sin(quarter);
  } else if (object.tween && object.tween_ease == 2.0F) {
    t = 1.0F - std::cos(quarter);
  }
  const auto tint = gs_rgba(object.colour);
  for (std::size_t i = cells.size(); i-- > 0;) {
    AnbCell cell = bank.cells[static_cast<std::size_t>(cells[i])];
    if (object.tween && i < next.size()) {
      cell = blend_cells(cell, bank.cells[static_cast<std::size_t>(next[i])], t);
    }
    AnbQuad quad;
    quad.texture = cell.texture;
    quad.blend = cell.blend;
    const float hw = cell.half_width;
    const float hh = cell.half_height;
    const std::array<Point, 4> corners{{{-hw, -hh}, {hw, -hh}, {-hw, hh}, {hw, hh}}};
    const std::array<Point, 4> uvs{
        {{cell.u0, cell.v0}, {cell.u1, cell.v0}, {cell.u0, cell.v1}, {cell.u1, cell.v1}}};
    std::array<float, 4> colour{};
    for (std::size_t c = 0; c < colour.size(); ++c) {
      colour[c] = cell.rgba[c] / byte_one * tint[c];
    }
    for (std::size_t c = 0; c < corners.size(); ++c) {
      const Point p = transform(cell, corners[c]);
      quad.corners[c] = {object.x + p.x * object.scale_x, object.y + p.y * object.scale_y, uvs[c].x,
                         uvs[c].y, colour};
    }
    quads.push_back(quad);
  }
  return quads;
}

} // namespace phantomcrash
