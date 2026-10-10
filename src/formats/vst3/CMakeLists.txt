set(STUDIOHUB_VST3_SOURCES
    studiohub_vst3_processor.cpp
    studiohub_vst3_processor.hpp
    studiohub_vst3_controller.cpp
    studiohub_vst3_controller.hpp
    studiohub_vst3_entry.cpp
    studiohub_vst3_ids.hpp
)

smtg_add_vst3plugin(StudioHubGain ${STUDIOHUB_VST3_SOURCES})
smtg_target_configure_version_file(StudioHubGain)

target_compile_features(StudioHubGain PRIVATE cxx_std_20)

target_include_directories(StudioHubGain
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(StudioHubGain
    PRIVATE
        sdk
        StudioHubDSP
)

set_target_properties(StudioHubGain PROPERTIES
    OUTPUT_NAME "StudioHubGain"
)

if(UNIX AND NOT APPLE)
    set(STUDIOHUB_VST3_EXPORTS
        "${CMAKE_CURRENT_SOURCE_DIR}/vst3.exports")
    target_link_options(StudioHubGain PRIVATE
        "LINKER:--version-script=${STUDIOHUB_VST3_EXPORTS}"
    )
endif()

if(STUDIOHUB_ENABLE_VSTGUI)
set(STUDIOCOMP_FET_SOURCES
    studiocomp_fet_processor.cpp
    studiocomp_fet_processor.hpp
    studiocomp_fet_controller.cpp
    studiocomp_fet_controller.hpp
    studiocomp_fet_entry.cpp
    studiocomp_fet_ids.hpp
    resource/studiocomp_fet.uidesc
    resource/meter_on.png
    resource/meter_off.png
)

smtg_add_vst3plugin(StudioCompFET ${STUDIOCOMP_FET_SOURCES})
smtg_target_configure_version_file(StudioCompFET)
if(UNIX AND NOT APPLE)
    # The SDK helper's main-source function loses public_sdk_SOURCE_DIR across
    # the SDK subdirectory scope; VSTGUI's module initializer needs this handle.
    target_sources(StudioCompFET PRIVATE
        ${PROJECT_SOURCE_DIR}/third_party/vst3sdk/public.sdk/source/main/linuxmain.cpp
    )
endif()
target_compile_features(StudioCompFET PRIVATE cxx_std_20)
target_include_directories(StudioCompFET PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(StudioCompFET PRIVATE StudioHubFETDSP vstgui_support sdk)
smtg_target_add_plugin_resources(StudioCompFET
    RESOURCES
        resource/studiocomp_fet.uidesc
        resource/meter_on.png
        resource/meter_off.png
)
set_target_properties(StudioCompFET PROPERTIES OUTPUT_NAME "StudioCompFET")

if(UNIX AND NOT APPLE)
    target_link_options(StudioCompFET PRIVATE
        "LINKER:--version-script=${STUDIOHUB_VST3_EXPORTS}"
    )
endif()

add_custom_command(
    TARGET StudioCompFET POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "[StudioHub] Validating StudioCompFET.vst3"
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/desktop/run-isolated-validator.py" --kind steinberg --tool $<TARGET_FILE:validator> --bundle "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioCompFET.vst3" --evidence "${CMAKE_BINARY_DIR}/studiohub-validation/steinberg"
    COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/scripts/normalize_moduleinfo.py"
        "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioCompFET.vst3/Contents/Resources/moduleinfo.json"
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/desktop/run-isolated-validator.py" --kind metadata --tool $<TARGET_FILE:moduleinfotool> --bundle "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioCompFET.vst3" --evidence "${CMAKE_BINARY_DIR}/studiohub-validation/metadata"
    COMMAND ${CMAKE_COMMAND} -E echo "[StudioHub] StudioCompFET validation passed"
    VERBATIM
)
else()
    message(STATUS "StudioCompFET target disabled; configure with STUDIOHUB_ENABLE_VSTGUI=ON")
endif()

# The SDK 3.8.1 Linux validator helper can lose the package path.
# Run the validator explicitly against the generated VST3 bundle.
add_custom_command(
    TARGET StudioHubGain
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo
        "[StudioHub] Validating StudioHubGain.vst3"
    COMMAND $<TARGET_FILE:validator>
        "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioHubGain.vst3"
    COMMAND ${CMAKE_COMMAND} -E echo
        "[StudioHub] VST3 validation passed"
    COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/scripts/normalize_moduleinfo.py"
        "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioHubGain.vst3/Contents/Resources/moduleinfo.json"
    COMMAND $<TARGET_FILE:moduleinfotool>
        -validate
        -path "${CMAKE_BINARY_DIR}/VST3/$<CONFIG>/StudioHubGain.vst3"
    COMMAND ${CMAKE_COMMAND} -E echo
        "[StudioHub] moduleinfo validation passed"
    VERBATIM
)
