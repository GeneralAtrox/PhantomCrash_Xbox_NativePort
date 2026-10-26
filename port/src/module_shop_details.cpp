// The details' second page (a port addition): a weapon's stats the panel's
// art has no line for (part_stats.json "details").

#include "phantomcrash/module_shop.hpp"
#include "phantomcrash/shop_pointer.hpp"

namespace phantomcrash {

bool ModuleShop::has_details() const {
  const ShopGoods *line = chosen();
  return line != nullptr && script_.extra_stats &&
         !detail_stats(table_, parts_, garage_, line->goods).empty();
}

// Left and right turn the page, while the goods have one; the page
// buttons do too, and only they while left and right turn a look.
bool ModuleShop::turn_details(const InputFrame &input) {
  const ShopGoods *line = chosen();
  Buttons page = script_.page_back | script_.page_next;
  if (line == nullptr || !offers_look(*line)) {
    page |= script_.left | script_.right;
  }
  if (!input.any_pressed(page) || !has_details()) {
    return false;
  }
  details_page_ = !details_page_;
  sound(script_.move_sound);
  return true;
}

// A click on the details windows or the page arrows turns the page as
// right does.
void ModuleShop::details_pointer(InputFrame &input) const {
  if (!input.clicked || !script_.extra_stats || !has_details()) {
    return;
  }
  bool hit = shop_clicked(input, script_.extra_stats->page_arrows);
  for (const PopWindow &window : script_.list.details) {
    hit = hit || shop_hit(input, window.sprite);
  }
  const ShopGoods *line = chosen();
  if (hit) {
    input.pressed |= line != nullptr && offers_look(*line) ? script_.page_next : script_.right;
  }
}

} // namespace phantomcrash
