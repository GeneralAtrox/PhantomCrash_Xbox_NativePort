#include "phantomcrash/slai_talk_data.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace phantomcrash {
namespace {

int integer(const json::JsonValue &value, std::string_view key, int fallback = 0) {
  return static_cast<int>(value.number_or(key, fallback));
}

std::vector<std::string> strings(const json::JsonValue &value, std::string_view key) {
  std::vector<std::string> out;
  if (const auto *items = value.find(key); items != nullptr && items->is_array()) {
    for (const auto &item : items->array()) {
      out.push_back(item.is_string() ? item.string() : std::string{});
    }
  }
  return out;
}

std::vector<std::uint32_t> colours(const json::JsonValue &value, std::string_view key) {
  std::vector<std::uint32_t> out;
  for (const auto &text : strings(value, key)) {
    out.push_back(parse_gs_colour(text));
  }
  return out;
}

std::vector<std::array<int, 2>> pairs(const json::JsonValue &value, std::string_view key) {
  std::vector<std::array<int, 2>> out;
  if (const auto *items = value.find(key); items != nullptr && items->is_array()) {
    for (const auto &item : items->array()) {
      const auto &pair = item.array();
      if (pair.size() >= 2) {
        out.push_back({static_cast<int>(pair[0].number()), static_cast<int>(pair[1].number())});
      }
    }
  }
  return out;
}

SlaiTalkEvent event_from(const json::JsonValue &value) {
  SlaiTalkEvent event;
  event.index = integer(value, "index");
  event.name = value.string_or("name", "");
  event.kind = value.string_or("kind", "");
  event.flags = strings(value, "flags");
  event.place = value.string_or("place", "");
  event.sub_place = value.string_or("sub_place", "");
  event.city = integer(value, "city", -1);
  event.threshold = integer(value, "threshold");
  event.condition = value.string_or("condition", "");
  event.condition_arg = integer(value, "condition_arg");
  event.required_flag = integer(value, "required_flag");
  event.blocking_flag = integer(value, "blocking_flag");
  event.completion_flag = integer(value, "completion_flag");
  return event;
}

constexpr std::string_view style_open = "{style:";

} // namespace

bool SlaiTalkEvent::has_flag(std::string_view flag) const {
  return std::find(flags.begin(), flags.end(), flag) != flags.end();
}

int SlaiTalkTable::opcode(std::string_view name) const {
  for (std::size_t i = 0; i < opcodes.size(); ++i) {
    if (opcodes[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

const SlaiTalkCharacter *SlaiTalkTable::character(int id) const {
  const auto found = characters.find(id);
  return found == characters.end() ? nullptr : &found->second;
}

const SlaiTalkEvent *SlaiTalkTable::event(std::string_view name) const {
  for (const auto &event : events) {
    if (event.name == name) {
      return &event;
    }
  }
  return nullptr;
}

std::uint32_t parse_gs_colour(std::string_view text) {
  std::uint32_t value{};
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
  if (error != std::errc{} || end != text.data() + text.size()) {
    throw std::runtime_error("Bad GS colour: " + std::string(text));
  }
  return value;
}

SlaiTalkTable slai_talk_table_from_json(const json::JsonValue &value) {
  SlaiTalkTable table;
  if (const auto *opcodes = value.find("opcodes"); opcodes != nullptr) {
    for (const auto &row : opcodes->array()) {
      SlaiTalkOpcode op;
      op.name = row.string_or("name", "");
      op.argc = integer(row, "argc");
      op.polls = row.boolean_or("polls", false);
      op.string_arg = integer(row, "string_arg", -1);
      table.opcodes.push_back(std::move(op));
    }
    // waits_for names ops, so it resolves once every name is known.
    for (std::size_t i = 0; i < table.opcodes.size(); ++i) {
      for (const auto &name : strings(opcodes->array()[i], "waits_for")) {
        if (const int bit = table.opcode(name); bit >= 0 && bit < 64) {
          table.opcodes[i].waits |= std::uint64_t{1} << bit;
        }
      }
    }
  }
  if (const auto *characters = value.find("characters"); characters != nullptr) {
    for (const auto &[key, row] : characters->object()) {
      SlaiTalkCharacter character;
      character.folder = row.string_or("folder", "");
      character.icon = row.string_or("icon", "");
      character.name = row.string_or("name", "");
      character.chip_icon = integer(row, "chip_icon");
      character.expressions = integer(row, "expressions");
      character.pictures = integer(row, "pictures");
      table.characters[std::stoi(key)] = std::move(character);
    }
  }
  if (const auto *events = value.find("events"); events != nullptr) {
    for (const auto &row : events->array()) {
      table.events.push_back(event_from(row));
    }
  }
  table.news_pictures = strings(value, "news_pictures");
  table.style_colours = colours(value, "style_colours");
  table.window_colours = colours(value, "window_colours");
  table.prompt_colours = colours(value, "prompt_colours");
  if (const auto *items = value.find("style_text_colours"); items != nullptr) {
    for (const auto &pair : items->array()) {
      const auto &both = pair.array();
      table.style_text_colours.push_back(
          {parse_gs_colour(both.at(0).string()), parse_gs_colour(both.at(1).string())});
    }
  }
  table.panel_sprites = pairs(value, "panel_sprites");
  table.login_progress = pairs(value, "login_progress");
  if (const auto *login = value.find("login_text"); login != nullptr) {
    table.login_message = login->string_or("message", "");
    table.login_label = login->string_or("label", "");
  }
  table.knocker_textures = strings(value, "knocker_textures");
  if (const auto *sets = value.find("voice_meter"); sets != nullptr) {
    for (const auto &set : sets->array()) {
      auto &bars = table.voice_meter.emplace_back();
      for (const auto &bar : set.array()) {
        bars.push_back({static_cast<float>(bar.number_or("low", 0.0)),
                        static_cast<float>(bar.number_or("high", 0.0)),
                        static_cast<float>(bar.number_or("phase", 0.0)),
                        std::max(1, integer(bar, "period", 1))});
      }
    }
  }
  table.tutorial_record = integer(value, "tutorial_record");
  return table;
}

SlaiTalkScript slai_talk_script_from_json(const json::JsonValue &value,
                                          const SlaiTalkTable &table) {
  SlaiTalkScript script;
  for (const auto &line : value.find("code")->array()) {
    const auto &items = line.array();
    const int op = items.empty() ? -1 : table.opcode(items[0].string());
    if (op < 0) {
      throw std::runtime_error("Unknown talk opcode in script");
    }
    SlaiTalkInstruction instruction{op, {}};
    for (std::size_t i = 1; i < items.size(); ++i) {
      instruction.args.push_back(static_cast<int>(items[i].number()));
    }
    if (static_cast<int>(instruction.args.size()) != table.opcodes[op].argc) {
      throw std::runtime_error(table.opcodes[op].name + " has the wrong argument count");
    }
    script.code.push_back(std::move(instruction));
  }
  for (const auto &text : strings(value, "strings")) {
    script.strings.push_back(slai_talk_glyphs(text));
  }
  return script;
}

std::vector<std::uint16_t> slai_talk_glyphs(std::string_view text) {
  std::vector<std::uint16_t> out;
  for (std::size_t at = 0; at < text.size();) {
    const char c = text[at];
    if (c == '{') {
      const auto close = text.find('}', at);
      if (close == std::string_view::npos) {
        throw std::runtime_error("Unclosed { in talk text");
      }
      const auto token = text.substr(at + 1, close - at - 1);
      if (token == "player") {
        out.push_back(slai_talk_glyph::player);
      } else if (token == "partner") {
        out.push_back(slai_talk_glyph::partner);
      } else if (token == "alias") {
        out.push_back(slai_talk_glyph::alias);
      } else if (token == "/style") {
        out.push_back(slai_talk_glyph::style_end);
      } else if (text.substr(at).starts_with(style_open)) {
        out.push_back(static_cast<std::uint16_t>(slai_talk_glyph::style_first +
                                                 std::stoi(std::string(token.substr(6)))));
      } else {
        std::uint16_t unit{};
        const auto [end, error] =
            std::from_chars(token.data(), token.data() + token.size(), unit, 16);
        if (error != std::errc{} || end != token.data() + token.size()) {
          throw std::runtime_error("Bad talk text token {" + std::string(token) + "}");
        }
        out.push_back(unit);
      }
      at = close + 1;
      continue;
    }
    if (c == '\n') {
      out.push_back(slai_talk_glyph::newline);
    } else {
      const int unit = static_cast<unsigned char>(c) - 0x20;
      if (unit < 0 || unit >= slai_talk_glyph::ascii_limit) {
        throw std::runtime_error("Talk text character has no glyph");
      }
      out.push_back(static_cast<std::uint16_t>(unit));
    }
    ++at;
  }
  return out;
}

int SlaiTalkFont::advance(std::uint16_t glyph) const {
  return glyph < width.size() ? width[glyph] : cell_width;
}

SlaiTalkFont slai_talk_font_from_json(const json::JsonValue &value) {
  SlaiTalkFont font;
  font.pages = strings(value, "pages");
  font.per_page = integer(value, "glyphs_per_page", font.per_page);
  font.columns = integer(value, "columns", font.columns);
  if (const auto *cell = value.find("cell"); cell != nullptr && cell->array().size() >= 2) {
    font.cell_width = static_cast<int>(cell->array()[0].number());
    font.cell_height = static_cast<int>(cell->array()[1].number());
  }
  for (const auto *key : {"left", "width"}) {
    auto &out = std::string_view(key) == "left" ? font.left : font.width;
    if (const auto *items = value.find(key); items != nullptr) {
      for (const auto &item : items->array()) {
        out.push_back(static_cast<int>(item.number()));
      }
    }
  }
  return font;
}

} // namespace phantomcrash
