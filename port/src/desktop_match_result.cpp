#include "desktop_draw_list.hpp"
#include "desktop_match_result.hpp"
#include "phantomcrash/json_fields.hpp"
#include "phantomcrash/money.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace phantomcrash {
using json::required;
namespace {
std::string money_text(const std::int64_t value) {
  std::string result = std::to_string(shown_ny(value));
  for (int at = static_cast<int>(result.size()) - 3; at > 0; at -= 3) {
    result.insert(static_cast<std::size_t>(at), ",");
  }
  return result;
}
std::string play_time(const double seconds) {
  const int time = std::clamp(static_cast<int>(seconds), 0, 359999);
  std::ostringstream out;
  out << std::setfill('0') << std::setw(2) << time / 3600 << ':' << std::setw(2) << time / 60 % 60
      << ':' << std::setw(2) << time % 60;
  return out.str();
}
} // namespace

MatchResultScreen::MatchResultScreen(TextureCache &textures, const GlyphFont &font,
                                     const FrameStyle &frame,
                                     const std::vector<MusicCredit> &credits,
                                     const json::JsonValue &data, MatchReceipt receipt,
                                     QuestSave quest, WorldRenderer *world, WorldFrame background,
                                     std::string next, std::shared_ptr<ReplayArchive> replay)
    : textures_(textures), font_(font),
      frame_(font, frame, credits, parse_frame_placement(required(data, "frame"))),
      receipt_(std::move(receipt)), quest_(std::move(quest)), world_(world),
      background_(std::move(background)), next_(std::move(next)) {
  sprites_ = parse_sprites(required(data, "sprites"));
  prize_label_ = parse_sprite(required(data, "prize"));
  rental_label_ = parse_sprite(required(data, "rental_prize"));
  retire_ = parse_sprite(required(data, "retire"));
  exit_ = parse_sprite(required(data, "exit"));
  replay_ = parse_sprite(required(data, "replay"));
  replay_selected_ = parse_sprite(required(data, "replay_selected"));
  exit_selected_ = parse_sprite(required(data, "exit_selected"));
  replay_data_ = required(data, "replay_settings");
  tape_ = std::move(replay);
  hint_ = parse_ticker(required(data, "hint"));
  hint_run_ = glyph_run(font_, required(data, "hint").string_or("text", ""));
  values_right_ = static_cast<float>(data.number_or("values_right", 312));
  const auto &rows = required(data, "value_rows").array();
  for (std::size_t row = 0; row < rows_.size(); ++row) {
    rows_[row] = static_cast<float>(rows.at(row).number());
  }
  tally_seconds_ = std::max(0.0, data.number_or("tally_seconds", 3));
  const auto &music = required(data, "music");
  music_ =
      music.string_or(receipt_.match.outcome == MatchOutcome::retire ? "retire" : "survive", "");
  frame_.preload(textures_);
}

void MatchResultScreen::tick(const InputFrame &input, const double seconds) {
  if (watching_) {
    watching_->tick(input, seconds);
    auto cues = watching_->take_sound_cues();
    pending_sounds_.insert(pending_sounds_.end(), cues.begin(), cues.end());
    if (watching_->done()) {
      watching_.reset();
      SDL_Log("Replay stopped: returned to RESULT");
    }
    return;
  }
  seconds_ += seconds;
  frame_.update(seconds);
  if (input.any_pressed(button::left)) {
    selected_ = 0;
  }
  if (input.any_pressed(button::right | button::cancel)) {
    selected_ = 1;
  }
  bool clicked = false;
  if (input.pointer) {
    const auto [x, y] = *input.pointer;
    if (y >= 376 && y < 408 && x >= 108 && x < 284) {
      if (input.pointer_moved || input.clicked) {
        selected_ = x < 196 ? 0 : 1;
      }
      clicked = input.clicked;
    }
  }
  if (input.any_pressed(button::confirm | button::cancel) || clicked) {
    if (seconds_ < tally_seconds_) {
      seconds_ = tally_seconds_;
    } else {
      if (selected_ == 0 && tape_ && tape_->size() > 1) {
        watching_ = std::make_unique<ReplayScreen>(textures_, font_, world_, tape_, replay_data_);
      } else if (selected_ == 1) {
        done_ = true;
      }
    }
  }
}

std::vector<SoundCue> MatchResultScreen::take_sound_cues() {
  return std::exchange(pending_sounds_, {});
}

void MatchResultScreen::text(DrawList &list, const std::string &value, const float x, const float y,
                             const std::uint32_t colour, const float scale,
                             const bool right) const {
  const auto run = glyph_run(font_, value);
  auto quads = layout_run(font_, run, 0, 0);
  const float left = right ? x - run_width(font_, run) * scale : x;
  for (auto &quad : quads) {
    quad.x = left + quad.x * scale;
    quad.y = y + quad.y * scale;
    quad.width *= scale;
    quad.height *= scale;
  }
  list.text(264, std::move(quads), colour);
}

void MatchResultScreen::draw(const Viewport &viewport) {
  if (watching_) {
    watching_->draw(viewport);
    return;
  }
  clear_black(textures_);
  if (world_ && world_->available()) {
    background_.width = viewport.pixel_width;
    background_.height = viewport.pixel_height;
    background_.projection.at(0, 0) = background_.projection.at(1, 1) *
                                      static_cast<float>(viewport.pixel_height) /
                                      static_cast<float>(viewport.pixel_width);
    world_->draw(background_);
  }
  DrawList list(textures_, font_, viewport, seconds_);
  for (const Sprite &sprite : sprites_) {
    list.sprite(sprite);
  }
  list.sprite(prize_label_);
  if (receipt_.match.rental) {
    list.sprite(rental_label_);
  }
  const double share = tally_seconds_ > 0 ? std::min(seconds_ / tally_seconds_, 1.0) : 1.0;
  std::array<std::int64_t, 9> values{
      receipt_.starting_money, receipt_.prize,        receipt_.repairs[0],
      receipt_.repairs[1],     receipt_.repairs[2],   receipt_.repairs[3],
      receipt_.repairs[4],     receipt_.repair_total, receipt_.total_money};
  values[7] = 0;
  for (std::size_t row = 1; row <= 6; ++row) {
    values[row] = static_cast<std::int64_t>(values[row] * share);
    if (row >= 2) {
      values[7] += values[row];
    }
  }
  values[8] = std::clamp(values[0] + values[1] - values[7], std::int64_t{0}, money_limit);
  for (std::size_t row = 0; row < values.size(); ++row) {
    const bool repair = row >= 2 && row <= 7;
    const auto amount = values[row];
    const auto colour = amount && repair ? 0xff0000U : row == 1 ? 0x86ff00U : 0xffffffU;
    const std::string sign = amount && repair ? "- " : row == 1 ? "+ " : "";
    const auto value = sign + money_text(amount);
    const float scale = std::min(0.9F, 100.0F / run_width(font_, glyph_run(font_, value)));
    text(list, value, values_right_, rows_[row], colour, scale, true);
  }
  if (receipt_.match.outcome == MatchOutcome::retire) {
    list.sprite(retire_);
  } else {
    text(list, receipt_.match.outcome == MatchOutcome::win ? "Win" : "Leave", 389, 253, 0xffd300,
         2.3F);
  }
  std::ostringstream kills;
  kills << std::setfill('0') << std::setw(4) << receipt_.match.shootdowns;
  text(list, kills.str(), 590, 288, 0xffffff, 2.0F, true);
  text(list, play_time(receipt_.match.play_seconds), 590, 338, 0xffffff, 0.95F, true);
  list.sprite(selected_ == 0 ? replay_selected_ : replay_,
              tape_ && tape_->size() > 1 ? 1.0F : 0.45F);
  list.sprite(selected_ == 1 ? exit_selected_ : exit_);
  list.ticker(hint_, hint_run_, seconds_);
  quest_.money = values[8];
  frame_.draw(list, quest_);
  list.flush();
}

} // namespace phantomcrash
