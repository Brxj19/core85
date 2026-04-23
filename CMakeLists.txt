cmake_minimum_required(VERSION 3.25)

project(Core85 VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CORE85_BUILD_GUI "Build the Qt GUI application" ON)
option(CORE85_BUILD_TESTS "Build the GTest test suite" ON)

if(APPLE AND "${CMAKE_OSX_DEPLOYMENT_TARGET}" STREQUAL "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING
        "Minimum macOS version supported by Core85" FORCE)
endif()

set(CORE85_CORE_HEADERS
    src/core/assembler.h
    src/core/cpu.h
    src/core/io_bus.h
    src/core/memory.h
    src/core/span.h
)

set(CORE85_DOC_FILES
    README.md
    runbook.md
    NOTICES.md
    RELEASE_NOTES_v1.0.md
)

add_library(core85_lib STATIC
    src/core/assembler.cpp
    src/core/cpu.cpp
    src/core/io_bus.cpp
    src/core/memory.cpp
)

target_include_directories(core85_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(MSVC)
    target_compile_options(core85_lib PRIVATE /W4)
else()
    target_compile_options(core85_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS core85_lib
    EXPORT Core85Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES ${CORE85_CORE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/core85/core
)

set(CORE85_DEPLOY_SCRIPT "")

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
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_BUNDLE_NAME "Core85"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.core85.app"
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
            WIN32_EXECUTABLE ON
        )

        target_link_libraries(core85_gui
            PRIVATE
                Qt6::Widgets
                core85_lib
        )

        install(TARGETS core85_gui
            BUNDLE DESTINATION .
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )

        qt_generate_deploy_app_script(
            TARGET core85_gui
            OUTPUT_SCRIPT CORE85_DEPLOY_SCRIPT
            NO_UNSUPPORTED_PLATFORM_ERROR
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
        tests/test_interrupts.cpp
        tests/test_logical.cpp
        tests/test_memory.cpp
        tests/test_stack.cpp
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

install(FILES ${CORE85_DOC_FILES}
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
    FILES_MATCHING
    PATTERN "*.docx"
)

install(DIRECTORY tests/asm/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/core85/examples
    FILES_MATCHING
    PATTERN "*.asm"
)

install(EXPORT Core85Targets
    FILE Core85Targets.cmake
    NAMESPACE Core85::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Core85
)

if(CORE85_DEPLOY_SCRIPT)
    install(SCRIPT ${CORE85_DEPLOY_SCRIPT})
endif()

set(CPACK_PACKAGE_NAME "Core85")
set(CPACK_PACKAGE_VENDOR "Core85 Engineering Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Cross-platform Intel 8085 emulator and educational simulator")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Core85")
set(CPACK_PACKAGE_FILE_NAME
    "Core85-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_MONOLITHIC_INSTALL ON)
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/NOTICES.md")

if(APPLE)
    set(CPACK_GENERATOR "ZIP")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
set(CPACK_SOURCE_IGNORE_FILES
    "/build/"
    "/build-gui/"
    "/\\.git/"
    "/\\.idea/"
    "/\\.vscode/"
    "\\.DS_Store"
)

include(CPack)
