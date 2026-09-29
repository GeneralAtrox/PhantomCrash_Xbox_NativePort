#include "desktop_training.hpp"

#include "desktop_draw_list.hpp"
#include "desktop_event_text.hpp"
#include "desktop_text.hpp"
#include "desktop_training_robots.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <utility>

namespace phantomcrash {
namespace {

// Every slot's weapon (FreePlay, and the lessons' own weapon masks).
constexpr unsigned all_weapons = (1U << weapon_slots) - 1;
// The lesson mode of Pepper's Optic Camouflage lesson (save +0x10f10).
constexpr int optic_mode = 2;

std::vector<LessonStep> course_for(const TrainingScript &script, const int mode) {
  const auto found = script.battle.courses.find(mode);
  return found == script.battle.courses.end() ? std::vector<LessonStep>{} : found->second;
}

} // namespace

TrainingScreen::TrainingScreen(TextureCache &textures, const GlyphFont &font,
                               const TrainingContext &context, const TrainingLesson &lesson)
    : textures_(textures), font_(font), world_(context.world), script_(context.script),
      talk_(context.talk), tuning_(context.tuning), mode_(lesson.mode),
      free_play_(!context.script.battle.courses.contains(lesson.mode)),
      tutorial_(course_for(context.script, lesson.mode)), camera_(context.script.battle.camera),
      views_(context.script.battle.views,
             load_fixed_cameras(context.roots, context.script.battle.scenes)),
      player_name_run_(glyph_run(font, context.player_name)) {
  screens_ = std::clamp(lesson.screens, 1, max_screens);
  day_ = context.day;
  money_ = context.money;
  if (context.stage != nullptr) {
    lighting_ = context.stage->lighting;
    stage_.sky_nodes = context.stage->sky_nodes;
    if (world_ != nullptr && !context.stage->model.empty()) {
      stage_.model = world_->model(context.stage->model);
    }
  }
  if (world_ == nullptr || !world_->available()) {
    missing_run_ = glyph_run(font, "The 3D world needs the Direct3D 11 renderer");
  } else if (stage_.model < 0) {
    missing_run_ = glyph_run(font, "The stage model is missing: convert the game files");
  }
  build_battle(context);
  load_sounds(context.roots);
  cockpit_ = load_cockpit(world_, context.roots);
  effects_ = DesktopEffects::load(world_, context.roots);
  if (effects_) {
    shots_ = std::make_unique<DesktopShots>(*world_, effects_->system().bank());
  }
  door_model_ = load_stage_doors(world_, context.roots, script_.battle.scenes, *battle_);
  if (free_play_) {
    supply_ =
        load_supply(world_, context.roots, script_.battle.scenes, script_.battle.supply, *battle_);
  }
  if (!free_play_) {
    load_messages(context.roots, context.standard_controls);
  } else if (!battle_->robots().empty()) {
    // FreePlay walks out of a gate (0x082552, robot state 1-4).
    scene_ = load_battle_scene(textures_, font_, context.roots, script_.battle.scenes,
                               static_cast<std::uint32_t>(SDL_GetTicks()));
    if (scene_) {
      static_cast<void>(scene_->scene().start_entrance(*battle_));
      for (int index = 1;
           live_world_ == nullptr && index < static_cast<int>(battle_->robots().size()); ++index) {
        static_cast<void>(scene_->scene().start_robot_entrance(*battle_, index));
      }
    }
  }
}

void TrainingScreen::build_battle(const TrainingContext &context) {
  const TrainingBattle &setup = script_.battle;
  EnemyPath path;
  if (const auto file = context.roots.find_asset(setup.enemy_path)) {
    try {
      path = parse_enemy_path(json::read_file(*file));
    } catch (const std::exception &error) {
      SDL_Log("%s: %s", setup.enemy_path.c_str(), error.what());
    }
  } else {
    SDL_Log("Enemy path %s is missing; convert the robots again", setup.enemy_path.c_str());
  }
  const bool drones = !path.entries().empty();
  const std::optional<PartTables> parts = load_part_tables(context.roots);
  BattleSettings settings = setup.settings;
  if (parts) {
    for (const std::string &name : apply_weapon_records(settings.weapons, *parts)) {
      SDL_Log("Weapon %s: its part is not in %s", name.c_str(), part_tables_asset);
    }
  }
  const float player_hp = robot_hp(script_, parts, "player", setup.player, true);
  live_world_ = context.live_battle ? context.live_world : nullptr;
  live_area_ = context.live_area;
  if (live_world_ != nullptr) {
    battle_ = context.live_battle;
    seat_player(settings, player_hp);
  } else {
    battle_ = std::make_shared<Battle>(std::move(settings), std::move(path));
    if (context.stage != nullptr && !context.stage->collision.empty()) {
      load_stage_collision(context.roots, context.stage->collision, *battle_);
    }
    battle_->add_robot("player", 0, setup.player_start, setup.player_heading, setup.player.weapons,
                       player_hp);
  }
  battle_->set_tag(0, pilot_tag(parts, setup.player.pilot));
  battle_->set_combat(0, robot_combat(script_, parts, "player", setup.player));
  const float drone_hp = robot_hp(script_, parts, "drone", setup.drone, false);
  const CombatStats drone_combat = robot_combat(script_, parts, "drone", setup.drone);
  for (int index = 0; drones && live_world_ == nullptr && index < setup.drones; ++index) {
    const int drone = battle_->add_drone("drone" + std::to_string(index), drone_hp);
    battle_->set_tag(drone, pilot_tag(parts, setup.drone.pilot));
    battle_->set_combat(drone, drone_combat);
    battle_->set_boss(drone, setup.drone.boss);
  }
  hud_ = make_battle_hud(textures_, font_, context.roots, screens_, 0);
  build_split(context.roots);
  chatter_ = load_chatter_window(textures_, font_, context.roots, talk_, tuning_, setup);
  if (const ChipRecord *chip = parts ? parts->chip(setup.player.chip) : nullptr) {
    const ChipStats stats = chip_stats(*chip, setup.player.chip_level);
    battle_->set_lock_seconds(0, stats.lock_frames / part_frames_per_second);
    hud_->set_fire_control(stats);
    if (chatter_) {
      chatter_->set_speaker(0, chip_speaker(*chip, context.player_name));
    }
  }
  if (world_ == nullptr || !world_->available() || context.robots == nullptr ||
      !context.robots->available()) {
    return;
  }
  view_ = std::make_unique<BattleView>(*world_, *context.robots);
  view_->set_optical(setup.optical);
  view_->set_shield(setup.shield_look);
  view_->set_shadows(setup.shadows, !splits_.empty());
  try {
    const auto player = script_.robots.find("player");
    const auto drone = script_.robots.find("drone");
    view_->add(player == script_.robots.end() ? RobotParts{} : player->second);
    if (live_world_ != nullptr) {
      add_live_views();
    }
    for (std::size_t index = view_->size(); index < battle_->robots().size(); ++index) {
      view_->add(drone == script_.robots.end() ? RobotParts{} : drone->second);
    }
  } catch (const std::exception &error) {
    SDL_Log("Training robots: %s", error.what());
    view_.reset();
    return;
  }
  // Each robot strides at its leg motions' pace, slowed by its load; the
  // live RUMBLING's opponents came in with their own.
  for (std::size_t index = 0; index < view_->size(); ++index) {
    battle_->set_radius(static_cast<int>(index), view_->radius(index, setup.settings.robot_radius));
    if (index > 0 && live_world_ != nullptr) {
      continue;
    }
    MovementTuning movement = setup.stride_speeds ? view_->movement(index, setup.settings.movement)
                                                  : setup.settings.movement;
    if (parts) {
      const RobotLoadout loadout = index == 0 ? training_loadout(script_, "player", setup.player)
                                              : training_loadout(script_, "drone", setup.drone);
      movement = loaded_movement(*parts, loadout, load_stats(*parts, loadout), movement);
    }
    battle_->set_movement(static_cast<int>(index), movement);
  }
}

void TrainingScreen::load_messages(const DataRoots &roots, const bool standard_controls) {
  const std::string &file = script_.battle.lessons;
  const auto path = roots.find_asset(file);
  if (!path) {
    SDL_Log("Lesson messages %s are missing; convert the robots again", file.c_str());
    return;
  }
  try {
    messages_ = parse_lesson_messages(json::read_file(*path), font_, standard_controls);
  } catch (const std::exception &error) {
    SDL_Log("%s: %s", file.c_str(), error.what());
  }
  for (const auto &[state, scripts] : messages_) {
    for (const auto &commands : scripts) {
      for (const EventCommand &command : commands) {
        if (command.op == EventOp::sprite && !command.sprite.image.empty()) {
          static_cast<void>(textures_.get(command.sprite.image));
        }
      }
    }
  }
}

// Starts the message the lesson asks for (skipping states without one) and
// plays it; its end moves the lesson on.
void TrainingScreen::update_message(const InputFrame &input, const double tick_seconds) {
  while (!message_) {
    const auto wanted = tutorial_.message();
    if (!wanted) {
      break;
    }
    const auto found = messages_.find(wanted->first);
    const auto index = static_cast<std::size_t>(wanted->second);
    if (found == messages_.end() || found->second[index].empty()) {
      tutorial_.message_done();
      continue;
    }
    message_event_ = std::make_unique<StoryEvent>();
    message_event_->scene = wanted->first;
    message_event_->commands = found->second[index];
    message_ = std::make_unique<EventPlayer>(*message_event_, talk_, tuning_,
                                             static_cast<std::uint32_t>(wanted->first));
  }
  if (!message_) {
    return;
  }
  message_->update(tick_seconds, input);
  for (std::string &sound : message_->take_sounds()) {
    sounds_.push_back(std::move(sound));
  }
  if (message_->finished()) {
    message_.reset();
    message_event_.reset();
    tutorial_.message_done();
    jump_armed_ = false;
  }
}

void TrainingScreen::tick_battle(const InputFrame &input, const float seconds) {
  const TrainingBattle &setup = script_.battle;
  const LessonStep *step = tutorial_.step();
  const bool practising = free_play_ || tutorial_.practising();
  // A scene walks the robot, or a boss's entrance holds it (0x082e60).
  const bool scripted = in_scene();
  const bool locked = message_ != nullptr || !practising || scripted;
  RobotControl control;
  FireControl fire;
  if (!locked) {
    control = battle_control(input, setup.buttons, free_play_ ? nullptr : step);
    fire = battle_fire(input, setup.buttons);
    fire.camouflage = fire.camouflage && (free_play_ || mode_ == optic_mode);
  }
  if (!jump_armed_) {
    jump_armed_ = (input.held & setup.buttons.jump) == 0;
    control.jump = false;
  }
  battle_->set_drones_active(free_play_ || (practising && step != nullptr && step->drones));
  battle_->set_allowed_weapons(free_play_                      ? all_weapons
                               : practising && step != nullptr ? step->weapons
                                                               : 0U);
  if (tutorial_.take_place_drone() && step != nullptr && battle_->robots().size() > 1) {
    battle_->place_facing_player(1, step->place_drone);
  }
  const float heading_before = battle_->player().mover.heading();
  if (scene_) {
    scene_->scene().prepare(*battle_, seconds);
  }
  battle_->tick(control, fire, seconds);
  for (const BattleEvent &event : battle_->events()) {
    if (event.kind == BattleEventKind::damaged && event.robot == 0) {
      rumble_.push_back({0, event.amount});
    }
  }
  if (scene_) {
    scene_->scene().observe(*battle_);
    std::ranges::move(scene_->scene().take_sounds(), std::back_inserter(sounds_));
    if (auto music = scene_->scene().take_music()) {
      cue_music_ = std::move(*music);
    }
  }
  gather_sounds(seconds);
  hud_->tick(*battle_, seconds);
  for (SplitView &split : splits_) {
    split.hud->tick(*battle_, seconds);
  }
  tutorial_.update(lesson_sample(battle_->player(), heading_before, seconds));
}

void TrainingScreen::tick(const InputFrame &input, const double tick_seconds) {
  if (input.any_pressed(script_.battle.buttons.exit)) {
    done_ = true;
    return;
  }
  const auto seconds = static_cast<float>(tick_seconds);
  clock_ += tick_seconds;
  if (!free_play_) {
    update_message(input, tick_seconds);
  }
  tick_battle(input, seconds);
  follow_live();
  if (chatter_) {
    chatter_->set_scene(scene_ && scene_->scene().active()); // the player's own scene
    // Pepper's lesson messages have the chatter's window to themselves.
    chatter_->tick(*battle_, tick_seconds, message_ != nullptr);
    std::ranges::move(chatter_->take_sounds(), std::back_inserter(sounds_));
  }
  if (view_) {
    view_->tick(*battle_, seconds);
  }
  if (supply_) {
    supply_->tick(*battle_, effects_ ? &effects_->system() : nullptr);
  }
  if (shots_) {
    shots_->tick(*battle_, effects_->system());
  }
  if (effects_) {
    effects_->tick(seconds);
  }
  tick_camera(input, seconds);
  tick_split(seconds);
  done_ = done_ || (scene_ && scene_->scene().finished()) ||
          (!free_play_ && tutorial_.finished() && !message_);
}

BattleHudInfo TrainingScreen::hud_info() const {
  BattleHudInfo info;
  const LessonStep *step = tutorial_.step();
  info.panels.day = day_;
  info.panels.money = money_;
  info.panels.mission_seconds = clock_;
  info.panels.gauges = !cockpit_up();
  info.sights = views_.view() <= camera_view::behind; // 0x04eaa3
  if (!free_play_ && tutorial_.practising() && step != nullptr && step->goal != LessonGoal::none) {
    info.progress = tutorial_.progress();
    if (step->goal == LessonGoal::kills) {
      info.counter = std::to_string(tutorial_.kills()) + " / " +
                     std::to_string(static_cast<int>(step->amount));
    }
  }
  return info;
}

void TrainingScreen::draw_message(const Viewport &viewport) {
  if (!message_) {
    return;
  }
  DrawList list(textures_, font_, viewport, message_->seconds());
  const TalkFrame &frame = message_->frame();
  for (const auto &sprite : frame.sprites) {
    list.sprite(sprite.sprite, sprite.alpha);
  }
  for (const auto &text : frame.texts) {
    draw_event_text(list, font_, text, message_->seconds(), player_name_run_);
  }
  list.flush();
}

void TrainingScreen::draw_notice(const std::vector<int> &run, const Viewport &viewport) {
  const float width = run_width(font_, run);
  std::vector<GlyphQuad> glyphs = layout_run(font_, run, 320.0F - width * 0.5F, 230.0F);
  draw_glyphs(textures_, font_, glyphs, viewport);
}

void TrainingScreen::draw(const Viewport &viewport) {
  SDL_Renderer *renderer = textures_.renderer();
  const Vec3 clear = lighting_.clear_colour;
  SDL_SetRenderDrawColorFloat(renderer, clear.x, clear.y, clear.z, 1.0F);
  SDL_RenderClear(renderer);
  if (viewport.pixel_width <= 0 || viewport.pixel_height <= 0) {
    return;
  }
  if (splits_.empty()) {
    draw_screen(viewport, 0);
  } else {
    // Each screen draws in its own rectangle, SDL's viewport moved there.
    const std::vector<SplitScreen> screens =
        split_screens(screens_, viewport.pixel_width, viewport.pixel_height);
    for (std::size_t index = 0; index < screens.size() && index <= splits_.size(); ++index) {
      const PixelRect &area = screens[index].rect;
      const SDL_Rect rect{static_cast<int>(area.x), static_cast<int>(area.y),
                          static_cast<int>(area.width), static_cast<int>(area.height)};
      SDL_SetRenderViewport(renderer, &rect);
      draw_screen(fit_screen({0.0F, 0.0F, area.width, area.height}, screens[index].canvas[0],
                             screens[index].canvas[1]),
                  index);
    }
    SDL_SetRenderViewport(renderer, nullptr);
  }
  if (scene_) {
    scene_->draw(viewport);
  }
  draw_message(viewport);
  if (chatter_) {
    chatter_->draw(viewport);
  }
  if (!missing_run_.empty()) {
    draw_notice(missing_run_, viewport);
  }
}

} // namespace phantomcrash
