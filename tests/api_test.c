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
    CHECK(openqr_style_count(OPENQR_EYE_FRAME) == 16);
    CHECK(openqr_style_count(OPENQR_EYE_BALL) == 18);
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
    CHECK(openqr_result_entry(result, 1, &entry) == OPENQR_INVALID_ARGUMENT);
    openqr_result_destroy(result);
    result = NULL;
    request.options.version = 41;
    CHECK(openqr_generate(&request, &result, &error) == OPENQR_INVALID_ARGUMENT);
    CHECK(result == NULL && error != NULL && openqr_error_message(error) != NULL);
    openqr_error_destroy(error);
    request.options.version = 1;
    request.abi_version = 0;
    CHECK(openqr_generate(&request, &result, NULL) == OPENQR_INVALID_ARGUMENT);
    openqr_result_destroy(NULL);
    openqr_error_destroy(NULL);
    puts("C ABI generation, exact payload, ownership, metadata, and validation passed.");
    return 0;
}
