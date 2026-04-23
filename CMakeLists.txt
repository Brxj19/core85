cmake_minimum_required(VERSION 3.25)

project(Core85 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CORE85_BUILD_GUI "Build the Qt GUI application" ON)
option(CORE85_BUILD_TESTS "Build the GTest test suite" ON)

add_library(core85_lib STATIC
    src/core/assembler.cpp
    src/core/cpu.cpp
    src/core/io_bus.cpp
    src/core/memory.cpp
)

target_include_directories(core85_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(core85_lib PRIVATE /W4)
else()
    target_compile_options(core85_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CORE85_BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets QUIET)

    if(Qt6_FOUND)
        qt_add_executable(core85_gui
            src/gui/main.cpp
            src/gui/codeeditor.cpp
            src/gui/emulator_types.cpp
            src/gui/emulatorcontroller.cpp
            src/gui/findreplacebar.cpp
            src/gui/iopanel.cpp
            src/gui/mainwindow.cpp
            src/gui/mainwindow.h
            src/gui/memoryviewer.cpp
            src/gui/problemspanel.cpp
            src/gui/registerviewer.cpp
            src/gui/value_utils.cpp
            src/gui/welcomepage.cpp
        )

        set_target_properties(core85_gui PROPERTIES
            AUTOMOC ON
            AUTOUIC ON
            AUTORCC ON
        )

        target_link_libraries(core85_gui
            PRIVATE
                Qt6::Widgets
                core85_lib
        )

        add_custom_target(run
            COMMAND $<TARGET_FILE:core85_gui>
            DEPENDS core85_gui
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            USES_TERMINAL
            COMMENT "Building and running core85_gui"
        )
    else()
        message(WARNING "Qt6 Widgets not found; skipping core85_gui target.")
    endif()
endif()

if(CORE85_BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(core85_tests
        tests/test_arithmetic.cpp
        tests/test_assembler.cpp
        tests/test_branch.cpp
        tests/test_cpu.cpp
        tests/test_data_transfer.cpp
        tests/test_io.cpp
        tests/test_instruction_matrix.cpp
        tests/test_logical.cpp
        tests/test_memory.cpp
        tests/test_stack.cpp
        tests/test_interrupts.cpp
    )

    target_link_libraries(core85_tests
        PRIVATE
            core85_lib
            GTest::gtest_main
    )

    if(MSVC)
        target_compile_options(core85_tests PRIVATE /W4)
    else()
        target_compile_options(core85_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    include(GoogleTest)
    gtest_discover_tests(core85_tests)

    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS core85_tests
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        USES_TERMINAL
        COMMENT "Building and running the Core85 test suite"
    )
endif()
