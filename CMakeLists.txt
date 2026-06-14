cmake_minimum_required(VERSION 3.28)

project(PrivacyLensOcrSdk
    VERSION 0.1.0
    DESCRIPTION "PrivacyLens OCR-only SDK boundary spike"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(PrivacyLensOcr SHARED)
target_sources(PrivacyLensOcr
    PRIVATE
        src/c_api/pl_ocr_api.cpp
        src/core/ocr_engine.cpp
        src/backends/stub/stub_ocr_backend.cpp
        src/backends/rapidocr_onnx/rapidocr_onnx_backend.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES include/pl_ocr.h)
target_include_directories(PrivacyLensOcr
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(PrivacyLensOcr PRIVATE PL_OCR_BUILD_DLL NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(PrivacyLensOcr PROPERTIES OUTPUT_NAME "PrivacyLensOcr")

add_executable(pl-ocr-cli tools/pl-ocr-cli/main.cpp)
target_link_libraries(pl-ocr-cli PRIVATE PrivacyLensOcr)
if(WIN32)
    target_link_libraries(pl-ocr-cli PRIVATE ole32 windowscodecs gdi32)
endif()

add_custom_command(TARGET pl-ocr-cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:PrivacyLensOcr>
        $<TARGET_FILE_DIR:pl-ocr-cli>)

if(MSVC)
    foreach(target_name IN ITEMS PrivacyLensOcr pl-ocr-cli)
        target_compile_options(${target_name} PRIVATE /W4 /permissive- /EHsc /Zc:__cplusplus /MP)
    endforeach()
endif()

enable_testing()
add_test(NAME pl_ocr_cli_self_test COMMAND pl-ocr-cli --self-test)
add_test(NAME pl_ocr_cli_stub_self_test COMMAND pl-ocr-cli --backend stub --self-test)
add_test(NAME pl_ocr_cli_rapidocr_unconfigured
    COMMAND pl-ocr-cli --backend rapidocr_onnx --model-dir "${CMAKE_CURRENT_BINARY_DIR}/missing-models" --self-test --expect-status model-not-configured)
