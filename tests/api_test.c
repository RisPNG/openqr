#include <openqr/openqr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)

int main(void) {
    openqr_request request;
    openqr_result* result = NULL;
    openqr_error* error = NULL;
    const uint8_t payload[] = {' ', 'O', 'p', 'e', 'n', 'Q', 'R', 0, ' ', '\n'};
    openqr_request_init(&request);
    request.payload.data = payload;
    request.payload.size = sizeof(payload);
    request.options.version = 2;
    request.options.seed = 42;
    request.options.flags |= OPENQR_SEED_SET;
    CHECK(openqr_abi_version() == OPENQR_ABI_VERSION);
    CHECK(openqr_style_count(OPENQR_BODY) == 22);
    CHECK(openqr_style_count(OPENQR_EYE_FRAME) == 17);
    CHECK(openqr_style_count(OPENQR_EYE_BALL) == 19);
    CHECK(openqr_generate(&request, &result, &error) == OPENQR_OK);
    CHECK(result != NULL && error == NULL);
    CHECK(openqr_result_count(result) == 1);
    openqr_entry entry;
    CHECK(openqr_result_entry(result, 0, &entry) == OPENQR_OK);
    CHECK(entry.payload.size == sizeof(payload));
    CHECK(memcmp(entry.payload.data, payload, sizeof(payload)) == 0);
    CHECK(entry.verified_frames == 1 && entry.total_frames == 1);
    CHECK(entry.image.size > 8 && memcmp(entry.image.data, "\x89PNG\r\n\x1a\n", 8) == 0);
    CHECK(entry.svg.size > 0);
    CHECK(entry.block_count > 0);
    CHECK(entry.version == 2 && entry.seed == 42);
    openqr_image_info image_info;
    openqr_buffer* pixels = NULL;
    CHECK(openqr_inspect_image(entry.image, entry.payload, &image_info, &pixels, &error) == OPENQR_OK);
    CHECK(pixels != NULL && error == NULL && image_info.verified == 1);
    CHECK(image_info.width == (25 + 4 * entry.version) * request.options.scale);
    CHECK(image_info.height == image_info.width);
    openqr_bytes rgba = openqr_buffer_bytes(pixels);
    CHECK(rgba.size == (size_t)image_info.width * image_info.height * 4);
    CHECK(rgba.data[0] == 255 && rgba.data[1] == 255 && rgba.data[2] == 255 && rgba.data[3] == 255);
    uint8_t different_payload[sizeof(payload)];
    memcpy(different_payload, payload, sizeof(payload));
    different_payload[sizeof(payload) - 2] = '!';
    openqr_buffer* mismatched_pixels = NULL;
    CHECK(openqr_inspect_image(entry.image, (openqr_bytes){different_payload, sizeof(different_payload)}, &image_info, &mismatched_pixels, &error) == OPENQR_OK);
    CHECK(mismatched_pixels != NULL && error == NULL && image_info.verified == 0);
    openqr_bytes mismatched_rgba = openqr_buffer_bytes(mismatched_pixels);
    CHECK(mismatched_rgba.size == rgba.size && memcmp(mismatched_rgba.data, rgba.data, rgba.size) == 0);
    openqr_buffer_destroy(mismatched_pixels);
    CHECK(openqr_result_entry(result, 1, &entry) == OPENQR_INVALID_ARGUMENT);
    openqr_result_destroy(result);
    result = NULL;
    CHECK(openqr_buffer_bytes(pixels).size == rgba.size && rgba.data[0] == 255);
    openqr_buffer_destroy(pixels);
    pixels = NULL;
    const uint8_t invalid_image[] = "invalid image";
    CHECK(openqr_inspect_image((openqr_bytes){invalid_image, sizeof(invalid_image) - 1}, (openqr_bytes){payload, sizeof(payload)}, &image_info, &pixels, &error) == OPENQR_INVALID_IMAGE);
    CHECK(pixels == NULL && error != NULL && image_info.width == 0 && image_info.height == 0 && image_info.verified == 0);
    openqr_error_destroy(error);
    CHECK(openqr_inspect_image((openqr_bytes){NULL, 1}, (openqr_bytes){payload, sizeof(payload)}, &image_info, &pixels, &error) == OPENQR_INVALID_ARGUMENT);
    CHECK(pixels == NULL && error != NULL);
    openqr_error_destroy(error);
    const uint32_t correction_flags[] = {OPENQR_ERROR_CORRECTION_L, OPENQR_ERROR_CORRECTION_M,
                                         OPENQR_ERROR_CORRECTION_Q, OPENQR_ERROR_CORRECTION_H};
    const char corrections[] = {'L', 'M', 'Q', 'H'};
    const uint8_t correction_payload[] = "ECC";
    request.payload = (openqr_bytes){correction_payload, sizeof(correction_payload) - 1};
    request.options.version = 3;
    request.options.scale = 3;
    for (size_t i = 0; i < sizeof(corrections); ++i) {
        request.options.flags = OPENQR_SEED_SET | correction_flags[i];
        CHECK(openqr_generate(&request, &result, &error) == OPENQR_OK);
        CHECK(openqr_result_entry(result, 0, &entry) == OPENQR_OK);
        CHECK(entry.error_correction == corrections[i]);
        CHECK(entry.verified_frames == 1 && entry.total_frames == 1);
        openqr_result_destroy(result);
        result = NULL;
    }
    request.options.flags = OPENQR_ERROR_CORRECTION_L | OPENQR_ERROR_CORRECTION_H;
    CHECK(openqr_generate(&request, &result, &error) == OPENQR_INVALID_ARGUMENT);
    CHECK(result == NULL && error != NULL);
    openqr_error_destroy(error);
    request.options.flags = OPENQR_ERROR_CORRECTION_H;
    request.options.version = 1;
    const uint8_t oversized_payload[] = "abcdefghijklmnopq";
    request.payload = (openqr_bytes){oversized_payload, sizeof(oversized_payload) - 1};
    CHECK(openqr_generate(&request, &result, &error) == OPENQR_NO_VERIFIED_RESULT);
    CHECK(result == NULL && error != NULL);
    CHECK(strstr(openqr_error_message(error), "selected error correction level") != NULL);
    openqr_error_destroy(error);
    request.options.flags = OPENQR_SEED_SET;
    request.options.version = 41;
    CHECK(openqr_generate(&request, &result, &error) == OPENQR_INVALID_ARGUMENT);
    CHECK(result == NULL && error != NULL && openqr_error_message(error) != NULL);
    openqr_error_destroy(error);
    request.options.version = 1;
    request.abi_version = 0;
    CHECK(openqr_generate(&request, &result, NULL) == OPENQR_INVALID_ARGUMENT);
    openqr_result_destroy(NULL);
    openqr_error_destroy(NULL);
    puts("C ABI generation, image inspection, exact payload, ownership, metadata, and validation passed.");
    return 0;
}
