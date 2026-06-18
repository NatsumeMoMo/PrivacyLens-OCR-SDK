cmake_minimum_required(VERSION 3.28)

project(PrivacyLensEngineSdk
    VERSION 0.1.0
    DESCRIPTION "PrivacyLens redaction engine SDK boundary"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(PrivacyLensEngine SHARED)
target_sources(PrivacyLensEngine
    PRIVATE
        src/c_api/pl_engine_api.cpp
        src/core/engine_session.cpp
        src/providers/app_window/app_window_provider.cpp
    PUBLIC
        FILE_SET public_engine_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES include/pl_engine.h)
target_include_directories(PrivacyLensEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(PrivacyLensEngine PRIVATE PL_ENGINE_BUILD_DLL NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(PrivacyLensEngine PRIVATE user32 dwmapi)
set_target_properties(PrivacyLensEngine PROPERTIES OUTPUT_NAME "PrivacyLensEngine")

add_executable(pl-engine-cli tools/pl-engine-cli/main.cpp)
target_link_libraries(pl-engine-cli PRIVATE PrivacyLensEngine user32)

add_executable(pl-engine-visible-regions-tests
    tests/visible_regions_tests.cpp)
target_include_directories(pl-engine-visible-regions-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(pl-engine-policy-tests
    tests/engine_policy_tests.cpp)
target_link_libraries(pl-engine-policy-tests PRIVATE PrivacyLensEngine)
target_include_directories(pl-engine-policy-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(pl-engine-app-window-provider-tests
    tests/app_window_provider_tests.cpp
    src/providers/app_window/app_window_provider.cpp)
target_include_directories(pl-engine-app-window-provider-tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(pl-engine-app-window-provider-tests PRIVATE user32 dwmapi)

if(MSVC)
    foreach(target_name IN ITEMS
        PrivacyLensEngine
        pl-engine-cli
        pl-engine-visible-regions-tests
        pl-engine-policy-tests
        pl-engine-app-window-provider-tests)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /MP)
    endforeach()
endif()

enable_testing()
add_test(NAME pl_engine_cli_self_test COMMAND pl-engine-cli --self-test --mask-wechat)
add_test(NAME pl_engine_visible_regions_tests COMMAND pl-engine-visible-regions-tests)
add_test(NAME pl_engine_policy_tests COMMAND pl-engine-policy-tests)
add_test(NAME pl_engine_app_window_provider_tests COMMAND pl-engine-app-window-provider-tests)
