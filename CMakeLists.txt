cmake_minimum_required(VERSION 3.13...3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_HOST_TESTS "Build host unit tests without the Pico SDK" OFF)

if (BUILD_HOST_TESTS)
    project(trackstomp_navigator_host_tests C CXX)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    enable_testing()

    add_library(firmware_host STATIC
        src/input/buttons.cpp
        src/sequencer/sequencer.cpp
        src/ui/ui.cpp
        src/midi/midi_out.cpp
        src/storage/config_store.cpp
        src/main.cpp
    )
    target_include_directories(firmware_host PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(firmware_host PUBLIC HOST_TEST)

    add_library(harness STATIC
        tests/harness/harness.cpp
        tests/harness/gpio_host.cpp
    )
    target_include_directories(harness PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/harness
    )
    target_link_libraries(harness PUBLIC firmware_host)
    # gpio_host.cpp supplies buttons_gpio_levels(); buttons_scan consumes it.
    target_link_libraries(firmware_host PUBLIC harness)

    add_executable(test_cue_table
        tests/test_cue_table.cpp
    )
    target_include_directories(test_cue_table PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_cue_table PRIVATE
        "PRD_PATH=\"${CMAKE_SOURCE_DIR}/docs/PRD.md\""
    )
    add_test(NAME cue_table_matches_prd COMMAND test_cue_table)

    add_executable(test_bounce tests/test_bounce.cpp)
    target_link_libraries(test_bounce PRIVATE harness)
    add_test(NAME bounce_injection COMMAND test_bounce)

    add_executable(test_debounce tests/test_debounce.cpp)
    target_link_libraries(test_debounce PRIVATE harness)
    add_test(NAME debounce_filter COMMAND test_debounce)

    add_executable(test_timing tests/test_timing.cpp)
    target_link_libraries(test_timing PRIVATE harness)
    add_test(NAME timing_deadline COMMAND test_timing)

    add_executable(test_tap_hold tests/test_tap_hold.cpp)
    target_link_libraries(test_tap_hold PRIVATE harness)
    add_test(NAME tap_hold_classification COMMAND test_tap_hold)

    add_executable(test_chord tests/test_chord.cpp)
    target_link_libraries(test_chord PRIVATE harness)
    add_test(NAME chord_detection COMMAND test_chord)

    add_executable(test_lockout tests/test_lockout.cpp)
    target_link_libraries(test_lockout PRIVATE harness)
    add_test(NAME hold_lockout COMMAND test_lockout)

    add_executable(test_sequence tests/test_sequence.cpp)
    target_link_libraries(test_sequence PRIVATE harness)
    add_test(NAME sequence_state_machine COMMAND test_sequence)

    add_executable(test_setup tests/test_setup.cpp)
    target_link_libraries(test_setup PRIVATE harness)
    add_test(NAME setup_mode_state COMMAND test_setup)

    add_executable(test_led_engine tests/test_led_engine.cpp)
    target_link_libraries(test_led_engine PRIVATE harness)
    add_test(NAME led_pattern_engine COMMAND test_led_engine)

    add_executable(test_led_perf tests/test_led_perf.cpp)
    target_link_libraries(test_led_perf PRIVATE harness)
    add_test(NAME led_performance_patterns COMMAND test_led_perf)

    add_executable(test_led_chord tests/test_led_chord.cpp)
    target_link_libraries(test_led_chord PRIVATE harness)
    add_test(NAME led_chord_progress COMMAND test_led_chord)

    add_executable(test_led_channel tests/test_led_channel.cpp)
    target_link_libraries(test_led_channel PRIVATE harness)
    add_test(NAME led_channel_blink COMMAND test_led_channel)

    add_executable(test_midi_out tests/test_midi_out.cpp)
    target_link_libraries(test_midi_out PRIVATE harness)
    add_test(NAME midi_note_transmission COMMAND test_midi_out)

    add_executable(test_config_store tests/test_config_store.cpp)
    target_link_libraries(test_config_store PRIVATE harness)
    add_test(NAME config_store_read COMMAND test_config_store)

    add_executable(test_loop tests/test_loop.cpp)
    target_link_libraries(test_loop PRIVATE harness)
    add_test(NAME main_loop_dispatch COMMAND test_loop)

    add_executable(test_release tests/test_release.cpp)
    target_compile_definitions(test_release PRIVATE
        "TUSB_CONFIG_PATH=\"${CMAKE_SOURCE_DIR}/src/midi/tusb_config.h\""
        "CMAKE_LISTS_PATH=\"${CMAKE_SOURCE_DIR}/CMakeLists.txt\""
    )
    add_test(NAME release_build_hardening COMMAND test_release)

    add_executable(test_capture tests/test_capture.cpp)
    target_link_libraries(test_capture PRIVATE harness)
    add_test(NAME capture_buffers COMMAND test_capture)
else()
    set(PICO_BOARD pico2)
    set(PICOTOOL_FETCH_FROM_GIT_PATH $ENV{HOME}/pico/picotool-build)

    if (NOT PICO_SDK_PATH AND NOT DEFINED ENV{PICO_SDK_PATH})
        message(FATAL_ERROR "PICO_SDK_PATH is unset. Point it at Pico SDK 2.3.1 or later.")
    endif()

    # initialize the SDK based on PICO_SDK_PATH
    # note: this must happen before project()
    include(pico_sdk_import.cmake)

    if (PICO_SDK_VERSION_STRING VERSION_LESS "2.3.1")
        message(FATAL_ERROR
            "Raspberry Pi Pico SDK 2.3.1 or later is required. Found ${PICO_SDK_VERSION_STRING}."
        )
    endif()

    project(trackstomp_navigator C CXX ASM)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    option(DEBUG_UART "Enable UART-only stdio on GP0/GP1" OFF)

    pico_sdk_init()

    add_executable(trackstomp_navigator
        src/main.cpp
        src/input/buttons.cpp
        src/sequencer/sequencer.cpp
        src/midi/midi_out.cpp
        src/midi/usb_descriptors.c
        src/storage/config_store.cpp
        src/ui/ui.cpp
    )

    target_include_directories(trackstomp_navigator PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/midi
    )

    target_link_libraries(trackstomp_navigator
        pico_stdlib
        pico_unique_id
        tinyusb_device
        hardware_flash
        hardware_sync
    )

    pico_enable_stdio_usb(trackstomp_navigator 0)
    if (DEBUG_UART)
        pico_enable_stdio_uart(trackstomp_navigator 1)
        target_compile_definitions(trackstomp_navigator PRIVATE DEBUG_UART)
    else()
        pico_enable_stdio_uart(trackstomp_navigator 0)
    endif()

    # PRD §5.1: do not embed absolute source paths in __FILE__ or DWARF.
    target_compile_options(trackstomp_navigator PRIVATE
        "-ffile-prefix-map=${CMAKE_SOURCE_DIR}=."
        "-ffile-prefix-map=${PICO_SDK_PATH}=pico-sdk"
    )

    pico_add_extra_outputs(trackstomp_navigator)

    add_custom_command(TARGET trackstomp_navigator POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DELF=$<TARGET_FILE:trackstomp_navigator>
            -DUF2=${CMAKE_CURRENT_BINARY_DIR}/trackstomp_navigator.uf2
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_no_abs_paths.cmake
        VERBATIM
    )

    # PRD §10.1: last 4 KB flash sector is reserved for ConfigRecord and must
    # sit outside the linker image so a firmware update cannot clobber it.
    # pico_override_flash_size also redefines PICO_FLASH_SIZE_BYTES, which
    # config_store uses as the reserved-sector offset (Q016).
    pico_override_flash_size(trackstomp_navigator "(4 * 1024 * 1024) - 4096")
endif()
