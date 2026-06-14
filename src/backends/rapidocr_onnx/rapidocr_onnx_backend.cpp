#include "backends/rapidocr_onnx/rapidocr_onnx_backend.hpp"

#include "core/ocr_error.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace plocr {
namespace {

using OcrHandle = void*;
using OcrBool = char;

struct RapidParam {
    int padding;
    int maxSideLen;
    float boxScoreThresh;
    float boxThresh;
    float unClipRatio;
    int doAngle;
    int mostAngle;
};

struct RapidPoint {
    double x;
    double y;
};

struct RapidInput {
    std::uint8_t* data;
    int type;
    int channels;
    int width;
    int height;
    long dataLength;
};

struct RapidTextBlock {
    RapidPoint* boxPoint;
    float boxScore;
    int angleIndex;
    float angleScore;
    double angleTime;
    std::uint8_t* text;
    float* charScores;
    unsigned long long charScoresLength;
    unsigned long long boxPointLength;
    unsigned long long textLength;
    double crnnTime;
    double blockTime;
};

struct RapidResult {
    double dbNetTime;
    RapidTextBlock* textBlocks;
    unsigned long long textBlocksLength;
    double detectTime;
};

using OcrInitFn = OcrHandle(__cdecl*)(const char*, const char*, const char*, const char*, int);
using OcrDetectInputFn = OcrBool(__cdecl*)(OcrHandle, RapidInput*, RapidParam*, RapidResult*);
using OcrFreeResultFn = OcrBool(__cdecl*)(RapidResult*);
using OcrDestroyFn = void(__cdecl*)(OcrHandle);

constexpr const char* BackendName = "rapidocr_onnx";
constexpr const char* ModelVersion = "PP-OCRv3-det-rec + PP-OCRv1-cls";
constexpr const char* RuntimeVersion = "RapidOcrOnnx 1.2.2 / ONNX Runtime 1.14.0";
constexpr const char* ExecutionProvider = "cpu";
constexpr const char* DetModelName = "ch_PP-OCRv3_det_infer.onnx";
constexpr const char* ClsModelName = "ch_ppocr_mobile_v2.0_cls_infer.onnx";
constexpr const char* RecModelName = "ch_PP-OCRv3_rec_infer.onnx";
constexpr const char* KeysName = "ppocr_keys_v1.txt";

std::filesystem::path default_rapidocr_dll()
{
    return std::filesystem::path(
        LR"(D:\Atlas\Artifacts\ThirdParty\PrivacyLens-OCR-SDK\RapidOcrOnnx-1.2.2\windows-clib\windows-clib\win-CLIB-CPU-x64\bin\RapidOcrOnnx.dll)");
}

std::string to_utf8(const std::filesystem::path& path)
{
    const auto wide = path.wstring();
    if (wide.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        throw OcrError(ErrorCode::invalid_argument, "path cannot be converted to UTF-8");
    }

    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring utf8_to_wide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        throw OcrError(ErrorCode::invalid_argument, "path is not valid UTF-8");
    }

    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, result.data(), size);
    return result;
}

std::filesystem::path path_from_utf8(const std::string& value)
{
    return std::filesystem::path(utf8_to_wide(value));
}

std::wstring get_env_wide(const wchar_t* name)
{
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) {
        return {};
    }
    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, value.data(), required);
    if (written == 0 || written >= required) {
        return {};
    }
    value.resize(written);
    return value;
}

void add_dll_candidates(std::vector<std::filesystem::path>* candidates, const std::filesystem::path& root)
{
    if (root.empty()) {
        return;
    }
    if (root.filename() == L"RapidOcrOnnx.dll") {
        candidates->push_back(root);
        return;
    }
    candidates->push_back(root / L"RapidOcrOnnx.dll");
    candidates->push_back(root / L"bin" / L"RapidOcrOnnx.dll");
}

std::filesystem::path find_rapidocr_dll(const std::filesystem::path& model_dir)
{
    std::vector<std::filesystem::path> candidates;
    const auto env_value = get_env_wide(L"PRIVACYLENS_OCR_RAPIDOCRONNX_DIR");
    if (!env_value.empty()) {
        add_dll_candidates(&candidates, env_value);
    }
    add_dll_candidates(&candidates, model_dir);
    candidates.push_back(default_rapidocr_dll());

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    throw OcrError(ErrorCode::backend_unavailable,
                   "RapidOcrOnnx.dll not found; set PRIVACYLENS_OCR_RAPIDOCRONNX_DIR or restore the runtime artifact");
}

void ensure_file_exists(const std::filesystem::path& path, const char* label)
{
    if (!std::filesystem::exists(path)) {
        throw OcrError(ErrorCode::model_not_configured,
                       std::string(label) + " missing from model_dir: " + to_utf8(path));
    }
}

RapidParam default_param()
{
    RapidParam param{};
    param.padding = 50;
    param.maxSideLen = 1024;
    param.boxScoreThresh = 0.6F;
    param.boxThresh = 0.3F;
    param.unClipRatio = 2.0F;
    param.doAngle = 1;
    param.mostAngle = 1;
    return param;
}

std::vector<std::uint8_t> bgra_to_rgba_contiguous(const ImageView& image)
{
    std::vector<std::uint8_t> output(static_cast<std::size_t>(image.width) *
                                     static_cast<std::size_t>(image.height) * 4U);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        const auto* source_row = image.data + static_cast<std::size_t>(y) * image.stride_bytes;
        auto* target_row = output.data() + static_cast<std::size_t>(y) * image.width * 4U;
        for (std::uint32_t x = 0; x < image.width; ++x) {
            const auto* source = source_row + static_cast<std::size_t>(x) * 4U;
            auto* target = target_row + static_cast<std::size_t>(x) * 4U;
            target[0] = source[2];
            target[1] = source[1];
            target[2] = source[0];
            target[3] = source[3];
        }
    }
    return output;
}

Rect bbox_from_points(const RapidTextBlock& block)
{
    if (block.boxPoint == nullptr || block.boxPointLength == 0ULL) {
        return {};
    }

    double min_x = block.boxPoint[0].x;
    double max_x = block.boxPoint[0].x;
    double min_y = block.boxPoint[0].y;
    double max_y = block.boxPoint[0].y;
    for (unsigned long long index = 1; index < block.boxPointLength; ++index) {
        min_x = std::min(min_x, block.boxPoint[index].x);
        max_x = std::max(max_x, block.boxPoint[index].x);
        min_y = std::min(min_y, block.boxPoint[index].y);
        max_y = std::max(max_y, block.boxPoint[index].y);
    }

    return Rect{static_cast<float>(min_x),
                static_cast<float>(min_y),
                static_cast<float>(std::max(0.0, max_x - min_x)),
                static_cast<float>(std::max(0.0, max_y - min_y))};
}

Quad quad_from_points(const RapidTextBlock& block)
{
    Quad quad;
    if (block.boxPoint == nullptr || block.boxPointLength < 4ULL) {
        return quad;
    }

    for (std::size_t index = 0; index < 4U; ++index) {
        quad.x[index] = static_cast<float>(block.boxPoint[index].x);
        quad.y[index] = static_cast<float>(block.boxPoint[index].y);
    }
    quad.has_quad = true;
    return quad;
}

float confidence_from_block(const RapidTextBlock& block)
{
    float char_confidence = block.boxScore;
    if (block.charScores != nullptr && block.charScoresLength > 0ULL) {
        const auto* begin = block.charScores;
        const auto* end = block.charScores + block.charScoresLength;
        char_confidence = std::accumulate(begin, end, 0.0F) / static_cast<float>(block.charScoresLength);
    }

    return std::clamp((block.boxScore + char_confidence) * 0.5F, 0.0F, 1.0F);
}

std::string text_from_block(const RapidTextBlock& block)
{
    if (block.text == nullptr || block.textLength == 0ULL) {
        return {};
    }

    const auto* begin = reinterpret_cast<const char*>(block.text);
    std::size_t length = static_cast<std::size_t>(block.textLength);
    while (length > 0U && begin[length - 1U] == '\0') {
        --length;
    }
    return std::string(begin, length);
}

double elapsed_ms(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

class RapidOcrOnnxBackend::Impl {
public:
    explicit Impl(std::string model_dir_utf8)
    {
        if (model_dir_utf8.empty()) {
            throw OcrError(ErrorCode::model_not_configured, "rapidocr_onnx requires model_dir_utf8");
        }

        model_dir_ = path_from_utf8(model_dir_utf8);
        det_model_ = model_dir_ / DetModelName;
        cls_model_ = model_dir_ / ClsModelName;
        rec_model_ = model_dir_ / RecModelName;
        keys_ = model_dir_ / KeysName;
        ensure_file_exists(det_model_, DetModelName);
        ensure_file_exists(cls_model_, ClsModelName);
        ensure_file_exists(rec_model_, RecModelName);
        ensure_file_exists(keys_, KeysName);

        dll_path_ = find_rapidocr_dll(model_dir_);
        module_ = LoadLibraryW(dll_path_.c_str());
        if (module_ == nullptr) {
            throw OcrError(ErrorCode::backend_unavailable, "LoadLibraryW failed for RapidOcrOnnx.dll");
        }

        init_ = reinterpret_cast<OcrInitFn>(GetProcAddress(module_, "OcrInit"));
        detect_input_ = reinterpret_cast<OcrDetectInputFn>(GetProcAddress(module_, "OcrDetectInput"));
        free_result_ = reinterpret_cast<OcrFreeResultFn>(GetProcAddress(module_, "OcrFreeResult"));
        destroy_ = reinterpret_cast<OcrDestroyFn>(GetProcAddress(module_, "OcrDestroy"));
        if (init_ == nullptr || detect_input_ == nullptr || free_result_ == nullptr || destroy_ == nullptr) {
            FreeLibrary(module_);
            module_ = nullptr;
            throw OcrError(ErrorCode::backend_unavailable, "RapidOcrOnnx.dll is missing required C API exports");
        }

        const auto det = to_utf8(det_model_);
        const auto cls = to_utf8(cls_model_);
        const auto rec = to_utf8(rec_model_);
        const auto keys = to_utf8(keys_);
        handle_ = init_(det.c_str(), cls.c_str(), rec.c_str(), keys.c_str(), 2);
        if (handle_ == nullptr) {
            FreeLibrary(module_);
            module_ = nullptr;
            throw OcrError(ErrorCode::backend_unavailable, "OcrInit failed");
        }
    }

    ~Impl()
    {
        if (handle_ != nullptr && destroy_ != nullptr) {
            destroy_(handle_);
            handle_ = nullptr;
        }
        if (module_ != nullptr) {
            FreeLibrary(module_);
            module_ = nullptr;
        }
    }

    [[nodiscard]] BackendInfo backend_info() const
    {
        BackendInfo info;
        info.name = BackendName;
        info.model_version = ModelVersion;
        info.runtime_version = RuntimeVersion;
        info.execution_provider = ExecutionProvider;
        info.is_configured = handle_ != nullptr;
        return info;
    }

    [[nodiscard]] BackendCapabilities capabilities() const
    {
        BackendCapabilities capabilities;
        capabilities.accepts_memory_input = true;
        capabilities.returns_source_space_bbox = true;
        capabilities.returns_source_space_quad = true;
        capabilities.returns_confidence = true;
        capabilities.supports_orientation = true;
        capabilities.supports_line_boxes = true;
        return capabilities;
    }

    [[nodiscard]] OcrResult recognize(const OcrRequest& request) const
    {
        if (handle_ == nullptr) {
            throw OcrError(ErrorCode::backend_unavailable, "rapidocr_onnx backend is not initialized");
        }

        const auto& image = request.image;
        const auto total_start = std::chrono::steady_clock::now();
        const auto preprocess_start = std::chrono::steady_clock::now();
        auto rgba = bgra_to_rgba_contiguous(image);
        const auto preprocess_end = std::chrono::steady_clock::now();

        RapidInput input{};
        input.data = rgba.data();
        input.type = 0;
        input.channels = 4;
        input.width = static_cast<int>(image.width);
        input.height = static_cast<int>(image.height);
        input.dataLength = static_cast<long>(rgba.size());

        RapidParam param = default_param();
        RapidResult rapid_result{};

        const auto inference_start = std::chrono::steady_clock::now();
        const OcrBool ok = detect_input_(handle_, &input, &param, &rapid_result);
        const auto inference_end = std::chrono::steady_clock::now();

        OcrResult output;
        output.backend = backend_info();
        output.latency.preprocess_ms = elapsed_ms(preprocess_start, preprocess_end);
        output.latency.inference_ms = rapid_result.dbNetTime;
        output.latency.postprocess_ms = 0.0;
        output.latency.total_ms = elapsed_ms(total_start, inference_end);

        if (ok == 0) {
            output.latency.inference_ms = elapsed_ms(inference_start, inference_end);
            free_result_(&rapid_result);
            return output;
        }

        output.boxes.reserve(static_cast<std::size_t>(rapid_result.textBlocksLength));
        double recognition_time = 0.0;
        for (unsigned long long index = 0; index < rapid_result.textBlocksLength; ++index) {
            const auto& block = rapid_result.textBlocks[index];
            recognition_time += block.angleTime + block.crnnTime;

            OcrBox box;
            box.text_utf8 = text_from_block(block);
            box.confidence = confidence_from_block(block);
            box.bbox = bbox_from_points(block);
            box.quad = quad_from_points(block);
            output.boxes.push_back(std::move(box));
        }

        output.latency.inference_ms = rapid_result.dbNetTime + recognition_time;
        output.latency.postprocess_ms = std::max(0.0, rapid_result.detectTime - output.latency.inference_ms);
        output.latency.total_ms = std::max(output.latency.total_ms,
                                           output.latency.preprocess_ms + rapid_result.detectTime);
        free_result_(&rapid_result);
        return output;
    }

private:
    std::filesystem::path model_dir_;
    std::filesystem::path det_model_;
    std::filesystem::path cls_model_;
    std::filesystem::path rec_model_;
    std::filesystem::path keys_;
    std::filesystem::path dll_path_;
    HMODULE module_ = nullptr;
    OcrHandle handle_ = nullptr;
    OcrInitFn init_ = nullptr;
    OcrDetectInputFn detect_input_ = nullptr;
    OcrFreeResultFn free_result_ = nullptr;
    OcrDestroyFn destroy_ = nullptr;
};

RapidOcrOnnxBackend::RapidOcrOnnxBackend(std::string model_dir)
    : impl_(std::make_unique<Impl>(std::move(model_dir)))
{
}

RapidOcrOnnxBackend::~RapidOcrOnnxBackend() = default;

BackendInfo RapidOcrOnnxBackend::backend_info() const
{
    return impl_->backend_info();
}

BackendCapabilities RapidOcrOnnxBackend::capabilities() const
{
    return impl_->capabilities();
}

OcrResult RapidOcrOnnxBackend::recognize(const OcrRequest& request) const
{
    return impl_->recognize(request);
}

}  // namespace plocr
