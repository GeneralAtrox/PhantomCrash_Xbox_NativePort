#include "desktop_draw_list.hpp"
#include "desktop_replay.hpp"
#include "phantomcrash/json_fields.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace phantomcrash {
namespace {
constexpr int free_view = 6, player_view = 7;
constexpr std::array<int, 6> original_views{camera_view::fixed,     camera_view::first_person,
                                            camera_view::behind,    camera_view::right_side,
                                            camera_view::left_side, camera_view::front};
} // namespace
ReplayScreen::ReplayScreen(TextureCache &textures, const GlyphFont &font, WorldRenderer *world,
                           std::shared_ptr<ReplayArchive> archive, const json::JsonValue &data)
    : textures_(textures), font_(font), world_(world), archive_(std::move(archive)),
      chase_(archive_->chase) {
  for (const auto &name : json::required(data, "views").array()) {
    names_.push_back(name.string());
  }
  if (names_.size() != 8) {
    throw std::runtime_error("Replay needs eight view names");
  }
  help_ = data.string_or("help", "");
  play_label_ = data.string_or("play", "Play");
  pause_label_ = data.string_or("pause", "Pause");
  stop_label_ = data.string_or("stop", "Stop");
  time_unit_ = data.string_or("time_unit", "sec");
  free_speed_ = static_cast<float>(data.number_or("free_speed", 30));
  turn_speed_ = static_cast<float>(data.number_or("turn_speed", 2));
  drag_scale_ = static_cast<float>(data.number_or("drag_radians_per_unit", 0.006));
  frame_ = archive_->read(0);
  sounds_ = frame_.sounds;
  for (const auto &cue : sounds_) {
    if (cue.key && cue.loop) {
      loops_[cue.key] = cue;
    }
  }
  if (archive_->roots) {
    cockpit_ = load_cockpit(world_, *archive_->roots);
  }
  select_view(player_view);
  SDL_Log("Replay started: %zu frames, %.2f seconds", archive_->size(), archive_->duration());
}
void ReplayScreen::stop_sounds(const bool clear) {
  for (const auto &[key, cue] : loops_) {
    sounds_.push_back({SoundCue::Action::stop, {}, {}, false, key});
  }
  if (clear) {
    loops_.clear();
  }
}
void ReplayScreen::select_view(int mode) {
  mode_ = (mode + 8) % 8;
  snap_ = true;
  views_.reset();
  if (mode_ < free_view) {
    auto settings = archive_->views;
    settings.start_view = original_views[mode_];
    views_ = std::make_unique<CameraViews>(settings, archive_->fixed_points);
  }
  if (mode_ == free_view) {
    free_eye_ = frame_.player.eye;
    const auto &m = frame_.player.view;
    const Vec3 direction{m.at(0, 2), m.at(1, 2), m.at(2, 2)};
    free_yaw_ = std::atan2(direction.x, direction.z);
    free_pitch_ = std::asin(std::clamp(direction.y, -1.0F, 1.0F));
  }
  SDL_Log("Replay view: %s", names_[mode_].c_str());
}
void ReplayScreen::select_subject(const int direction) {
  const int count = static_cast<int>(frame_.subjects.size());
  if (!count) {
    return;
  }
  for (int i = 0; i < count; ++i) {
    subject_ = (subject_ + direction + count) % count;
    if (frame_.subjects[subject_].alive) {
      break;
    }
  }
  if (mode_ == player_view) {
    select_view(2); // AI pilots have no recorded human viewport.
  }
  if (mode_ == free_view) {
    const auto &subject = frame_.subjects[subject_].camera;
    chase_.tick(subject.position, subject.yaw, subject.pitch, archive_->collision, 0, true);
    free_eye_ = chase_.chase().eye;
    const Vec3 forward = normalize(chase_.chase().target - free_eye_);
    free_yaw_ = std::atan2(forward.x, forward.z);
    free_pitch_ = std::asin(std::clamp(forward.y, -1.0F, 1.0F));
  }
  snap_ = true;
  SDL_Log("Replay subject: %s", frame_.subjects[subject_].name.c_str());
}
void ReplayScreen::tick(const InputFrame &input, const double seconds) {
  controls_seconds_ -= seconds;
  if (input.pressed || input.pointer_moved || input.mouse_right_held) {
    controls_seconds_ = 3;
  }
  if (input.any_pressed(button::cancel)) {
    stop_sounds();
    done_ = true;
    return;
  }
  if (input.any_pressed(button::x)) {
    show_controls_ = !show_controls_;
  }
  if (input.any_pressed(button::pad_right)) {
    select_view(mode_ + 1);
  }
  if (input.any_pressed(button::pad_left)) {
    select_view(mode_ - 1);
  }
  if (input.any_pressed(button::black)) {
    select_subject(1);
  }
  if (input.any_pressed(button::white)) {
    select_subject(-1);
  }
  bool pause = input.any_pressed(button::confirm);
  if (input.clicked && input.pointer && (*input.pointer)[1] >= 430) {
    const float x = (*input.pointer)[0];
    if (x < 180) {
      select_view(mode_ + 1);
    } else if (x < 400) {
      select_subject(1);
    } else if (x < 520) {
      pause = true;
    } else {
      stop_sounds();
      done_ = true;
      return;
    }
  }
  if (pause) {
    paused_ = !paused_;
    if (paused_) {
      stop_sounds(false);
    } else {
      for (const auto &[key, cue] : loops_) {
        sounds_.push_back(cue);
      }
    }
  }
  if (!paused_) {
    time_ += seconds;
    if (time_ > archive_->duration() + 1e-9) {
      stop_sounds();
      done_ = true;
      return;
    }
    const auto next = archive_->at(time_);
    while (index_ < next) {
      frame_ = archive_->read(++index_);
      for (const auto &cue : frame_.sounds) {
        sounds_.push_back(cue);
        if (cue.key && (cue.loop || cue.action == SoundCue::Action::move) &&
            cue.action != SoundCue::Action::stop) {
          if (cue.action == SoundCue::Action::play) {
            loops_[cue.key] = cue;
          } else if (auto active = loops_.find(cue.key); active != loops_.end()) {
            active->second.mix = cue.mix;
          }
        }
        if (cue.action == SoundCue::Action::stop) {
          loops_.erase(cue.key);
        }
      }
    }
  }
  camera(input, seconds);
}
void ReplayScreen::camera(const InputFrame &input, const double seconds) {
  const float dt = static_cast<float>(seconds);
  if (mode_ == free_view) {
    free_yaw_ += input.right_stick[0] * turn_speed_ * dt;
    free_pitch_ += input.right_stick[1] * turn_speed_ * dt;
    if (input.mouse_right_held && input.pointer && pointer_) {
      free_yaw_ += ((*input.pointer)[0] - (*pointer_)[0]) * drag_scale_;
      free_pitch_ -= ((*input.pointer)[1] - (*pointer_)[1]) * drag_scale_;
    }
    free_pitch_ = std::clamp(free_pitch_, -1.5F, 1.5F);
    const Vec3 forward{std::sin(free_yaw_) * std::cos(free_pitch_), std::sin(free_pitch_),
                       std::cos(free_yaw_) * std::cos(free_pitch_)};
    const Vec3 right{std::cos(free_yaw_), 0, -std::sin(free_yaw_)};
    const float vertical = ((input.held & button::right_trigger) ? 1.0F : 0.0F) -
                           ((input.held & button::left_trigger) ? 1.0F : 0.0F);
    free_eye_ = free_eye_ + (forward * input.left_stick[1] + right * input.left_stick[0] +
                             Vec3{0, vertical, 0}) *
                                (free_speed_ * dt);
    shot_ = {free_eye_, free_eye_ + forward, archive_->chase.fov_degrees, 0};
  } else if (views_ && !frame_.subjects.empty()) {
    subject_ = std::min(subject_, static_cast<int>(frame_.subjects.size()) - 1);
    const auto &subject = frame_.subjects[subject_].camera;
    chase_.tick(subject.position, subject.yaw, subject.pitch, archive_->collision, dt, snap_);
    views_->tick(
        subject, chase_.chase(), dt,
        [this](Vec3 from, Vec3 to) { return !archive_->collision.segment_hit(from, to); }, snap_);
    if (cockpit_ && mode_ == 1) {
      cockpit_->tick(frame_.subjects[subject_].cockpit, paused_ ? 0 : dt);
    }
    shot_ = views_->shot();
    snap_ = false;
  }
  pointer_ = input.pointer;
}
void ReplayScreen::draw(const Viewport &viewport) {
  clear_black(textures_);
  auto world = frame_.player;
  auto *renderer = textures_.renderer();
  if (mode_ == player_view) {
    const float scale = std::min(static_cast<float>(viewport.pixel_width) / world.width,
                                 static_cast<float>(viewport.pixel_height) / world.height);
    const int width = static_cast<int>(world.width * scale);
    const int height = static_cast<int>(world.height * scale);
    const SDL_Rect area{(viewport.pixel_width - width) / 2, (viewport.pixel_height - height) / 2,
                        width, height};
    SDL_SetRenderViewport(renderer, &area);
    world.width = width;
    world.height = height;
    world_->draw(world, frame_.monitor ? &*frame_.monitor : nullptr);
    draw_replay_overlay(renderer, frame_.overlay, scale);
    SDL_SetRenderViewport(renderer, nullptr);
  } else {
    world.items.insert(world.items.end(), frame_.hidden_player.begin(), frame_.hidden_player.end());
    world.overlay.clear();
    if (mode_ == 1) {
      std::erase_if(world.items, [this](const auto &item) { return item.robot == subject_; });
      if (cockpit_ && subject_ < static_cast<int>(frame_.subjects.size())) {
        cockpit_->add_items(frame_.subjects[subject_].cockpit, time_, world.overlay);
      }
    }
    world.width = viewport.pixel_width;
    world.height = viewport.pixel_height;
    world.eye = shot_.eye;
    world.view = look_at(shot_.eye, shot_.target, {0, 1, 0});
    world.projection =
        perspective(shot_.fov_degrees * pi / 180.0F, static_cast<float>(world.width) / world.height,
                    archive_->chase.near_plane, archive_->chase.far_plane);
    world_->draw(world, frame_.monitor ? &*frame_.monitor : nullptr);
  }
  if (show_controls_ && controls_seconds_ > 0) {
    controls(viewport);
  }
}
void ReplayScreen::controls(const Viewport &viewport) {
  auto *renderer = textures_.renderer();
  const auto box = viewport.to_pixels(0, 414, 640, 66);
  const SDL_FRect rect{box.x, box.y, box.width, box.height};
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 215);
  SDL_RenderFillRect(renderer, &rect);
  DrawList list(textures_, font_, viewport, time_);
  const auto write = [&](const std::string &s, float x, float y, float scale) {
    auto quads = layout_run(font_, glyph_run(font_, s), 0, 0);
    for (auto &q : quads) {
      q.x = x + q.x * scale;
      q.y = y + q.y * scale;
      q.width *= scale;
      q.height *= scale;
    }
    list.text(270, std::move(quads), 0xffffff);
  };
  write(help_, 8, 416, 0.5F);
  write(names_[mode_] + " >", 8, 439, 0.7F);
  const int watched = mode_ == player_view ? 0 : subject_;
  if (watched < static_cast<int>(frame_.subjects.size())) {
    write(frame_.subjects[watched].name + " >", 185, 439, 0.7F);
  }
  write(paused_ ? play_label_ : pause_label_, 410, 439, 0.7F);
  write(stop_label_, 540, 439, 0.7F);
  std::ostringstream time;
  time << static_cast<int>(time_) << " / " << static_cast<int>(archive_->duration()) << ' '
       << time_unit_;
  write(time.str(), 8, 464, 0.6F);
  list.flush();
}
} // namespace phantomcrash
