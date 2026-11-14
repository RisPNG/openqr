#ifndef OPENQR_OPENQR_H
#define OPENQR_OPENQR_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && !defined(OPENQR_STATIC)
#if defined(OPENQR_BUILDING)
#define OPENQR_API __declspec(dllexport)
#else
#define OPENQR_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define OPENQR_API __attribute__((visibility("default")))
#else
#define OPENQR_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define OPENQR_ABI_VERSION 1u

typedef enum openqr_status {
    OPENQR_OK = 0,
    OPENQR_INVALID_ARGUMENT = 1,
    OPENQR_INVALID_IMAGE = 2,
    OPENQR_NO_VERIFIED_RESULT = 3,
    OPENQR_OUT_OF_MEMORY = 4,
    OPENQR_INTERNAL_ERROR = 5
} openqr_status;

typedef enum openqr_mode { OPENQR_MOSAIC = 0, OPENQR_PICTURE = 1 } openqr_mode;
typedef enum openqr_format { OPENQR_PNG = 0, OPENQR_GIF = 1 } openqr_format;
typedef enum openqr_style_category { OPENQR_BODY = 0, OPENQR_EYE_FRAME = 1, OPENQR_EYE_BALL = 2 } openqr_style_category;
typedef enum openqr_flags {
    OPENQR_COLOR = 1u,
    OPENQR_VARY_URL = 2u,
    OPENQR_SEED_SET = 4u,
    OPENQR_RANDOMIZE_SEED = 8u
} openqr_flags;

typedef struct openqr_bytes {
    const uint8_t* data;
    size_t size;
} openqr_bytes;

typedef struct openqr_options {
    uint32_t struct_size;
    uint32_t abi_version;
    uint32_t version;
    uint32_t scale;
    uint32_t recovery_margin;
    uint32_t seed;
    uint32_t flags;
    openqr_mode mode;
    double contrast;
    double brightness;
    uint32_t background_rgba;
    const char* body_shape;
    const char* eye_frame;
    const char* eye_ball;
} openqr_options;

typedef struct openqr_request {
    uint32_t struct_size;
    uint32_t abi_version;
    openqr_bytes payload;
    openqr_bytes reference;
    openqr_bytes background;
    openqr_options options;
} openqr_request;

typedef struct openqr_entry {
    uint32_t version;
    uint32_t seed;
    openqr_mode mode;
    openqr_format format;
    uint32_t verified_frames;
    uint32_t total_frames;
    uint32_t changed_codewords;
    uint32_t layout_attempts;
    char error_correction;
    double artwork_fraction;
    double color_exposure;
    openqr_bytes payload;
    openqr_bytes image;
    openqr_bytes preview;
    openqr_bytes svg;
    openqr_bytes target_preview;
    const int32_t* block_errors;
    const int32_t* block_capacities;
    size_t block_count;
} openqr_entry;

typedef struct openqr_result openqr_result;
typedef struct openqr_error openqr_error;
typedef struct openqr_buffer openqr_buffer;

OPENQR_API uint32_t openqr_abi_version(void);
OPENQR_API const char* openqr_version(void);
OPENQR_API void openqr_options_init(openqr_options* options);
OPENQR_API void openqr_request_init(openqr_request* request);
OPENQR_API openqr_status openqr_generate(const openqr_request* request, openqr_result** result, openqr_error** error);
OPENQR_API void openqr_result_destroy(openqr_result* result);
OPENQR_API size_t openqr_result_count(const openqr_result* result);
OPENQR_API uint32_t openqr_result_seed(const openqr_result* result);
OPENQR_API openqr_status openqr_result_entry(const openqr_result* result, size_t index, openqr_entry* entry);
OPENQR_API size_t openqr_result_skipped_count(const openqr_result* result);
OPENQR_API uint32_t openqr_result_skipped_version(const openqr_result* result, size_t index);
OPENQR_API const char* openqr_result_skipped_reason(const openqr_result* result, size_t index);
OPENQR_API openqr_bytes openqr_result_reference_preview(const openqr_result* result);
OPENQR_API openqr_bytes openqr_result_reference_input(const openqr_result* result);
OPENQR_API const char* openqr_error_message(const openqr_error* error);
OPENQR_API void openqr_error_destroy(openqr_error* error);
OPENQR_API size_t openqr_style_count(openqr_style_category category);
OPENQR_API const char* openqr_style_id(openqr_style_category category, size_t index);
OPENQR_API const char* openqr_style_label(openqr_style_category category, size_t index);
OPENQR_API openqr_status openqr_style_preview(openqr_style_category category, size_t index, openqr_buffer** buffer, openqr_error** error);
OPENQR_API openqr_bytes openqr_buffer_bytes(const openqr_buffer* buffer);
OPENQR_API void openqr_buffer_destroy(openqr_buffer* buffer);

#ifdef __cplusplus
}
#endif

#endif
