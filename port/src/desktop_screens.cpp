#include "desktop_screens.hpp"

#include "desktop_front_end.hpp"
#include "desktop_intro.hpp"
#include "desktop_option.hpp"
#include "desktop_option_screens.hpp"

namespace phantomcrash {

namespace {

// A converted table, or null (logged) when the conversion lacks it.
std::optional<json::JsonValue> table(const DataRoots &roots, const char *relative) {
  if (const auto path = roots.find_asset(relative)) {
    return json::read_file(*path);
  }
  SDL_Log("Table %s is missing; convert the game files again", relative);
  return std::nullopt;
}

} // namespace

ScreenRunner::ScreenRunner(SDL_Renderer *renderer, const DataRoots &roots, const std::string &first,
                           const GameOptions options)
    : roots_(roots), textures_(renderer, roots),
      audio_(roots, roots.document("audio/music.json"), roots.document("audio/sfx.json")),
      intro_(parse_intro(roots.document("boot/intro.json"))),
      title_(parse_title(roots.document("ui/title.json"))),
      menu_(parse_main_menu(roots.document("ui/main_menu.json"))),
      option_(parse_option(roots.document("ui/option.json"))),
      sound_config_(parse_sound_config(roots.document("ui/sound_config.json"))),
      button_config_(parse_button_config(roots.document("ui/button_config.json"))),
      screen_adjust_(parse_screen_adjust(roots.document("ui/screen_adjust.json"))),
      message_box_(parse_message_box(roots.document("ui/message_box.json"))), options_(options) {
  if (const auto font = table(roots, "tables/font.json")) {
    font_ = parse_glyph_font(*font);
  }
  if (const auto music = table(roots, "tables/music.json")) {
    credits_ = parse_music_credits(*music);
    if (const auto *table = music->find("volume_millibels")) {
      for (const auto &level : table->array()) {
        volume_millibels_.push_back(static_cast<int>(level.number()));
      }
    }
  }
  const auto music_names = roots.document("audio/music.json");
  if (const auto *tracks = music_names.find("tracks")) {
    for (const auto &[name, track] : tracks->object()) {
      track_files_[name] = track.string_or("file", "");
    }
  }
  const auto placeholders = roots.document("ui/placeholders.json");
  placeholder_buttons_ = buttons_from_json(*placeholders.find("buttons"));
  placeholder_return_ = placeholders.string_or("return_to", "main_menu");
  if (const auto *screens = placeholders.find("screens")) {
    for (const auto &[name, value] : screens->object()) {
      placeholders_[name] = {value.string_or("label", name), value.string_or("music", ""),
                             value.string_or("return_to", placeholder_return_)};
    }
  }
  apply_volumes();
  show(first);
}

std::string ScreenRunner::now_playing(const std::string &track, const std::string &format) const {
  const auto file = track_files_.find(track);
  return file == track_files_.end() ? std::string{} : credit_line(credits_, file->second, format);
}

void ScreenRunner::apply_volumes() {
  audio_.set_volumes(volume_gain(options_.bgm_volume, volume_millibels_),
                     volume_gain(options_.se_volume, volume_millibels_));
}

std::unique_ptr<Screen> ScreenRunner::make(const std::string &name, const std::string &from) {
  if (name == "intro") {
    // The licence screen and Dolby play on the first intro only.
    const bool first_boot = intro_pass_ == 0;
    return std::make_unique<IntroScreen>(textures_, roots_, intro_, first_boot, intro_pass_++);
  }
  if (name == "title") {
    return std::make_unique<FrontEndScreen>(textures_, title_, menu_);
  }
  if (name == "main_menu") {
    return std::make_unique<FrontEndScreen>(textures_, title_, menu_, from.empty() ? "?" : from);
  }
  if (name == "option") {
    return std::make_unique<OptionScreen>(textures_, font_, option_, message_box_, options_,
                                          now_playing(option_.music, option_.now_playing_format),
                                          from);
  }
  if (name == "sound_config") {
    return std::make_unique<SoundConfigScreen>(
        textures_, font_, sound_config_, options_,
        now_playing(sound_config_.music, sound_config_.now_playing_format));
  }
  if (name == "button_config") {
    return std::make_unique<ButtonConfigScreen>(
        textures_, font_, button_config_, options_,
        now_playing(button_config_.music, button_config_.now_playing_format));
  }
  if (name == "screen_adjust") {
    return std::make_unique<ScreenAdjustScreen>(textures_, font_, screen_adjust_, message_box_,
                                                options_);
  }
  if (const auto found = placeholders_.find(name); found != placeholders_.end()) {
    return std::make_unique<PlaceholderScreen>(textures_.renderer(), found->second.label,
                                               found->second.music, placeholder_buttons_,
                                               found->second.return_to);
  }
  return nullptr;
}

void ScreenRunner::show(const std::string &name) {
  auto next = make(name, name_);
  if (next == nullptr) {
    SDL_Log("No screen named \"%s\"; showing the title", name.c_str());
    next = make("title", name_);
    name_ = "title";
  } else {
    name_ = name;
  }
  screen_ = std::move(next);
  audio_.play_music(screen_->music());
}

void ScreenRunner::tick(const InputFrame &input, const double tick_seconds) {
  screen_->tick(input, tick_seconds);
  if (const GameOptions *changed = screen_->game_options()) {
    options_ = *changed;
    apply_volumes();
  }
  for (const auto &sound : screen_->take_sounds()) {
    audio_.play_sound(sound);
  }
  audio_.play_music(screen_->music());
  if (screen_->done()) {
    show(screen_->next());
  }
}

void ScreenRunner::advance(const double elapsed_seconds) {
  screen_->advance(elapsed_seconds);
  audio_.update();
}

void ScreenRunner::draw(const Viewport &viewport) {
  // The whole picture moves by the screen offset, as the original moves
  // every render pass's viewport (0xb7cc0); the uncovered edge is black.
  Viewport moved = viewport;
  moved.offset_x += static_cast<float>(options_.screen_offset[0]) * viewport.scale;
  moved.offset_y += static_cast<float>(options_.screen_offset[1]) * viewport.scale;
  screen_->draw(moved);
}

void ScreenRunner::set_paused(const bool paused) {
  screen_->set_paused(paused);
  audio_.set_paused(paused);
}

} // namespace phantomcrash
