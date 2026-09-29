#pragma once

#include "phantomcrash/chip_shop_script.hpp"
#include "phantomcrash/garage_tables.hpp"
#include "phantomcrash/input.hpp"
#include "phantomcrash/menu_cursor.hpp"
#include "phantomcrash/robot_parts.hpp"
#include "phantomcrash/settings.hpp"
#include "phantomcrash/town_logic.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace phantomcrash {

// A CHIP on the shop's cards: its record and level, the name shown (the
// record's on sale, the one it was given in the stock), its price, and for
// one of the player's its stock entry and whether it is fitted.
struct ChipOffer {
  int chip{};
  int level{};
  std::string name;
  std::int64_t price{};
  int stock{-1};
  bool equipped{};
};

// The day's CHIPs on sale: its lineup (tables/garage.json chip_lineups,
// 0x01efdf), those whose record exists.
[[nodiscard]] std::vector<ChipOffer> chips_for_sale(const PartTables &parts,
                                                    const GarageTables &garage, int lineup);
// The player's CHIPs in stock, by stock index (0x0182b1).
[[nodiscard]] std::vector<ChipOffer>
chips_in_stock(const PartTables &parts, const GarageTables &garage, const Garage &owned);

// What the dialog shows of a CHIP's fire control: its reticles' shapes and
// half sizes in 640x480 units, its Mortal Shot and Repulsion Shield in
// percent and its weight in hundredths of a tonne.
struct ChipSpec {
  int inner_shape{};
  int outer_shape{};
  std::array<float, 2> inner_half{};
  std::array<float, 2> outer_half{};
  int mortal_shot{};
  int repulsion_shield{};
  int weight{};
};

[[nodiscard]] ChipSpec chip_spec(const ChipSpecPanel &panel, const ChipRecord &chip, int level);

enum class ChipShopView { browse, dialog, notice, question };

// Wild Machines' BUY CHIP and SELL CHIP (scene 15, states 2 and 5): the
// window grows open and the cards fade in, the middle first; left/right
// slide the cards, up/down move between them and EXIT. Confirm on a CHIP
// opens its dialog: the trade (BUY or SELL) or CANCEL. A CHIP bought goes
// into the stock and NAME ENTRY names it; one sold leaves the stock for
// part of its price, unless it is fitted. PlusTech's UPLIFT (scene 27,
// state 2) shows the player's CHIPs the same way; its dialog chooses a
// higher level, paid for by the difference in price.
class ChipShop final : public TownLogic {
public:
  ChipShop(ChipShopScript script, std::vector<ChipOffer> offers, QuestSave quest,
           const PartTables &parts, int resume_item = 0);

  void update(double tick_seconds, const InputFrame &input);

  [[nodiscard]] const ChipShopScript &script() const noexcept {
    return script_;
  }
  [[nodiscard]] const QuestSave &quest() const noexcept {
    return quest_;
  }
  [[nodiscard]] const std::vector<ChipOffer> &offers() const noexcept {
    return offers_;
  }
  [[nodiscard]] const ChipRecord *record(const ChipOffer &offer) const;
  [[nodiscard]] ChipShopView view() const noexcept {
    return view_;
  }

  [[nodiscard]] int cursor() const noexcept {
    return cursor_;
  }
  // The card the cards slide from, towards the cursor's.
  [[nodiscard]] int previous() const noexcept {
    return previous_;
  }
  [[nodiscard]] bool on_exit() const noexcept {
    return on_exit_;
  }
  // How far the cards have slid, 0 to 1.
  [[nodiscard]] float slide() const;
  // The cards stand still and take input.
  [[nodiscard]] bool settled() const;
  // After a dialog is left the middle card's highlight pulses.
  [[nodiscard]] bool highlight_pulses() const noexcept {
    return highlight_pulse_;
  }

  // The opening: the window's growth (0 to 1), then each ring of cards
  // fading in (`ring` 0 is the middle card).
  [[nodiscard]] float window_open() const;
  [[nodiscard]] float card_alpha(int ring) const;
  [[nodiscard]] float scan_offset() const;

  // The dialog's stages, each 0 to 1 (see ChipDialog), and its option.
  [[nodiscard]] float dialog_stage(int stage) const;
  [[nodiscard]] bool dialog_open() const noexcept {
    return in_dialog_;
  }
  [[nodiscard]] int choice() const noexcept {
    return choice_;
  }
  [[nodiscard]] double view_seconds() const noexcept {
    return view_seconds_;
  }

  // The notice or question shown; empty for none.
  [[nodiscard]] const std::string &message() const noexcept {
    return message_;
  }
  // The stock entry of the CHIP bought, for NAME ENTRY; -1 until then.
  [[nodiscard]] int naming_stock() const noexcept {
    return naming_stock_;
  }
  // Uplifting: the level chosen and what raising the CHIP to it costs.
  [[nodiscard]] int level() const noexcept {
    return level_;
  }
  [[nodiscard]] std::int64_t uplift_price() const;
  [[nodiscard]] std::int64_t money() const noexcept {
    return quest_.money;
  }
  [[nodiscard]] const std::string &help_text() const noexcept {
    return help_;
  }
  [[nodiscard]] double help_seconds() const noexcept {
    return help_seconds_;
  }

private:
  enum class After { browse, dialog, name_chip, name_entry, leave, sold, see_level, uplifted };

  void browse_input(double tick_seconds, const InputFrame &input);
  void dialog_input(double tick_seconds, const InputFrame &input);
  void message_input(const InputFrame &input);
  void open_dialog();
  void answer(bool yes);
  void trade();
  // UPLIFT (chip_uplift.cpp).
  void open_uplift();
  void level_input(double tick_seconds, const InputFrame &input);
  void uplift_chosen();
  void sell_chosen();
  void show(ChipShopView view, const std::string &message, After after);
  void close_dialog(bool pulse);
  void menu_help();
  void set_help(const std::string &text);

  ChipShopScript script_;
  std::vector<ChipOffer> offers_;
  QuestSave quest_;
  const PartTables &parts_;
  CursorRepeat side_;
  ChipShopView view_{ChipShopView::browse};
  bool in_dialog_{}; // the dialog shows, under any notice
  int cursor_{};
  int previous_{};
  double slide_seconds_{};
  bool on_exit_{};
  bool highlight_pulse_{};
  double view_seconds_{};
  int choice_{};
  std::string message_;
  After after_{After::browse};
  int naming_stock_{-1};
  int level_{}; // uplifting: the level chosen
  std::string help_;
  double help_seconds_{};
};

} // namespace phantomcrash
