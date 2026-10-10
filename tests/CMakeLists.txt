find_package(GTest REQUIRED)

add_executable(StudioHubTests
    dsp/test_dsp_core.cpp
    dsp/test_dsp_math.cpp
    dsp/test_parameter_smoother.cpp
    dsp/test_core_components.cpp
    analysis/test_basic_analyzers.cpp
    analysis/test_signal_generator.cpp
    analysis/test_sweep_generator.cpp
    analysis/test_signals.cpp
    analysis/test_level_analyzer.cpp
    metrology/test_metrology.cpp
)

target_link_libraries(StudioHubTests
    PRIVATE
        StudioHubDSP
        StudioHubMetrology
        GTest::gtest_main
)

target_compile_features(StudioHubTests PRIVATE cxx_std_20)

include(GoogleTest)
gtest_discover_tests(StudioHubTests)
