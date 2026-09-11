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
        tinyusb_device
        hardware_flash
        hardware_sync
    )

    pico_enable_stdio_usb(trackstomp_navigator 0)
    if (DEBUG_UART)
        pico_enable_stdio_uart(trackstomp_navigator 1)
    else()
        pico_enable_stdio_uart(trackstomp_navigator 0)
    endif()

    pico_add_extra_outputs(trackstomp_navigator)
endif()
