cmake_minimum_required(VERSION 3.28)

project(StudioHub
    VERSION 1.0.3
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(STUDIOHUB_BUILD_TESTS "Build StudioHub tests" ON)
option(STUDIOHUB_BUILD_TOOLS "Build StudioHub analysis tools" ON)
option(STUDIOHUB_BUILD_FET_TESTS "Build standalone StudioComp FET DSP tests" ON)
option(STUDIOHUB_ENABLE_VSTGUI "Build the StudioComp FET custom VSTGUI editor" OFF)
option(STUDIOHUB_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(STUDIOHUB_WARNINGS_AS_ERRORS "Treat StudioHub warnings as errors" ON)

include(GNUInstallDirs)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/StudioHubOptions.cmake)
include(CMakePackageConfigHelpers)

function(studiohub_apply_warnings target)
    if(STUDIOHUB_ENABLE_WARNINGS)
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnull-dereference
            -Wdouble-promotion
            -Wformat=2
        )

        if(STUDIOHUB_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(STUDIOHUB_BUILD_FET_TESTS)
    enable_testing()
endif()

if(STUDIOHUB_ENABLE_VST3)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
    # Enable VSTGUI only for explicit custom-editor builds. StudioHubGain's
    # established default build keeps the SDK GUI dependency disabled.
    set(SMTG_ENABLE_VSTGUI_SUPPORT ${STUDIOHUB_ENABLE_VSTGUI} CACHE BOOL "" FORCE)

    # Steinberg SDK is third-party code. Do not apply StudioHub's
    # project-wide -Werror / strict conversion policy to it.
    set(SMTG_ADD_VST3_SDK_SOURCE ON CACHE BOOL "" FORCE)

    # Steinberg SDK 3.8.1 can generate an invalid Linux/Fedora
    # automatic validator command with an empty plugin path.
    # We therefore disable its automatic validator hook and run the
    # validator ourselves against the exact generated .vst3 package.
    set(SMTG_RUN_VST_VALIDATOR OFF CACHE BOOL "" FORCE)

    add_subdirectory(third_party/vst3sdk EXCLUDE_FROM_ALL)

endif()

add_subdirectory(src)

if(STUDIOHUB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(STUDIOHUB_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

install(TARGETS StudioHubDSP StudioHubMetrology
    EXPORT StudioHubTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(EXPORT StudioHubTargets
    NAMESPACE StudioHub::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StudioHub
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/StudioHubConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/StudioHubConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StudioHub
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/StudioHubConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/StudioHubConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/StudioHubConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/StudioHub
)
install(DIRECTORY src/metrology/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
