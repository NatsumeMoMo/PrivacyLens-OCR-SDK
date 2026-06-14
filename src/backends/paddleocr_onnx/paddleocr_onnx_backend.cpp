#include "backends/paddleocr_onnx/paddleocr_onnx_backend.hpp"

#include "core/ocr_error.hpp"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
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

constexpr const char* BackendName = "paddleocr_onnx";
constexpr const char* ModelVersion = "PP-OCRv6-small-det-rec";
constexpr const char* RuntimeVersion = "ONNX Runtime";
constexpr const char* ExecutionProvider = "cpu";
constexpr const char* DetModelPath = "det/model.onnx";
constexpr const char* DetConfigPath = "det/inference.yml";
constexpr const char* RecModelPath = "rec/model.onnx";
constexpr const char* RecConfigPath = "rec/inference.yml";

constexpr float DetThresh = 0.2F;
constexpr float DetBoxThresh = 0.45F;
constexpr std::uint32_t DetLimitSideLen = 960U;
constexpr std::uint32_t RecTargetHeight = 48U;
constexpr std::uint32_t RecMaxWidth = 960U;

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

std::string trim_copy(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

std::string parse_yaml_scalar(std::string value)
{
    value = trim_copy(value);
    if (value == "''''") {
        return "'";
    }
    if (value.size() >= 2U && value.front() == '\'' && value.back() == '\'') {
        value = value.substr(1U, value.size() - 2U);
    } else if (value.size() >= 2U && value.front() == '"' && value.back() == '"') {
        value = value.substr(1U, value.size() - 2U);
    }
    return value;
}

void ensure_file_exists(const std::filesystem::path& path, const char* label)
{
    if (!std::filesystem::exists(path)) {
        throw OcrError(ErrorCode::model_not_configured,
                       std::string(label) + " missing from paddleocr_onnx model_dir");
    }
}

std::vector<std::string> load_recognition_characters(const std::filesystem::path& config_path)
{
    std::ifstream input(config_path);
    if (!input) {
        throw OcrError(ErrorCode::model_not_configured, "rec/inference.yml cannot be opened");
    }

    std::vector<std::string> characters;
    characters.emplace_back();
    bool in_dictionary = false;
    std::string line;
    while (std::getline(input, line)) {
        const auto trimmed = trim_copy(line);
        if (trimmed == "character_dict:") {
            in_dictionary = true;
            continue;
        }
        if (!in_dictionary) {
            continue;
        }
        if (trimmed.rfind("- ", 0) == 0) {
            characters.push_back(parse_yaml_scalar(trimmed.substr(2U)));
            continue;
        }
        if (!trimmed.empty() && trimmed.back() == ':') {
            break;
        }
    }

    if (characters.size() <= 1U) {
        throw OcrError(ErrorCode::model_not_configured, "rec/inference.yml does not contain character_dict");
    }
    return characters;
}

std::uint32_t round_to_multiple(std::uint32_t value, std::uint32_t multiple)
{
    return std::max(multiple, ((value + multiple / 2U) / multiple) * multiple);
}

struct ResizeInfo {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float scale_x_to_source = 1.0F;
    float scale_y_to_source = 1.0F;
};

ResizeInfo det_resize_info(const ImageView& image)
{
    const auto max_side = static_cast<float>(std::max(image.width, image.height));
    const float ratio = max_side > static_cast<float>(DetLimitSideLen) ? static_cast<float>(DetLimitSideLen) / max_side
                                                                       : 1.0F;
    const auto resized_width = round_to_multiple(static_cast<std::uint32_t>(
                                                     std::max(32.0F, std::round(static_cast<float>(image.width) * ratio))),
                                                 32U);
    const auto resized_height = round_to_multiple(static_cast<std::uint32_t>(
                                                      std::max(32.0F, std::round(static_cast<float>(image.height) * ratio))),
                                                  32U);
    return ResizeInfo{resized_width,
                      resized_height,
                      static_cast<float>(image.width) / static_cast<float>(resized_width),
                      static_cast<float>(image.height) / static_cast<float>(resized_height)};
}

std::array<float, 3> sample_bgr_bilinear(const ImageView& image, float source_x, float source_y)
{
    source_x = std::clamp(source_x, 0.0F, static_cast<float>(image.width - 1U));
    source_y = std::clamp(source_y, 0.0F, static_cast<float>(image.height - 1U));
    const auto x0 = static_cast<std::uint32_t>(std::floor(source_x));
    const auto y0 = static_cast<std::uint32_t>(std::floor(source_y));
    const auto x1 = std::min(x0 + 1U, image.width - 1U);
    const auto y1 = std::min(y0 + 1U, image.height - 1U);
    const float tx = source_x - static_cast<float>(x0);
    const float ty = source_y - static_cast<float>(y0);

    auto pixel = [&](std::uint32_t x, std::uint32_t y) -> const std::uint8_t* {
        return image.data + static_cast<std::size_t>(y) * image.stride_bytes + static_cast<std::size_t>(x) * 4U;
    };

    std::array<float, 3> output{};
    for (std::size_t channel = 0; channel < 3U; ++channel) {
        const float p00 = static_cast<float>(pixel(x0, y0)[channel]);
        const float p10 = static_cast<float>(pixel(x1, y0)[channel]);
        const float p01 = static_cast<float>(pixel(x0, y1)[channel]);
        const float p11 = static_cast<float>(pixel(x1, y1)[channel]);
        output[channel] = ((1.0F - tx) * (1.0F - ty) * p00) + (tx * (1.0F - ty) * p10) +
                          ((1.0F - tx) * ty * p01) + (tx * ty * p11);
    }
    return output;
}

std::vector<float> make_detector_input(const ImageView& image, const ResizeInfo& resize)
{
    std::vector<float> tensor(static_cast<std::size_t>(3U) * resize.width * resize.height);
    constexpr std::array<float, 3> mean{0.485F, 0.456F, 0.406F};
    constexpr std::array<float, 3> stddev{0.229F, 0.224F, 0.225F};

    for (std::uint32_t y = 0; y < resize.height; ++y) {
        const float source_y = (static_cast<float>(y) + 0.5F) * resize.scale_y_to_source - 0.5F;
        for (std::uint32_t x = 0; x < resize.width; ++x) {
            const float source_x = (static_cast<float>(x) + 0.5F) * resize.scale_x_to_source - 0.5F;
            const auto bgr = sample_bgr_bilinear(image, source_x, source_y);
            for (std::size_t channel = 0; channel < 3U; ++channel) {
                const auto offset = channel * static_cast<std::size_t>(resize.width) * resize.height +
                                    static_cast<std::size_t>(y) * resize.width + x;
                tensor[offset] = ((bgr[channel] / 255.0F) - mean[channel]) / stddev[channel];
            }
        }
    }
    return tensor;
}

struct DetectedRegion {
    Rect bbox;
    Quad quad;
    float score = 0.0F;
};

std::vector<DetectedRegion> extract_regions(const float* probability,
                                            std::uint32_t map_width,
                                            std::uint32_t map_height,
                                            const ResizeInfo& resize,
                                            const ImageView& image)
{
    std::vector<std::uint8_t> visited(static_cast<std::size_t>(map_width) * map_height, 0U);
    std::vector<DetectedRegion> regions;
    const auto index_of = [map_width](std::uint32_t x, std::uint32_t y) {
        return static_cast<std::size_t>(y) * map_width + x;
    };

    for (std::uint32_t y = 0; y < map_height; ++y) {
        for (std::uint32_t x = 0; x < map_width; ++x) {
            const auto start = index_of(x, y);
            if (visited[start] != 0U || probability[start] <= DetThresh) {
                continue;
            }

            std::queue<std::pair<std::uint32_t, std::uint32_t>> queue;
            queue.emplace(x, y);
            visited[start] = 1U;
            std::uint32_t min_x = x;
            std::uint32_t max_x = x;
            std::uint32_t min_y = y;
            std::uint32_t max_y = y;
            float score_sum = 0.0F;
            std::uint32_t count = 0;

            while (!queue.empty()) {
                const auto [cx, cy] = queue.front();
                queue.pop();
                const auto current_index = index_of(cx, cy);
                score_sum += probability[current_index];
                ++count;
                min_x = std::min(min_x, cx);
                max_x = std::max(max_x, cx);
                min_y = std::min(min_y, cy);
                max_y = std::max(max_y, cy);

                const std::array<std::pair<int, int>, 4> neighbors{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};
                for (const auto& [dx, dy] : neighbors) {
                    const auto nx = static_cast<int>(cx) + dx;
                    const auto ny = static_cast<int>(cy) + dy;
                    if (nx < 0 || ny < 0 || nx >= static_cast<int>(map_width) || ny >= static_cast<int>(map_height)) {
                        continue;
                    }
                    const auto next_index = index_of(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny));
                    if (visited[next_index] == 0U && probability[next_index] > DetThresh) {
                        visited[next_index] = 1U;
                        queue.emplace(static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny));
                    }
                }
            }

            if (count < 3U) {
                continue;
            }
            const float score = score_sum / static_cast<float>(count);
            if (score < DetBoxThresh) {
                continue;
            }

            const float map_scale_x = static_cast<float>(resize.width) / static_cast<float>(map_width);
            const float map_scale_y = static_cast<float>(resize.height) / static_cast<float>(map_height);
            float left = static_cast<float>(min_x) * map_scale_x * resize.scale_x_to_source;
            float top = static_cast<float>(min_y) * map_scale_y * resize.scale_y_to_source;
            float right = static_cast<float>(max_x + 1U) * map_scale_x * resize.scale_x_to_source;
            float bottom = static_cast<float>(max_y + 1U) * map_scale_y * resize.scale_y_to_source;

            left = std::clamp(left, 0.0F, static_cast<float>(image.width - 1U));
            top = std::clamp(top, 0.0F, static_cast<float>(image.height - 1U));
            right = std::clamp(right, left + 1.0F, static_cast<float>(image.width));
            bottom = std::clamp(bottom, top + 1.0F, static_cast<float>(image.height));

            DetectedRegion region;
            region.bbox = Rect{left, top, right - left, bottom - top};
            region.score = score;
            region.quad.x = {left, right, right, left};
            region.quad.y = {top, top, bottom, bottom};
            region.quad.has_quad = true;
            regions.push_back(region);
        }
    }

    std::sort(regions.begin(), regions.end(), [](const DetectedRegion& lhs, const DetectedRegion& rhs) {
        if (std::abs(lhs.bbox.y - rhs.bbox.y) > 8.0F) {
            return lhs.bbox.y < rhs.bbox.y;
        }
        return lhs.bbox.x < rhs.bbox.x;
    });
    return regions;
}

std::uint32_t ceil_to_multiple(std::uint32_t value, std::uint32_t multiple)
{
    return std::max(multiple, ((value + multiple - 1U) / multiple) * multiple);
}

std::vector<float> make_recognizer_input(const ImageView& image, const Rect& bbox, std::uint32_t* out_width)
{
    constexpr float Padding = 4.0F;
    const float left = std::clamp(bbox.x - Padding, 0.0F, static_cast<float>(image.width - 1U));
    const float top = std::clamp(bbox.y - Padding, 0.0F, static_cast<float>(image.height - 1U));
    const float right = std::clamp(bbox.x + bbox.width + Padding, left + 1.0F, static_cast<float>(image.width));
    const float bottom = std::clamp(bbox.y + bbox.height + Padding, top + 1.0F, static_cast<float>(image.height));
    const float crop_width = std::max(1.0F, right - left);
    const float crop_height = std::max(1.0F, bottom - top);
    const auto target_width = std::min(RecMaxWidth,
                                       ceil_to_multiple(static_cast<std::uint32_t>(
                                                            std::ceil(crop_width * RecTargetHeight / crop_height)),
                                                        8U));
    *out_width = target_width;

    std::vector<float> tensor(static_cast<std::size_t>(3U) * RecTargetHeight * target_width);
    for (std::uint32_t y = 0; y < RecTargetHeight; ++y) {
        const float source_y = top + (static_cast<float>(y) + 0.5F) * crop_height / static_cast<float>(RecTargetHeight) - 0.5F;
        for (std::uint32_t x = 0; x < target_width; ++x) {
            const float source_x = left + (static_cast<float>(x) + 0.5F) * crop_width / static_cast<float>(target_width) - 0.5F;
            const auto bgr = sample_bgr_bilinear(image, source_x, source_y);
            for (std::size_t channel = 0; channel < 3U; ++channel) {
                const auto offset = channel * static_cast<std::size_t>(RecTargetHeight) * target_width +
                                    static_cast<std::size_t>(y) * target_width + x;
                tensor[offset] = (bgr[channel] / 255.0F - 0.5F) / 0.5F;
            }
        }
    }
    return tensor;
}

struct DecodedText {
    std::string text;
    float confidence = 0.0F;
};

DecodedText ctc_decode(const float* output,
                       std::size_t time_steps,
                       std::size_t class_count,
                       const std::vector<std::string>& characters)
{
    DecodedText decoded;
    std::vector<float> confidences;
    std::size_t last_index = std::numeric_limits<std::size_t>::max();
    for (std::size_t time = 0; time < time_steps; ++time) {
        const auto* row = output + time * class_count;
        const auto* best = std::max_element(row, row + class_count);
        const auto index = static_cast<std::size_t>(best - row);
        if (index != 0U && index != last_index && index < characters.size()) {
            decoded.text += characters[index];
            confidences.push_back(*best);
        }
        last_index = index;
    }

    if (!confidences.empty()) {
        decoded.confidence = std::accumulate(confidences.begin(), confidences.end(), 0.0F) /
                             static_cast<float>(confidences.size());
    }
    return decoded;
}

double elapsed_ms(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - start).count();
}

}  // namespace

class PaddleOcrOnnxBackend::Impl {
public:
    explicit Impl(std::string model_dir_utf8)
    {
        if (model_dir_utf8.empty()) {
            throw OcrError(ErrorCode::model_not_configured, "paddleocr_onnx requires model_dir_utf8");
        }

        model_dir_ = path_from_utf8(model_dir_utf8);
        det_model_ = model_dir_ / DetModelPath;
        det_config_ = model_dir_ / DetConfigPath;
        rec_model_ = model_dir_ / RecModelPath;
        rec_config_ = model_dir_ / RecConfigPath;
        ensure_file_exists(det_model_, DetModelPath);
        ensure_file_exists(det_config_, DetConfigPath);
        ensure_file_exists(rec_model_, RecModelPath);
        ensure_file_exists(rec_config_, RecConfigPath);

        characters_ = load_recognition_characters(rec_config_);
        session_options_.SetIntraOpNumThreads(4);
        session_options_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        det_session_ = std::make_unique<Ort::Session>(env_, det_model_.c_str(), session_options_);
        rec_session_ = std::make_unique<Ort::Session>(env_, rec_model_.c_str(), session_options_);

        Ort::AllocatorWithDefaultOptions allocator;
        det_input_name_ = det_session_->GetInputNameAllocated(0, allocator).get();
        det_output_name_ = det_session_->GetOutputNameAllocated(0, allocator).get();
        rec_input_name_ = rec_session_->GetInputNameAllocated(0, allocator).get();
        rec_output_name_ = rec_session_->GetOutputNameAllocated(0, allocator).get();
    }

    [[nodiscard]] BackendInfo backend_info() const
    {
        BackendInfo info;
        info.name = BackendName;
        info.model_version = ModelVersion;
        info.runtime_version = RuntimeVersion;
        info.execution_provider = ExecutionProvider;
        info.is_configured = det_session_ != nullptr && rec_session_ != nullptr;
        return info;
    }

    [[nodiscard]] BackendCapabilities capabilities() const
    {
        BackendCapabilities capabilities;
        capabilities.accepts_memory_input = true;
        capabilities.returns_source_space_bbox = true;
        capabilities.returns_source_space_quad = true;
        capabilities.returns_confidence = true;
        capabilities.supports_line_boxes = true;
        return capabilities;
    }

    [[nodiscard]] OcrResult recognize(const OcrRequest& request) const
    {
        const auto total_start = std::chrono::steady_clock::now();
        const auto preprocess_start = std::chrono::steady_clock::now();
        const auto resize = det_resize_info(request.image);
        auto det_input = make_detector_input(request.image, resize);
        const auto preprocess_end = std::chrono::steady_clock::now();

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        std::array<std::int64_t, 4> det_shape{1, 3, static_cast<std::int64_t>(resize.height), static_cast<std::int64_t>(resize.width)};
        auto det_tensor = Ort::Value::CreateTensor<float>(memory_info,
                                                          det_input.data(),
                                                          det_input.size(),
                                                          det_shape.data(),
                                                          det_shape.size());
        const char* det_inputs[] = {det_input_name_.c_str()};
        const char* det_outputs[] = {det_output_name_.c_str()};

        const auto det_start = std::chrono::steady_clock::now();
        auto det_output_values = det_session_->Run(Ort::RunOptions{nullptr},
                                                   det_inputs,
                                                   &det_tensor,
                                                   1,
                                                   det_outputs,
                                                   1);
        const auto det_end = std::chrono::steady_clock::now();
        const auto& det_output = det_output_values.front();
        const auto det_info = det_output.GetTensorTypeAndShapeInfo();
        const auto det_dims = det_info.GetShape();
        if (det_dims.size() != 4U) {
            throw OcrError(ErrorCode::internal_error, "paddleocr detector returned unexpected tensor rank");
        }
        const auto map_height = static_cast<std::uint32_t>(det_dims[2]);
        const auto map_width = static_cast<std::uint32_t>(det_dims[3]);
        const auto* probability = det_output.GetTensorData<float>();

        const auto post_start = std::chrono::steady_clock::now();
        const auto regions = extract_regions(probability, map_width, map_height, resize, request.image);

        OcrResult result;
        result.backend = backend_info();
        result.boxes.reserve(regions.size());
        double rec_ms = 0.0;
        for (const auto& region : regions) {
            std::uint32_t rec_width = 0;
            auto rec_input = make_recognizer_input(request.image, region.bbox, &rec_width);
            std::array<std::int64_t, 4> rec_shape{1, 3, RecTargetHeight, static_cast<std::int64_t>(rec_width)};
            auto rec_tensor = Ort::Value::CreateTensor<float>(memory_info,
                                                              rec_input.data(),
                                                              rec_input.size(),
                                                              rec_shape.data(),
                                                              rec_shape.size());
            const char* rec_inputs[] = {rec_input_name_.c_str()};
            const char* rec_outputs[] = {rec_output_name_.c_str()};

            const auto rec_start = std::chrono::steady_clock::now();
            auto rec_output_values = rec_session_->Run(Ort::RunOptions{nullptr},
                                                       rec_inputs,
                                                       &rec_tensor,
                                                       1,
                                                       rec_outputs,
                                                       1);
            const auto rec_end = std::chrono::steady_clock::now();
            rec_ms += elapsed_ms(rec_start, rec_end);

            const auto& rec_output = rec_output_values.front();
            const auto rec_info = rec_output.GetTensorTypeAndShapeInfo();
            const auto rec_dims = rec_info.GetShape();
            if (rec_dims.size() != 3U) {
                throw OcrError(ErrorCode::internal_error, "paddleocr recognizer returned unexpected tensor rank");
            }
            const auto time_steps = static_cast<std::size_t>(rec_dims[1]);
            const auto class_count = static_cast<std::size_t>(rec_dims[2]);
            const auto decoded = ctc_decode(rec_output.GetTensorData<float>(), time_steps, class_count, characters_);
            if (decoded.text.empty()) {
                continue;
            }

            OcrBox box;
            box.text_utf8 = decoded.text;
            box.confidence = std::clamp(region.score * decoded.confidence, 0.0F, 1.0F);
            box.bbox = region.bbox;
            box.quad = region.quad;
            result.boxes.push_back(std::move(box));
        }
        const auto post_end = std::chrono::steady_clock::now();

        result.latency.preprocess_ms = elapsed_ms(preprocess_start, preprocess_end);
        result.latency.inference_ms = elapsed_ms(det_start, det_end) + rec_ms;
        result.latency.postprocess_ms = elapsed_ms(post_start, post_end) - rec_ms;
        result.latency.total_ms = elapsed_ms(total_start, post_end);
        return result;
    }

private:
    std::filesystem::path model_dir_;
    std::filesystem::path det_model_;
    std::filesystem::path det_config_;
    std::filesystem::path rec_model_;
    std::filesystem::path rec_config_;
    std::vector<std::string> characters_;
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "PrivacyLensPaddleOcrOnnx"};
    Ort::SessionOptions session_options_;
    std::unique_ptr<Ort::Session> det_session_;
    std::unique_ptr<Ort::Session> rec_session_;
    std::string det_input_name_;
    std::string det_output_name_;
    std::string rec_input_name_;
    std::string rec_output_name_;
};

PaddleOcrOnnxBackend::PaddleOcrOnnxBackend(std::string model_dir)
    : impl_(std::make_unique<Impl>(std::move(model_dir)))
{
}

PaddleOcrOnnxBackend::~PaddleOcrOnnxBackend() = default;

BackendInfo PaddleOcrOnnxBackend::backend_info() const
{
    return impl_->backend_info();
}

BackendCapabilities PaddleOcrOnnxBackend::capabilities() const
{
    return impl_->capabilities();
}

OcrResult PaddleOcrOnnxBackend::recognize(const OcrRequest& request) const
{
    return impl_->recognize(request);
}

}  // namespace plocr
