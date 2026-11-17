cmake_minimum_required(VERSION 3.24)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BANCADA_VERSION LIMIT_COUNT 1)
project(Bancada VERSION ${BANCADA_VERSION} LANGUAGES C)
if(NOT WIN32 OR NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Bancada currently requires Windows x64 and MinGW UCRT64.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Werror -UNDEBUG)
# Large benches (256 parts x 420 pins) keep pin-sized union-find arrays on the stack.
add_link_options(-Wl,--stack,67108864)
find_package(SDL3 CONFIG REQUIRED)
add_executable(bancada WIN32
    src/workbench_window.c
    src/workbench_grid.c
    src/app_text_utf8.c
    src/app_commands.c
    src/app_editing.c
    src/app_settings.c
    src/app_preferences.c
    src/app_layout.c
    src/app_packing.c
    src/app_editor.c
    src/workbench_group_edit.c
    src/workbench_selection.c
    src/workbench_pack.c
    src/app_input.c
    src/app_code_editor.c
    src/app_render.c
    src/app_palette.c
    src/app_state.c
    src/mcu_runtime.c
    src/mcu_lang.c
    src/code_text.c
    src/app_simulation.c
    src/sim_state.c
    src/dev_capture.c
    src/dev_scenarios.c
    src/workbench.c
    src/bench_path.c
    src/component_registry.c
    src/component_devices.c
    src/workbench_examples.c
    src/workbench_view.c
    src/workbench_file.c
    src/workbench_pack_file.c
    src/workbench_group_name.c
    src/workbench_history.c
    src/netlist.c
    src/netlist_devices.c
    src/mcu_board.c
    src/spice_process.c
    src/spice_job.c
    src/spice_live.c
    src/spice_session.c
    src/spice_trace.c
    src/sim_auto.c
    src/ui_text.c
)
target_link_libraries(bancada PRIVATE SDL3::SDL3 opengl32 gdi32)
add_executable(spice-live-host src/spice_live_host.c src/ui_text.c)
target_include_directories(spice-live-host PRIVATE src)
include(CTest)
if(BUILD_TESTING)
    add_executable(test_recursive_chips
        tests/test_recursive_chips.c
        src/spice_process.c
        src/netlist.c
        src/netlist_devices.c
        src/mcu_board.c
        src/workbench_group_edit.c
        src/workbench_group_name.c
        src/workbench_pack.c
        src/workbench_selection.c
        src/workbench_file.c
        src/workbench_pack_file.c
        src/workbench.c
        src/bench_path.c
        src/component_registry.c
        src/component_devices.c
        src/ui_text.c
    )
    target_include_directories(test_recursive_chips PRIVATE src)
    add_test(NAME recursive_chips COMMAND test_recursive_chips)
    add_executable(test_ui_text tests/test_ui_text.c src/ui_text.c)
    target_include_directories(test_ui_text PRIVATE src)
    add_test(NAME ui_text COMMAND test_ui_text)
    add_executable(test_spice_session
        tests/test_spice_session.c
        src/spice_session.c
        src/ui_text.c
    )
    target_include_directories(test_spice_session PRIVATE src)
    add_executable(test_spice_live
        tests/test_spice_live.c
        src/spice_live.c
        src/spice_session.c
        src/sim_auto.c
        src/spice_process.c
        src/netlist.c
        src/netlist_devices.c
        src/mcu_board.c
        src/workbench.c
        src/bench_path.c
        src/component_registry.c
        src/component_devices.c
        src/ui_text.c
    )
    target_include_directories(test_spice_live PRIVATE src)
    add_executable(test_spice_process
        tests/test_spice_process.c
        src/spice_process.c
        src/netlist.c
        src/netlist_devices.c
        src/mcu_board.c
        src/workbench.c
        src/bench_path.c
        src/component_registry.c
        src/component_devices.c
        src/ui_text.c
    )
    target_include_directories(test_spice_process PRIVATE src)
    add_executable(test_workbench_examples
        tests/test_workbench_examples.c
        src/workbench_examples.c
        src/spice_process.c
        src/netlist.c
        src/netlist_devices.c
        src/mcu_board.c
        src/workbench.c
        src/bench_path.c
        src/component_registry.c
        src/component_devices.c
        src/ui_text.c
    )
    target_include_directories(test_workbench_examples PRIVATE src)
    add_executable(test_terminal_contacts
        tests/test_terminal_contacts.c
        src/workbench_examples.c
        src/workbench_pack.c
        src/workbench_selection.c
        src/workbench_file.c
        src/workbench_pack_file.c
        src/workbench_group_name.c
        src/spice_process.c
        src/netlist.c
        src/netlist_devices.c
        src/mcu_board.c
        src/workbench.c
        src/bench_path.c
        src/component_registry.c
        src/component_devices.c
        src/ui_text.c
    )
    target_include_directories(test_terminal_contacts PRIVATE src)
    add_test(NAME terminal_contacts COMMAND test_terminal_contacts)
    add_test(NAME gui COMMAND bancada --self-test-history)
    set_tests_properties(gui PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" TIMEOUT 20)
endif()
# CMake installs one prepared payload for both ZIP and NSIS. No dependency downloads at configure time.
set(BANCADA_PAYLOAD "" CACHE PATH "Prepared runtime, documentation, licenses and sources")
install(TARGETS bancada spice-live-host RUNTIME DESTINATION .)
if(BANCADA_PAYLOAD)
    install(DIRECTORY "${BANCADA_PAYLOAD}/" DESTINATION .)
endif()
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME Bancada)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "Bancada-${PROJECT_VERSION}-windows-x64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
