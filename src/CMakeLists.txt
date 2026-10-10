add_library(StudioHubDSP STATIC
    dsp/core/dsp_core.cpp
    dsp/core/parameter_smoother.cpp
    analysis/analyzers.cpp
    analysis/signal_generator.cpp
    analysis/sweep_generator.cpp
    analysis/test_signals.cpp
    analysis/level_analyzer.cpp
)

add_library(StudioHubFETDSP STATIC dsp/fet/studio_comp_fet.cpp)
target_include_directories(StudioHubFETDSP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(StudioHubFETDSP PUBLIC cxx_std_20)
studiohub_apply_warnings(StudioHubFETDSP)

if(STUDIOHUB_BUILD_FET_TESTS)
    add_executable(StudioCompFETTests ../tests/fet/test_studiocomp_fet.cpp)
    target_link_libraries(StudioCompFETTests PRIVATE StudioHubFETDSP)
    target_compile_features(StudioCompFETTests PRIVATE cxx_std_20)
    studiohub_apply_warnings(StudioCompFETTests)
    # The test intentionally pairs custom global new/delete with malloc/free
    # to count heap activity inside the callback; GCC's heuristic flags it.
    target_compile_options(StudioCompFETTests PRIVATE -Wno-mismatched-new-delete)
    add_test(NAME StudioCompFETTests COMMAND StudioCompFETTests)
endif()

target_include_directories(StudioHubDSP
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/studiohub>
)

target_compile_features(StudioHubDSP PUBLIC cxx_std_20)

add_subdirectory(metrology)

add_library(StudioHub::DSP ALIAS StudioHubDSP)

install(DIRECTORY dsp/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/studiohub/dsp FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(DIRECTORY analysis/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/studiohub/analysis FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(DIRECTORY engine/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/studiohub/engine FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

if(STUDIOHUB_ENABLE_VST3)
    add_subdirectory(formats/vst3)
endif()
