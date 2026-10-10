add_library(StudioHubMetrology STATIC
    calibration.cpp
    uncertainty.cpp
    reference_signal.cpp
    measurement_result.cpp
    chain.cpp
    comparison.cpp
)

target_include_directories(StudioHubMetrology
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(StudioHubMetrology PUBLIC cxx_std_20)

add_library(StudioHub::Metrology ALIAS StudioHubMetrology)
