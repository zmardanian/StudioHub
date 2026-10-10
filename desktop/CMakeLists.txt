cmake_minimum_required(VERSION 3.28)
project(StudioHubDesktop VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Concurrent)

qt_add_executable(StudioHubDesktop
    src/main.cpp
    src/projectcontroller.cpp
    src/projectcontroller.hpp
    src/codexframes.cpp
    src/toolchainprofile.cpp
    src/activitymodel.cpp
    src/activitymodel.hpp
    src/projectinspection.cpp
    src/provenance.cpp
    src/projectmanifest.cpp
    src/projectmanifest.hpp
    src/projectscaffold.cpp
    src/projectscaffold.hpp
)
qt_add_resources(StudioHubDesktop "desktop_qml"
    PREFIX "/"
    FILES qml/Main.qml
)
target_link_libraries(StudioHubDesktop PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Concurrent)
target_compile_definitions(StudioHubDesktop PRIVATE
    STUDIOHUB_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.."
    STUDIOHUB_DESKTOP_VERSION="${PROJECT_VERSION}"
)

enable_testing()
add_executable(ProjectManifestTests tests/projectmanifest_tests.cpp src/provenance.cpp
    src/projectmanifest.cpp)
target_include_directories(ProjectManifestTests PRIVATE src)
target_link_libraries(ProjectManifestTests PRIVATE Qt6::Core)
add_test(NAME ProjectManifestTests COMMAND ProjectManifestTests "${CMAKE_CURRENT_SOURCE_DIR}/../studiohub-project.json")
add_executable(ControllerTests tests/controller_tests.cpp src/projectcontroller.cpp src/projectcontroller.hpp
    src/codexframes.cpp
    src/toolchainprofile.cpp
    src/activitymodel.cpp
    src/activitymodel.hpp
    src/projectinspection.cpp
    src/provenance.cpp
    src/projectmanifest.cpp src/projectscaffold.cpp)
target_include_directories(ControllerTests PRIVATE src)
target_link_libraries(ControllerTests PRIVATE Qt6::Core Qt6::Gui Qt6::Concurrent)
target_compile_definitions(ControllerTests PRIVATE STUDIOHUB_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/.." STUDIOHUB_DESKTOP_VERSION="${PROJECT_VERSION}")
add_test(NAME ControllerTests COMMAND ControllerTests "${CMAKE_CURRENT_SOURCE_DIR}/tests/appserver_fixture.py")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME IsolatedValidatorTests COMMAND ${Python3_EXECUTABLE}
 "${CMAKE_CURRENT_SOURCE_DIR}/tests/isolation_tests.py" "${CMAKE_CURRENT_SOURCE_DIR}/run-isolated-validator.py")

add_executable(ProvenanceTests tests/provenance_tests.cpp src/provenance.cpp)
target_include_directories(ProvenanceTests PRIVATE src)
target_link_libraries(ProvenanceTests PRIVATE Qt6::Core)
add_test(NAME ProvenanceTests COMMAND ProvenanceTests)

add_executable(ActivityTests tests/activity_tests.cpp src/activitymodel.cpp src/activitymodel.hpp)
target_include_directories(ActivityTests PRIVATE src)
target_link_libraries(ActivityTests PRIVATE Qt6::Core)
add_test(NAME ActivityTests COMMAND ActivityTests)

add_executable(ToolchainTests tests/toolchain_tests.cpp src/toolchainprofile.cpp)
target_include_directories(ToolchainTests PRIVATE src)
target_link_libraries(ToolchainTests PRIVATE Qt6::Core)
add_test(NAME ToolchainTests COMMAND ToolchainTests "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_test(NAME QmlSmokeTest COMMAND StudioHubDesktop --smoke-test)
set_tests_properties(QmlSmokeTest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QSG_RHI_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/smoke-config;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/smoke-cache")

add_executable(CodexFrameTests tests/codexframe_tests.cpp src/codexframes.cpp)
target_include_directories(CodexFrameTests PRIVATE src)
target_link_libraries(CodexFrameTests PRIVATE Qt6::Core)
add_test(NAME CodexFrameTests COMMAND CodexFrameTests)
