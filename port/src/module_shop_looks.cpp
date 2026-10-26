// Per-part appearance while buying (a port addition, part_looks.hpp): left
// and right turn the chosen part's look on the preview, and the part bought
// keeps the look last shown for it.

#include "phantomcrash/module_shop.hpp"
#include "phantomcrash/shop_pointer.hpp"

namespace phantomcrash {

bool ModuleShop::offers_look(const ShopGoods &line) const {
  const auto kind = stock_kind(line.goods.kind);
  return script_.mode == ModuleShopMode::buy && script_.look_arrows && kind &&
         looks_.offers(*kind, line.goods.id);
}

int ModuleShop::chosen_look(const ShopGoods &line) const {
  const auto found = looks_chosen_.find({line.goods.kind, line.goods.id});
  return found != looks_chosen_.end() && offers_look(line) ? found->second : -1;
}

int ModuleShop::shown_part(const ShopGoods &line) const {
  const int look = chosen_look(line);
  return look >= 0 ? look : line.goods.id;
}

bool ModuleShop::turn_look(const double tick_seconds, const InputFrame &input) {
  const ShopGoods *line = chosen();
  if (line == nullptr || !offers_look(*line)) {
    return false;
  }
  bool fresh = false;
  const int step = across_.step(tick_seconds, input, script_.left, script_.right, fresh);
  if (step == 0) {
    return false;
  }
  const PartKind kind = *stock_kind(line->goods.kind);
  looks_chosen_[{line->goods.kind, line->goods.id}] =
      looks_.step(kind, line->goods.id, chosen_look(*line), step);
  sound(script_.move_sound);
  return true;
}

// A click on an arrow turns the look as left or right does.
void ModuleShop::look_pointer(InputFrame &input) const {
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
