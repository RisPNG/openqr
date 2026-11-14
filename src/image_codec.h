#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    int duration_ms;
    int dispose;
    int blend;
} OqrPixels;

typedef struct OqrPngReader OqrPngReader;
OqrPngReader *oqr_png_open(const uint8_t *data, size_t size, uint32_t *width,
                          uint32_t *height, uint32_t *frames, uint32_t *loop,
                          int *hidden, uint32_t pixel_limit, int still_only, char *error);
int oqr_png_next(OqrPngReader *reader, OqrPixels *frame, char *error);
void oqr_png_close(OqrPngReader *reader);
int oqr_png_encode(const OqrPixels *frames, uint32_t count, uint32_t loop,
                   uint8_t **bytes, size_t *size, char *error);
int oqr_jpeg_decode(const uint8_t *bytes, size_t size, OqrPixels *image,
                    uint32_t pixel_limit, char *error);

#ifdef __cplusplus
}
#endif
