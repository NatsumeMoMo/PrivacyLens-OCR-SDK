#include "pl_ocr.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>

namespace {

struct ImageBuffer {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
};

struct Options {
    std::string backend = "stub";
    std::string model_dir;
    std::string image_path;
    std::string expect_status = "ok";
    bool self_test = false;
    bool print_text = false;
};

class ComScope {
public:
    ComScope()
    {
        hr_ = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr_) && hr_ != RPC_E_CHANGED_MODE) {
            throw std::runtime_error("CoInitializeEx failed");
        }
    }

    ~ComScope()
    {
        if (SUCCEEDED(hr_)) {
            CoUninitialize();
        }
    }

private:
    HRESULT hr_ = E_FAIL;
};

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr()
    {
        reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    [[nodiscard]] T* get() const
    {
        return ptr_;
    }

    [[nodiscard]] T** put()
    {
        reset();
        return &ptr_;
    }

    T* operator->() const
    {
        return ptr_;
    }

    void reset()
    {
        if (ptr_ != nullptr) {
            ptr_->Release();
            ptr_ = nullptr;
        }
    }

private:
    T* ptr_ = nullptr;
};

void print_usage()
{
    std::cout << "Usage:\n"
              << "  pl-ocr-cli --self-test\n"
              << "  pl-ocr-cli --backend stub --self-test\n"
              << "  pl-ocr-cli --backend rapidocr_onnx --model-dir <path> --image <path> [--print-text]\n"
              << "  pl-ocr-cli --backend rapidocr_onnx --model-dir <path> --self-test [--print-text]\n"
              << "  pl-ocr-cli --backend rapidocr_onnx --model-dir <path> --self-test --expect-status model-not-configured\n";
}

std::wstring utf8_to_wide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        throw std::runtime_error("argument is not valid UTF-8");
    }

    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.c_str(), -1, result.data(), size);
    return result;
}

std::string status_name(pl_ocr_status status)
{
    switch (status) {
    case PL_OCR_STATUS_OK:
        return "ok";
    case PL_OCR_STATUS_INVALID_ARGUMENT:
        return "invalid-argument";
    case PL_OCR_STATUS_UNSUPPORTED_ABI:
        return "unsupported-abi";
    case PL_OCR_STATUS_MODEL_NOT_CONFIGURED:
        return "model-not-configured";
    case PL_OCR_STATUS_BACKEND_UNAVAILABLE:
        return "backend-unavailable";
    case PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED:
        return "image-format-unsupported";
    case PL_OCR_STATUS_INTERNAL_ERROR:
        return "internal-error";
    default:
        return "unknown";
    }
}

std::optional<pl_ocr_status> parse_expected_status(const std::string& value)
{
    if (value == "ok") {
        return PL_OCR_STATUS_OK;
    }
    if (value == "invalid-argument") {
        return PL_OCR_STATUS_INVALID_ARGUMENT;
    }
    if (value == "unsupported-abi") {
        return PL_OCR_STATUS_UNSUPPORTED_ABI;
    }
    if (value == "model-not-configured") {
        return PL_OCR_STATUS_MODEL_NOT_CONFIGURED;
    }
    if (value == "backend-unavailable") {
        return PL_OCR_STATUS_BACKEND_UNAVAILABLE;
    }
    if (value == "image-format-unsupported") {
        return PL_OCR_STATUS_IMAGE_FORMAT_UNSUPPORTED;
    }
    if (value == "internal-error") {
        return PL_OCR_STATUS_INTERNAL_ERROR;
    }
    return std::nullopt;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg(argv[index]);
        auto require_value = [&](const char* name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string(name) + " requires a value");
            }
            ++index;
            return argv[index];
        };

        if (arg == "--self-test") {
            options.self_test = true;
        } else if (arg == "--backend") {
            options.backend = require_value("--backend");
        } else if (arg == "--model-dir") {
            options.model_dir = require_value("--model-dir");
        } else if (arg == "--image") {
            options.image_path = require_value("--image");
        } else if (arg == "--print-text") {
            options.print_text = true;
        } else if (arg == "--expect-status") {
            options.expect_status = require_value("--expect-status");
        } else if (arg == "--help" || arg == "-h") {
            print_usage();
            std::exit(0);
        } else {
            throw std::runtime_error("unknown argument: " + std::string(arg));
        }
    }
    return options;
}

ImageBuffer make_gradient_image()
{
    constexpr std::uint32_t width = 640;
    constexpr std::uint32_t height = 360;
    constexpr std::uint32_t stride = width * 4U;
    ImageBuffer image;
    image.width = width;
    image.height = height;
    image.stride = stride;
    image.pixels.resize(static_cast<std::size_t>(stride) * height, 0U);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const auto offset = static_cast<std::size_t>(y) * stride + static_cast<std::size_t>(x) * 4U;
            image.pixels[offset + 0U] = static_cast<std::uint8_t>(x % 255U);
            image.pixels[offset + 1U] = static_cast<std::uint8_t>(y % 255U);
            image.pixels[offset + 2U] = 32U;
            image.pixels[offset + 3U] = 255U;
        }
    }
    return image;
}

void draw_text(HDC dc, int x, int y, const wchar_t* text)
{
    TextOutW(dc, x, y, text, static_cast<int>(wcslen(text)));
}

ImageBuffer make_synthetic_text_image()
{
    constexpr int width = 960;
    constexpr int height = 540;
    constexpr int stride = width * 4;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC dc = CreateCompatibleDC(screen);
    HGDIOBJ old_bitmap = SelectObject(dc, bitmap);

    RECT rect{0, 0, width, height};
    HBRUSH background = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &rect, background);
    DeleteObject(background);

    HFONT title_font = CreateFontW(72, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   VARIABLE_PITCH, L"Arial");
    HFONT body_font = CreateFontW(56, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  VARIABLE_PITCH, L"Arial");

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    HGDIOBJ old_font = SelectObject(dc, title_font);
    draw_text(dc, 80, 90, L"PrivacyLens OCR TEST");
    SelectObject(dc, body_font);
    draw_text(dc, 92, 220, L"demo@example.com");
    draw_text(dc, 92, 330, L"+1 555 0100");

    ImageBuffer image;
    image.width = width;
    image.height = height;
    image.stride = stride;
    image.pixels.resize(static_cast<std::size_t>(stride) * height);
    std::copy_n(static_cast<const std::uint8_t*>(bits), image.pixels.size(), image.pixels.data());

    SelectObject(dc, old_font);
    SelectObject(dc, old_bitmap);
    DeleteObject(body_font);
    DeleteObject(title_font);
    DeleteDC(dc);
    DeleteObject(bitmap);
    ReleaseDC(nullptr, screen);
    return image;
}

ImageBuffer load_image_with_wic(const std::string& path)
{
    ComScope com;

    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(factory.put()));
    if (FAILED(hr)) {
        throw std::runtime_error("CoCreateInstance(IWICImagingFactory) failed");
    }

    ComPtr<IWICBitmapDecoder> decoder;
    const std::wstring wide_path = utf8_to_wide(path);
    hr = factory->CreateDecoderFromFilename(wide_path.c_str(), nullptr, GENERIC_READ,
                                            WICDecodeMetadataCacheOnLoad, decoder.put());
    if (FAILED(hr)) {
        throw std::runtime_error("WIC failed to open image");
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.put());
    if (FAILED(hr)) {
        throw std::runtime_error("WIC failed to get first frame");
    }

    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(converter.put());
    if (FAILED(hr)) {
        throw std::runtime_error("WIC failed to create format converter");
    }

    hr = converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        throw std::runtime_error("WIC failed to convert image to BGRA8");
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0U || height == 0U) {
        throw std::runtime_error("WIC returned invalid image dimensions");
    }

    ImageBuffer image;
    image.width = width;
    image.height = height;
    image.stride = width * 4U;
    image.pixels.resize(static_cast<std::size_t>(image.stride) * image.height);
    hr = converter->CopyPixels(nullptr, image.stride, static_cast<UINT>(image.pixels.size()), image.pixels.data());
    if (FAILED(hr)) {
        throw std::runtime_error("WIC failed to copy image pixels");
    }
    return image;
}

void print_error(const pl_ocr_api_v1& api, pl_ocr_context* context, pl_ocr_status status)
{
    pl_ocr_error_info error{};
    error.struct_size = sizeof(error);
    const auto error_status = api.get_last_error(context, &error);
    std::cerr << "status=" << status_name(status) << " (" << api.status_to_string(status) << ")";
    if (error_status == PL_OCR_STATUS_OK && error.message_utf8 != nullptr) {
        std::cerr << " message=\"" << error.message_utf8 << "\"";
    }
    std::cerr << '\n';
}

int finish_with_status(const Options& options, pl_ocr_status actual_status)
{
    const auto expected = parse_expected_status(options.expect_status);
    if (!expected.has_value()) {
        std::cerr << "unknown --expect-status value: " << options.expect_status << '\n';
        return 2;
    }
    if (*expected != actual_status) {
        std::cerr << "expected status " << options.expect_status << " but got " << status_name(actual_status) << '\n';
        return 1;
    }
    return 0;
}

int run_ocr(const Options& options)
{
    pl_ocr_api_v1 api{};
    auto status = pl_ocr_get_api(PL_OCR_ABI_VERSION_V1, &api);
    if (status != PL_OCR_STATUS_OK) {
        std::cerr << "pl_ocr_get_api failed: " << static_cast<int>(status) << '\n';
        return finish_with_status(options, status);
    }

    pl_ocr_context_options context_options{};
    context_options.struct_size = sizeof(context_options);
    context_options.requested_backend_utf8 = options.backend.c_str();
    context_options.model_dir_utf8 = options.model_dir.empty() ? nullptr : options.model_dir.c_str();

    pl_ocr_context* context = nullptr;
    status = api.create_context(&context_options, &context);
    if (status != PL_OCR_STATUS_OK) {
        print_error(api, nullptr, status);
        return finish_with_status(options, status);
    }

    pl_ocr_backend_info backend{};
    backend.struct_size = sizeof(backend);
    status = api.get_backend_info(context, &backend);
    if (status != PL_OCR_STATUS_OK) {
        print_error(api, context, status);
        api.destroy_context(context);
        return finish_with_status(options, status);
    }

    ImageBuffer image;
    const bool image_from_file = !options.image_path.empty();
    if (image_from_file) {
        image = load_image_with_wic(options.image_path);
    } else if (options.backend == "stub") {
        image = make_gradient_image();
    } else {
        image = make_synthetic_text_image();
    }

    pl_ocr_image sdk_image{};
    sdk_image.struct_size = sizeof(sdk_image);
    sdk_image.data = image.pixels.data();
    sdk_image.width = image.width;
    sdk_image.height = image.height;
    sdk_image.stride_bytes = image.stride;
    sdk_image.pixel_format = PL_OCR_PIXEL_FORMAT_BGRA8;

    pl_ocr_result* result = nullptr;
    status = api.recognize_image(context, &sdk_image, &result);
    if (status != PL_OCR_STATUS_OK) {
        print_error(api, context, status);
        api.destroy_context(context);
        return finish_with_status(options, status);
    }

    std::cout << "PrivacyLens OCR SDK\n";
    std::cout << "abi_version=" << api.get_abi_version() << " sdk_version=0x" << std::hex
              << api.get_sdk_version() << std::dec << '\n';
    std::cout << "backend=" << (backend.backend_name_utf8 != nullptr ? backend.backend_name_utf8 : "-")
              << " model=" << (backend.model_version_utf8 != nullptr ? backend.model_version_utf8 : "-")
              << " runtime=" << (backend.runtime_version_utf8 != nullptr ? backend.runtime_version_utf8 : "-")
              << " provider=" << (backend.execution_provider_utf8 != nullptr ? backend.execution_provider_utf8 : "-")
              << " configured=" << backend.is_configured << '\n';
    std::cout << "status=" << status_name(status)
              << " boxes=" << result->box_count
              << " latency_preprocess_ms=" << std::fixed << std::setprecision(3) << result->latency.preprocess_ms
              << " latency_inference_ms=" << result->latency.inference_ms
              << " latency_postprocess_ms=" << result->latency.postprocess_ms
              << " latency_total_ms=" << result->latency.total_ms << '\n';

    const bool may_print_text = options.print_text || !image_from_file || options.backend == "stub";
    for (std::uint32_t index = 0; index < result->box_count; ++index) {
        const auto& box = result->boxes[index];
        std::cout << "box[" << index << "]"
                  << " text=";
        if (may_print_text) {
            std::cout << '"' << (box.text_utf8 != nullptr ? box.text_utf8 : "") << '"';
        } else {
            std::cout << "<hidden:" << box.text_bytes << " bytes>";
        }
        std::cout << " confidence=" << std::setprecision(3) << box.confidence
                  << " bbox=(" << std::setprecision(1) << box.bbox.x << ","
                  << box.bbox.y << "," << box.bbox.width << "," << box.bbox.height
                  << ") quad=" << box.quad.has_quad << '\n';
    }

    api.destroy_result(result);
    api.destroy_context(context);
    return finish_with_status(options, status);
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 1) {
            print_usage();
            return 0;
        }

        Options options = parse_options(argc, argv);
        if (!options.self_test && options.image_path.empty()) {
            print_usage();
            return 2;
        }

        return run_ocr(options);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 2;
    }
}
