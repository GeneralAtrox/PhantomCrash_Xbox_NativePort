#include "desktop_part_stats.hpp"

#include "desktop_draw_list.hpp"

namespace phantomcrash {

void draw_part_stats(DrawList &list, const GlyphFont &font, const PartStatsStyle &style,
                     const std::vector<StatLine> &lines, const Goods &goods, const float top) {
  float y = top;
  for (const StatLine &line : lines) {
    const std::vector<int> run = glyph_run(font, stat_text(line));
    list.text(style.layer, layout_run(font, run, style.right - run_width(font, run), y),
              style.colour);
    // A CHIP's level has no word beside it.
    if (line.row == StatRow::level && goods.kind != GoodsKind::chip) {
      const std::size_t sign = goods.level < 0 ? 0 : (goods.level == 0 ? 1 : 2);
      Sprite word = style.signs[sign];
      word.position = {style.right + style.sign_offset, y};
      list.sprite(word);
    }
    y += style.step;
  }
}

} // namespace phantomcrash
