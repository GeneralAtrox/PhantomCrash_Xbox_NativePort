// Per-part appearance in ASSEMBLE (a port addition, part_looks.hpp): in the
// list, left and right turn the chosen part's look, which the robot then
// tries on; the stock entry keeps it.

#include "phantomcrash/assemble.hpp"
#include "phantomcrash/shop_pointer.hpp"

namespace phantomcrash {

bool Assemble::offers_look(const ShopGoods &line) const {
  if (!script_.look_arrows || line.stock < 0) {
    return false;
  }
  const auto found = quest_.garage.stock.find(line.stock);
  return found != quest_.garage.stock.end() && looks_.offers(found->second.kind, found->second.id);
}

const ShopGoods *Assemble::tried_on() const {
  const ShopGoods *line = chosen();
  if (!in_list_ || line == nullptr || tried_on_ < 0 || line->stock != tried_on_) {
    return nullptr;
  }
  return line;
}

Hangar Assemble::shown_hangar() const {
  Hangar shown = fitted();
  const ShopGoods *line = tried_on();
  const auto kind = line != nullptr ? stock_kind(line->goods.kind) : std::nullopt;
  if (!kind || line->hangar == hangar_) {
    return shown;
  }
  const bool right = side_ == 0;
  switch (*kind) {
  case PartKind::body:
    shown.parts[static_cast<std::size_t>(PartSlot::body)] = line->stock;
    break;
  case PartKind::legs:
    shown.parts[static_cast<std::size_t>(PartSlot::legs)] = line->stock;
    break;
  case PartKind::arm:
    shown.parts[static_cast<std::size_t>(right ? PartSlot::right_arm : PartSlot::left_arm)] =
        line->stock;
    break;
  case PartKind::shoulder:
    shown.parts[static_cast<std::size_t>(right ? PartSlot::right_shoulder
                                               : PartSlot::left_shoulder)] = line->stock;
    break;
  case PartKind::chip:
  case PartKind::module:
    break;
  }
  return shown;
}

bool Assemble::turn_look(const double tick_seconds, const InputFrame &input) {
  const ShopGoods *line = chosen();
  if (line == nullptr || !offers_look(*line)) {
    return false;
  }
  bool fresh = false;
  const int step = across_.step(tick_seconds, input, script_.left, script_.right, fresh);
  if (step == 0) {
    return false;
  }
  StockItem &item = quest_.garage.stock.at(line->stock);
  item.look = looks_.step(item.kind, item.id, item.look, step);
  tried_on_ = line->stock;
  sound(script_.move_sound);
  return true;
}

// A click on an arrow turns the look as left or right does.
void Assemble::look_pointer(InputFrame &input) const {
  const ShopGoods *line = chosen();
  if (!input.clicked || line == nullptr || !offers_look(*line)) {
    return;
  }
  const std::array<Buttons, 2> sides{script_.left, script_.right};
  for (std::size_t side = 0; side < sides.size(); ++side) {
    if (shop_hit(input, (*script_.look_arrows)[side])) {
      input.pressed |= sides[side];
      input.clicked = false;
      return;
    }
  }
}

} // namespace phantomcrash
