if(MSVC)
  add_compile_options(/MP /utf-8)
endif()
if(WIN32)
  add_compile_definitions(NOMINMAX)
endif()

# Platform-independent core: data loading, input model, timing, canvas maths.
add_library(phantomcrash_core
  src/data_roots.cpp
  src/frame_rate.cpp
  src/front_end.cpp
  src/front_end_script.cpp
  src/glyph_font.cpp
  src/input.cpp
  src/intro_sequence.cpp
  src/json.cpp
  src/json_access.cpp
  src/button_config.cpp
  src/menu_cursor.cpp
  src/menu_screen.cpp
  src/message_box.cpp
  src/option_menu.cpp
  src/option_script.cpp
  src/screen_adjust.cpp
  src/settings.cpp
  src/sound_config.cpp
  src/sprite_layer.cpp
  src/version.cpp
)

target_include_directories(phantomcrash_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(phantomcrash_core PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(phantomcrash_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(phantomcrash_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
  add_executable(phantomcrash_core_tests
    tests/core_tests.cpp
    tests/data_tests.cpp
    tests/front_end_tests.cpp
    tests/host_tests.cpp
    tests/intro_tests.cpp
    tests/option_screen_tests.cpp
    tests/option_tests.cpp
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
    src/desktop_draw_list.cpp
    src/desktop_frame_counter.cpp
    src/desktop_frame_rate_dialog.cpp
    src/desktop_front_end.cpp
    src/desktop_host.cpp
    src/desktop_input.cpp
    src/desktop_intro.cpp
    src/desktop_main.cpp
    src/desktop_option.cpp
    src/desktop_option_screens.cpp
    src/desktop_options.cpp
    src/desktop_screens.cpp
    src/desktop_settings_menu.cpp
    src/desktop_text.cpp
    src/desktop_textures.cpp
    src/movie_audio_output.cpp
    src/movie_player.cpp
  )
  target_link_libraries(phantomcrash_port PRIVATE
    phantomcrash_core phantomcrash_media SDL3::SDL3 SDL3_image::SDL3_image)
  if(WIN32)
    target_link_libraries(phantomcrash_port PRIVATE comctl32)
  endif()
  if(MSVC)
    target_compile_options(phantomcrash_port PRIVATE /W4 /permissive-)
  endif()

  if(BUILD_TESTING)
    add_test(NAME phantomcrash_port_smoke COMMAND phantomcrash_port --smoke-test 60
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    # Title, menu, submenu and a placeholder screen, driven by injected presses.
    add_test(NAME phantomcrash_port_menu_smoke
             COMMAND phantomcrash_port --start title --smoke-test 500 --press 150:start
                     --press 200:down --press 230:a --press 260:up --press 290:a
                     --press 320:a --press 450:b
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
    # Silent audio, so test runs do not play sound.
    set_tests_properties(phantomcrash_port_smoke phantomcrash_port_menu_smoke
      phantomcrash_port_option_smoke phantomcrash_port_option_screens_smoke PROPERTIES
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
