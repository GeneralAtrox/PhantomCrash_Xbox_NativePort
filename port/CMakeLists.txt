if(MSVC)
  add_compile_options(/MP /utf-8)
endif()
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

# Platform-independent core: data loading, input model, timing, canvas maths.
add_library(phantomcrash_core
  src/match_result.cpp
  src/location_menu.cpp
  src/location_menu_script.cpp
  src/data_roots.cpp
  src/event_player.cpp
  src/frame_rate.cpp
  src/front_end.cpp
  src/front_end_script.cpp
  src/glyph_font.cpp
  src/input.cpp
  src/intro_sequence.cpp
  src/json.cpp
  src/json_access.cpp
  src/jukebox.cpp
  src/location_frame.cpp
  src/music_spectrum.cpp
  src/battle.cpp
  src/battle_damage.cpp
  src/entry_shield.cpp
  src/effect_mixer.cpp
  src/world_sound.cpp
  src/frame_stats.cpp
  src/battle_sounds.cpp
  src/battle_chatter.cpp
  src/battle_scene.cpp
  src/battle_scene_robots.cpp
  src/battle_scene_tables.cpp
  src/robot_load.cpp
  src/battle_ai.cpp
  src/battle_fire.cpp
  src/battle_roster.cpp
  src/battle_weapon_tuning.cpp
  src/battle_weapons.cpp
  src/battle_weapon_targets.cpp
  src/fence_impacts.cpp
  src/stage_collision.cpp
  src/stage_doors.cpp
  src/stage_objects.cpp
  src/battle_objects.cpp
  src/battle_hud_style.cpp
  src/battle_hud_state.cpp
  src/battle_panels_style.cpp
  src/button_config.cpp
  src/battle_camera.cpp
  src/battle_monitor.cpp
  src/chase_camera.cpp
  src/cockpit.cpp
  src/optical_camouflage.cpp
  src/robot_shadow.cpp
  src/effect_bank.cpp
  src/effect_pictures.cpp
  src/effect_flashes.cpp
  src/effect_particles.cpp
  src/effect_system.cpp
  src/shadow_volume.cpp
  src/supply_drop.cpp
  src/supply_settings.cpp
  src/battle_supply.cpp
  src/battle_watch.cpp
  src/split_screen.cpp
  src/sleep.cpp
  src/contests.cpp
  src/schedule.cpp
  src/schedule_script.cpp
  src/contest_info.cpp
  src/rumbling_rules.cpp
  src/rumbling_script.cpp
  src/robot_preview.cpp
  src/rumble.cpp
  src/rumbling.cpp
  src/rumbling_gates.cpp
  src/rumbling_map.cpp
  src/rumbling_robot.cpp
  src/rumbling_world.cpp
  src/rumbling_roster.cpp
  src/rumbling_battle.cpp
  src/vs_scoobee.cpp
  src/saved_time.cpp
  src/keep_sv_script.cpp
  src/keep_sv.cpp
  src/card_row.cpp
  src/vs_mode_script.cpp
  src/vs_mode.cpp
  src/vs_mode_input.cpp
  src/vs_mode_area.cpp
  src/vs_battle.cpp
  src/town_screen.cpp
  src/records.cpp
  src/records_script.cpp
  src/battle_music.cpp
  src/battle_music_script.cpp
  src/shop_dialog.cpp
  src/sonic_amp.cpp
  src/sonic_amp_script.cpp
  src/garage.cpp
  src/garage_tables.cpp
  src/dealer_script.cpp
  src/dealer.cpp
  src/sv_purchase.cpp
  src/used_sv_script.cpp
  src/used_sv.cpp
  src/chip_shop_script.cpp
  src/chip_shop.cpp
  src/chip_uplift.cpp
  src/part_stats.cpp
  src/shop_goods.cpp
  src/sort_hints.cpp
  src/module_shop_script.cpp
  src/shop_list_style.cpp
  src/tune_shop_script.cpp
  src/tune_shop.cpp
  src/hangar_menu_script.cpp
  src/hangar_menu.cpp
  src/assemble_script.cpp
  src/assemble_fitting.cpp
  src/assemble.cpp
  src/sv_spec.cpp
  src/sv_spec_script.cpp
  src/test_drive.cpp
  src/robot_totals.cpp
  src/module_shop.cpp
  src/music_library.cpp
  src/enemy_path.cpp
  src/menu_cursor.cpp
  src/menu_screen.cpp
  src/message_box.cpp
  src/motion.cpp
  src/name_entry.cpp
  src/name_entry_script.cpp
  src/option_menu.cpp
  src/option_script.cpp
  src/pepper_guide.cpp
  src/pepper_guide_script.cpp
  src/load_game.cpp
  src/load_game_script.cpp
  src/town_map.cpp
  src/town_map_script.cpp
  src/robot.cpp
  src/robot_library.cpp
  src/robot_mover.cpp
  src/robot_parts.cpp
  src/screen_adjust.cpp
  src/settings.cpp
  src/host_controls.cpp
  src/quest_json.cpp
  src/skeleton.cpp
  src/sound_config.cpp
  src/sprite_layer.cpp
  src/story_event_script.cpp
  src/training_battle.cpp
  src/training_ground.cpp
  src/tutorial.cpp
  src/version.cpp
  src/world_model.cpp
)

# cgltf (header only) reads the converted glTF models.
find_path(CGLTF_INCLUDE_DIRS cgltf.h REQUIRED)

target_include_directories(phantomcrash_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CGLTF_INCLUDE_DIRS}
)

target_compile_features(phantomcrash_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(phantomcrash_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(phantomcrash_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(phantomcrash_core_tests
    tests/match_result_tests.cpp
    tests/club_tests.cpp
    tests/core_tests.cpp
    tests/data_tests.cpp
    tests/event_player_tests.cpp
    tests/guide_tests.cpp
    tests/front_end_tests.cpp
    tests/host_tests.cpp
    tests/host_controls_tests.cpp
    tests/robot_preview_tests.cpp
    tests/rumble_tests.cpp
    tests/intro_tests.cpp
    tests/load_game_tests.cpp
    tests/music_spectrum_tests.cpp
    tests/town_map_tests.cpp
    tests/name_entry_tests.cpp
    tests/option_screen_tests.cpp
    tests/option_tests.cpp
    tests/battle_tests.cpp
    tests/armory_tests.cpp
    tests/battle_damage_tests.cpp
    tests/battle_chatter_tests.cpp
    tests/battle_scene_tests.cpp
    tests/battle_scene_robots_tests.cpp
    tests/world_sound_tests.cpp
    tests/battle_sounds_tests.cpp
    tests/robot_load_tests.cpp
    tests/stage_collision_tests.cpp
    tests/stage_doors_tests.cpp
    tests/stage_objects_tests.cpp
    tests/fence_impacts_tests.cpp
    tests/battle_monitor_tests.cpp
    tests/battle_camera_tests.cpp
    tests/cockpit_tests.cpp
    tests/optical_camouflage_tests.cpp
    tests/supply_drop_tests.cpp
    tests/supply_destruction_tests.cpp
    tests/split_screen_tests.cpp
    tests/garage_tests.cpp
    tests/town_screen_tests.cpp
    tests/records_tests.cpp
    tests/battle_music_tests.cpp
    tests/sonic_amp_tests.cpp
    tests/garage_stock_tests.cpp
    tests/dealer_tests.cpp
    tests/used_sv_tests.cpp
    tests/chip_shop_tests.cpp
    tests/chip_uplift_tests.cpp
    tests/tune_shop_tests.cpp
    tests/hangar_menu_tests.cpp
    tests/assemble_tests.cpp
    tests/sv_spec_tests.cpp
    tests/test_drive_tests.cpp
    tests/rumbling_tests.cpp
    tests/rumbling_live_tests.cpp
    tests/rumbling_roster_tests.cpp
    tests/keep_sv_tests.cpp
    tests/vs_mode_tests.cpp
    tests/stock_module_tests.cpp
    tests/module_shop_tests.cpp
    tests/system_menu_tests.cpp
    tests/battle_hud_style_tests.cpp
    tests/robot_tests.cpp
    tests/robot_parts_tests.cpp
    tests/tutorial_tests.cpp
    tests/world_tests.cpp
    tests/shadow_volume_tests.cpp
    tests/effect_tests.cpp
  )
  target_link_libraries(phantomcrash_core_tests PRIVATE phantomcrash_core)
  # The shipped data/ documents are part of the source tree and tested too.
  target_compile_definitions(phantomcrash_core_tests
    PRIVATE PHANTOMCRASH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
  if(MSVC)
    target_compile_options(phantomcrash_core_tests PRIVATE /W4 /permissive-)
  endif()
  add_test(NAME phantomcrash_core_tests COMMAND phantomcrash_core_tests)
endif()

if(PHANTOMCRASH_BUILD_DESKTOP)
  find_package(SDL3 CONFIG REQUIRED)
  find_package(SDL3_image CONFIG REQUIRED)
  find_package(ZLIB REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswresample
    libswscale
  )

  # FFmpeg movie decoding without SDL, shared by the port and the media test.
  add_library(phantomcrash_media STATIC src/movie_decoder.cpp)
  target_include_directories(phantomcrash_media PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(phantomcrash_media PUBLIC phantomcrash_core PkgConfig::FFMPEG)
  if(MSVC)
    target_compile_options(phantomcrash_media PRIVATE /W4 /permissive-)
  endif()

  add_executable(phantomcrash_port WIN32
    src/desktop_audio.cpp
    src/desktop_audio_effects.cpp
    src/desktop_battle_hud.cpp
    src/desktop_battle_sights.cpp
    src/desktop_battle_panels.cpp
    src/desktop_battle_chatter.cpp
    src/desktop_battle_scene.cpp
    src/desktop_battle_view.cpp
    src/desktop_location_menu.cpp
    src/desktop_draw_list.cpp
    src/desktop_event_text.cpp
    src/desktop_frame.cpp
    src/desktop_frame_counter.cpp
    src/desktop_frame_rate_dialog.cpp
    src/desktop_front_end.cpp
    src/desktop_host.cpp
    src/desktop_input.cpp
    src/desktop_input_bindings.cpp
    src/desktop_controls_dialog.cpp
    src/desktop_intro.cpp
    src/desktop_main.cpp
    src/desktop_name_entry.cpp
    src/desktop_option.cpp
    src/desktop_option_screens.cpp
    src/desktop_pepper_guide.cpp
    src/desktop_load_game.cpp
    src/desktop_town_map.cpp
    src/desktop_options.cpp
    src/desktop_screens.cpp
    src/desktop_screens_rumbling.cpp
    src/desktop_screens_music.cpp
    src/desktop_screens_menus.cpp
    src/desktop_screens_battle.cpp
    src/desktop_settings_menu.cpp
    src/desktop_talk.cpp
    src/desktop_text.cpp
    src/desktop_textures.cpp
    src/desktop_robot.cpp
    src/desktop_training.cpp
    src/desktop_training_result.cpp
    src/desktop_training_replay.cpp
    src/desktop_replay.cpp
    src/desktop_replay_archive.cpp
    src/desktop_replay_overlay.cpp
    src/desktop_match_result.cpp
    src/desktop_screens_result.cpp
    src/desktop_training_sound.cpp
    src/desktop_training_camera.cpp
    src/desktop_training_live.cpp
    src/desktop_training_split.cpp
    src/desktop_monitor_ship.cpp
    src/desktop_stage_objects.cpp
    src/desktop_game_screens.cpp
    src/desktop_records.cpp
    src/desktop_battle_music.cpp
    src/desktop_shop_dialog.cpp
    src/desktop_sonic_amp.cpp
    src/desktop_dealer.cpp
    src/desktop_sv_purchase.cpp
    src/desktop_used_sv.cpp
    src/desktop_chip_shop.cpp
    src/desktop_module_shop.cpp
    src/desktop_shop_list.cpp
    src/desktop_tune_shop.cpp
    src/desktop_hangar_menu.cpp
    src/desktop_assemble.cpp
    src/desktop_assemble_list.cpp
    src/desktop_robot_preview.cpp
    src/desktop_sv_spec.cpp
    src/desktop_part_stats.cpp
    src/desktop_chip_dialog.cpp
    src/desktop_records_convention.cpp
    src/desktop_schedule.cpp
    src/desktop_contest_info.cpp
    src/desktop_rumbling.cpp
    src/desktop_rumbling_robots.cpp
    src/desktop_keep_sv.cpp
    src/desktop_vs_mode.cpp
    src/desktop_vs_mode_area.cpp
    src/desktop_game_screens_vs.cpp
    src/desktop_town_screen.cpp
    src/desktop_cockpit.cpp
    src/desktop_supply.cpp
    src/desktop_effects.cpp
    src/desktop_shots.cpp
    src/desktop_training_robots.cpp
    src/desktop_world_gpu.cpp
    src/desktop_world_passes.cpp
    src/desktop_world_monitor.cpp
    src/desktop_world_shadows.cpp
    src/desktop_world_screen.cpp
    src/desktop_world_sprites.cpp
    src/desktop_world_renderer.cpp
    src/desktop_world_shaders.cpp
    src/movie_audio_output.cpp
    src/movie_player.cpp
  )
  target_link_libraries(phantomcrash_port PRIVATE
    phantomcrash_core phantomcrash_media SDL3::SDL3 SDL3_image::SDL3_image ZLIB::ZLIB)
  if(WIN32)
    target_link_libraries(phantomcrash_port PRIVATE comctl32 d3d11 dxgi d3dcompiler)
  endif()
  if(MSVC)
    target_compile_options(phantomcrash_port PRIVATE /W4 /permissive-)
  endif()

  if(BUILD_TESTING)
    add_executable(phantomcrash_input_tests tests/core_tests.cpp tests/desktop_input_tests.cpp
      tests/desktop_replay_archive_tests.cpp src/desktop_replay_archive.cpp
      src/desktop_input.cpp src/desktop_input_bindings.cpp)
    target_link_libraries(phantomcrash_input_tests PRIVATE phantomcrash_core SDL3::SDL3 ZLIB::ZLIB)
    add_test(NAME phantomcrash_input_tests COMMAND phantomcrash_input_tests)
    add_test(NAME phantomcrash_port_smoke COMMAND phantomcrash_port --smoke-test 60
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Title, menu, submenu and a placeholder screen, driven by injected presses.
    add_test(NAME phantomcrash_port_menu_smoke
             COMMAND phantomcrash_port --start title --smoke-test 500 --press 150:start
                     --press 200:down --press 230:a --press 260:up --press 290:a
                     --press 320:a --press 450:b
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The tutorial battle: Pepper's first pages, then walking, firing and back to Pepper.
    add_test(NAME phantomcrash_port_training_smoke
             COMMAND phantomcrash_port --start training_attack --smoke-test 400 --press 60:a
                     --press 120:a --press 180:a --press 200-300:up --press 250-320:right_trigger
                     --press 380:back
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The option screen: the vibration box answered NO, cancel to EXIT, back to the menu.
    add_test(NAME phantomcrash_port_option_smoke
             COMMAND phantomcrash_port --start option --smoke-test 300 --press 60:a
                     --press 90:right --press 120:a --press 150:b --press 180:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The option sub-screens: SOUND CONFIG (a volume step, the SE test, EXIT),
    # BUTTON CONFIG (a choice, EXIT) and SCREEN ADJUST (a step, answered NO).
    add_test(NAME phantomcrash_port_option_screens_smoke
             COMMAND phantomcrash_port --start option --smoke-test 700 --press 50:down
                     --press 60:a --press 140:left --press 150:down --press 160:down
                     --press 170:a --press 180:down --press 190:a --press 260:down
                     --press 270:a --press 340:right --press 350:up --press 360:a
                     --press 430:down --press 440:a --press 510:right --press 520:a
                     --press 530:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # NEW GAME: the opening movie skipped, event 48 fast-forwarded, a name
    # typed on NAME ENTRY, End, event 49 fast-forwarded, Pepper's guide
    # placeholder, and on to Club Wired.
    add_test(NAME phantomcrash_port_new_game_smoke
             COMMAND phantomcrash_port --start new_game --smoke-test 660 --press 20:a
                     --press 40-180:left_trigger --press 300:a --press 320:right
                     --press 330:a --press 350:b --press 370:a --press 460-620:left_trigger
                     --press 640:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Club Wired: the wheel turned both ways, cancel to Exit, back to
    # Training, into Pepper's guide (event 129 first) and back.
    add_test(NAME phantomcrash_port_club_smoke
             COMMAND phantomcrash_port --start club_wired --smoke-test 700 --press 60:right
                     --press 90:left --press 120:b --press 150:right --press 180:right
                     --press 200:a --press 260-620:left_trigger --press 660:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Pepper's guide on a cleared save riding a Scoobee (the RIDING SV plate):
    # into a topic, the cleared answer to "What is a Ranker?" (event 157)
    # fast-forwarded and back, the topic closed, cancel to End and out.
    add_test(NAME phantomcrash_port_guide_smoke
             COMMAND phantomcrash_port --start pepper_guide
                     --settings ${CMAKE_CURRENT_SOURCE_DIR}/tests/cleared_save.json
                     --smoke-test 720 --press 60:down --press 80:down --press 100:a
                     --press 120:down --press 130:down --press 140:a
                     --press 200-520:left_trigger --press 560:b --press 580:b --press 600:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # LOAD GAME with the cleared save in the first slot: the slots and EXIT
    # walked, cancel to EXIT, back up, the save chosen, its notice, the
    # garage placeholder. With no save, only the notice, and confirm leaves.
    add_test(NAME phantomcrash_port_load_smoke
             COMMAND phantomcrash_port --start load_game
                     --settings ${CMAKE_CURRENT_SOURCE_DIR}/tests/cleared_save.json
                     --smoke-test 400 --press 60:right --press 80:left --press 100:down
                     --press 120:up --press 140:b --press 160:up --press 180:a --press 260:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    add_test(NAME phantomcrash_port_load_empty_smoke
             COMMAND phantomcrash_port --start load_game
                     --settings ${CMAKE_CURRENT_BINARY_DIR}/no_such_settings.json
                     --smoke-test 200 --press 90:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The garage with the cleared save: the wheel turned, the Y and X
    # shortcuts, cancel to the way out, then the hangar: its menu and back.
    add_test(NAME phantomcrash_port_garage_smoke
             COMMAND phantomcrash_port --start garage
                     --settings ${CMAKE_CURRENT_SOURCE_DIR}/tests/cleared_save.json
                     --smoke-test 320 --press 60:right --press 90:y --press 130:b
                     --press 170:x --press 210:a --press 280:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The town map with the cleared save: round the places both ways, past
    # the ends, then into the garage.
    add_test(NAME phantomcrash_port_town_map_smoke
             COMMAND phantomcrash_port --start town_map
                     --settings ${CMAKE_CURRENT_SOURCE_DIR}/tests/cleared_save.json
                     --smoke-test 260 --press 60:left --press 80:left --press 100:right
                     --press 120:right --press 140:right --press 160:left --press 190:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The town map by mouse: onto Wild Machines' number, a turn round the
    # rider, then a click on Sonic Amp's name.
    add_test(NAME phantomcrash_port_town_map_mouse_smoke
             COMMAND phantomcrash_port --start town_map
                     --settings ${CMAKE_CURRENT_SOURCE_DIR}/tests/cleared_save.json
                     --smoke-test 240 --mouse 60:120,130 --mouse 100:122,137 --mouse 104:142,157
                     --mouse 108:122,177 --mouse 112:102,157 --mouse 116:122,138
                     --mouse 180:420,340:click
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # The first talk scene on its own, its pages turned by presses.
    add_test(NAME phantomcrash_port_talk_smoke
             COMMAND phantomcrash_port --start event_48 --smoke-test 400 --press 60:a
                     --press 80:a --press 100:a --press 120:a --press 140:a --press 160:a
                     --press 180:a --press 200:a --press 220:a --press 240:a --press 260:a
                     --press 280:a --press 300:a
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Silent audio, so test runs do not play sound.
    set_tests_properties(phantomcrash_port_smoke phantomcrash_port_menu_smoke
      phantomcrash_port_option_smoke phantomcrash_port_option_screens_smoke
      phantomcrash_port_new_game_smoke phantomcrash_port_club_smoke phantomcrash_port_talk_smoke
      phantomcrash_port_guide_smoke phantomcrash_port_load_smoke phantomcrash_port_load_empty_smoke
      phantomcrash_port_garage_smoke phantomcrash_port_town_map_smoke
      phantomcrash_port_town_map_mouse_smoke
      PROPERTIES
      TIMEOUT 60 ENVIRONMENT SDL_AUDIO_DRIVER=dummy)

    # Needs the movies extracted from the disc; reports a skip without them.
    add_executable(phantomcrash_media_tests tests/media_tests.cpp)
    target_link_libraries(phantomcrash_media_tests PRIVATE phantomcrash_media)
    target_compile_definitions(phantomcrash_media_tests
      PRIVATE PHANTOMCRASH_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
    if(MSVC)
      target_compile_options(phantomcrash_media_tests PRIVATE /W4 /permissive-)
    endif()
    add_test(NAME phantomcrash_media_tests COMMAND phantomcrash_media_tests)
    set_tests_properties(phantomcrash_media_tests PROPERTIES
      TIMEOUT 300 SKIP_RETURN_CODE 77 LABELS private-input)
  endif()
endif()
