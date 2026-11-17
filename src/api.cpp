#include <openqr/openqr.h>

#include "engine.hpp"
#include "image.hpp"
#include "render.hpp"

#include <algorithm>
#include <exception>
#include <new>
#include <stdexcept>
#include <utility>

struct openqr_result { openqr::Batch batch; };
struct openqr_error { std::string message; };
struct openqr_buffer { std::string bytes; };

namespace {

openqr_status translate_native_failure(openqr_error** error) noexcept {
    openqr_status status = OPENQR_INTERNAL_ERROR;
    const char* message = "OpenQR encountered an unknown native error.";
    try { throw; }
    catch (const openqr::ImageError& failure) { status = OPENQR_INVALID_IMAGE; message = failure.what(); }
    catch (const std::bad_alloc&) { status = OPENQR_OUT_OF_MEMORY; message = "OpenQR could not allocate memory."; }
    catch (const std::invalid_argument& failure) { status = OPENQR_INVALID_ARGUMENT; message = failure.what(); }
    catch (const std::exception& failure) { message = failure.what(); }
    catch (...) {}
    if (error) {
        try { *error = new openqr_error{message}; } catch (...) {}
    }
    return status;
}

}

extern "C" {

uint32_t openqr_abi_version(void) { return OPENQR_ABI_VERSION; }
const char* openqr_version(void) { return OPENQR_VERSION; }

void openqr_options_init(openqr_options* options) {
    if (!options) return;
    *options = {sizeof(openqr_options), OPENQR_ABI_VERSION, 0, 12, 2, 0,
                OPENQR_COLOR, OPENQR_MOSAIC, 1, 1, 0, "rounded", "rounded-dots", "dots"};
}

void openqr_request_init(openqr_request* request) {
    if (!request) return;
    *request = {};
    request->struct_size = sizeof(openqr_request);
    request->abi_version = OPENQR_ABI_VERSION;
    openqr_options_init(&request->options);
}

openqr_status openqr_generate(const openqr_request* request, openqr_result** result, openqr_error** error) {
    if (error) *error = nullptr;
    if (result) *result = nullptr;
    try {
        if (!request || !result) throw std::invalid_argument("A request and result pointer are required.");
        if (request->struct_size != sizeof(openqr_request) || request->abi_version != OPENQR_ABI_VERSION ||
            request->options.struct_size != sizeof(openqr_options) || request->options.abi_version != OPENQR_ABI_VERSION)
            throw std::invalid_argument("The request does not match this OpenQR ABI version.");
        auto batch = openqr::generate(*request);
        if (batch.entries.empty()) {
            if (error) *error = new openqr_error{batch.skipped.empty() ? "No verified result was found." : batch.skipped.front().reason};
            return OPENQR_NO_VERIFIED_RESULT;
        }
        *result = new openqr_result{std::move(batch)};
        return OPENQR_OK;
    } catch (...) {
        return translate_native_failure(error);
    }
}

void openqr_result_destroy(openqr_result* result) { delete result; }
size_t openqr_result_count(const openqr_result* result) { return result ? result->batch.entries.size() : 0; }
uint32_t openqr_result_seed(const openqr_result* result) { return result ? result->batch.seed : 0; }

openqr_status openqr_result_entry(const openqr_result* result, size_t index, openqr_entry* entry) {
    if (!result || !entry || index >= result->batch.entries.size()) return OPENQR_INVALID_ARGUMENT;
    const auto& source = result->batch.entries[index];
    *entry = {source.version, source.seed, source.mode, source.format, source.verified_frames,
              source.total_frames, source.changed_codewords, source.layout_attempts,
              source.error_correction, source.artwork_fraction, source.color_exposure,
              {source.payload.data(), source.payload.size()},
              {source.image.data(), source.image.size()},
              {source.preview.data(), source.preview.size()},
              {source.svg.data(), source.svg.size()},
              {source.target_preview.data(), source.target_preview.size()},
              source.block_errors.data(), source.block_capacities.data(), source.block_capacities.size()};
    return OPENQR_OK;
}

size_t openqr_result_skipped_count(const openqr_result* result) { return result ? result->batch.skipped.size() : 0; }
uint32_t openqr_result_skipped_version(const openqr_result* result, size_t index) {
    return result && index < result->batch.skipped.size() ? result->batch.skipped[index].version : 0;
}
const char* openqr_result_skipped_reason(const openqr_result* result, size_t index) {
    return result && index < result->batch.skipped.size() ? result->batch.skipped[index].reason.c_str() : nullptr;
}
openqr_bytes openqr_result_reference_preview(const openqr_result* result) {
    if (!result) return {};
    return {result->batch.reference_preview.data(), result->batch.reference_preview.size()};
}
openqr_bytes openqr_result_reference_input(const openqr_result* result) {
    if (!result) return {};
    return {result->batch.reference_input.data(), result->batch.reference_input.size()};
}
const char* openqr_error_message(const openqr_error* error) { return error ? error->message.c_str() : nullptr; }
void openqr_error_destroy(openqr_error* error) { delete error; }

openqr_status openqr_inspect_image(openqr_bytes encoded, openqr_bytes expected_payload, openqr_image_info* info, openqr_buffer** pixels, openqr_error** error) {
    if (info) *info = {};
    if (pixels) *pixels = nullptr;
    if (error) *error = nullptr;
    try {
        if (!info || !pixels || (!encoded.data && encoded.size) || (!expected_payload.data && expected_payload.size))
            throw std::invalid_argument("Image inspection requires valid input spans and output pointers.");
        const auto animation = openqr::decode_image({encoded.data, encoded.size}, {.still_only = true});
        const auto& image = animation.frames.front().image;
        const bool verified = openqr::verify_frames(animation, {expected_payload.data, expected_payload.size});
        *pixels = new openqr_buffer{std::string(reinterpret_cast<const char*>(image.pixels.data()), image.pixels.size())};
        *info = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), verified};
        return OPENQR_OK;
    } catch (...) {
        return translate_native_failure(error);
    }
}

size_t openqr_style_count(openqr_style_category category) {
    switch (category) {
    case OPENQR_BODY: return openqr::body_styles().size();
    case OPENQR_EYE_FRAME: return openqr::eye_frame_styles().size();
    case OPENQR_EYE_BALL: return openqr::eye_ball_styles().size();
    }
    return 0;
}
const char* openqr_style_id(openqr_style_category category, size_t index) {
    switch (category) {
    case OPENQR_BODY: return index < openqr::body_styles().size() ? openqr::body_styles()[index].id.data() : nullptr;
    case OPENQR_EYE_FRAME: return index < openqr::eye_frame_styles().size() ? openqr::eye_frame_styles()[index].id.data() : nullptr;
    case OPENQR_EYE_BALL: return index < openqr::eye_ball_styles().size() ? openqr::eye_ball_styles()[index].id.data() : nullptr;
    }
    return nullptr;
}
const char* openqr_style_label(openqr_style_category category, size_t index) {
    switch (category) {
    case OPENQR_BODY: return index < openqr::body_styles().size() ? openqr::body_styles()[index].label.data() : nullptr;
    case OPENQR_EYE_FRAME: return index < openqr::eye_frame_styles().size() ? openqr::eye_frame_styles()[index].label.data() : nullptr;
    case OPENQR_EYE_BALL: return index < openqr::eye_ball_styles().size() ? openqr::eye_ball_styles()[index].label.data() : nullptr;
    }
    return nullptr;
}

openqr_status openqr_style_preview(openqr_style_category category, size_t index, openqr_buffer** buffer, openqr_error** error) {
    if (buffer) *buffer = nullptr;
    if (error) *error = nullptr;
    try {
        const auto* id = openqr_style_id(category, index);
        if (!buffer || !id) throw std::invalid_argument("Choose a valid QR style preview.");
        std::string_view name;
        switch (category) {
        case OPENQR_BODY: name = "body"; break;
        case OPENQR_EYE_FRAME: name = "eye_frame"; break;
        case OPENQR_EYE_BALL: name = "eye_ball"; break;
        default: throw std::invalid_argument("Choose a valid QR style preview.");
        }
        *buffer = new openqr_buffer{openqr::render_style_preview(name, id)};
        return OPENQR_OK;
    } catch (...) {
        return translate_native_failure(error);
    }
}

openqr_bytes openqr_buffer_bytes(const openqr_buffer* buffer) {
    if (!buffer) return {};
    return {reinterpret_cast<const std::uint8_t*>(buffer->bytes.data()), buffer->bytes.size()};
}
void openqr_buffer_destroy(openqr_buffer* buffer) { delete buffer; }

}
