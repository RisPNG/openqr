#include "image_codec.h"

#include <stdio.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct jpeg_decompress_struct decoder;
    struct jpeg_error_mgr error;
    jmp_buf jump;
    uint8_t *pixels;
    uint8_t *row;
    char message[256];
} JpegDecoder;

static void jpeg_failure(j_common_ptr decoder) {
    JpegDecoder *state = (JpegDecoder *)decoder->client_data;
    (*decoder->err->format_message)(decoder, state->message);
    longjmp(state->jump, 1);
}

static void jpeg_notice(j_common_ptr decoder, int level) {
    if (level < 0) {
        if (decoder->err->msg_code == JWRN_JPEG_EOF) jpeg_failure(decoder);
        ++decoder->err->num_warnings;
    }
}

int oqr_jpeg_decode(const uint8_t *bytes, size_t size, OqrPixels *image,
                    uint32_t pixel_limit, char *error) {
    JpegDecoder *state = (JpegDecoder *)calloc(1, sizeof(*state));
    if (!state) { snprintf(error, 256, "There is not enough memory for the JPEG image."); return 0; }
    state->decoder.err = jpeg_std_error(&state->error);
    state->error.error_exit = jpeg_failure;
    state->error.emit_message = jpeg_notice;
    state->decoder.client_data = state;
    if (setjmp(state->jump)) {
        snprintf(error, 256, "%s", state->message);
        jpeg_destroy_decompress(&state->decoder);
        free(state->pixels); free(state->row); free(state); return 0;
    }
    jpeg_create_decompress(&state->decoder);
    jpeg_mem_src(&state->decoder, bytes, (unsigned long)size);
    jpeg_read_header(&state->decoder, TRUE);
    if (!state->decoder.image_width || !state->decoder.image_height ||
        (uint64_t)state->decoder.image_width * state->decoder.image_height > pixel_limit) {
        snprintf(state->message, 256, "Images must contain at most 16 million pixels.");
        longjmp(state->jump, 1);
    }
    int cmyk = state->decoder.jpeg_color_space == JCS_CMYK || state->decoder.jpeg_color_space == JCS_YCCK;
    state->decoder.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    jpeg_start_decompress(&state->decoder);
    size_t width = state->decoder.output_width, height = state->decoder.output_height;
    state->pixels = (uint8_t *)malloc(width * height * 4);
    state->row = (uint8_t *)malloc(width * state->decoder.output_components);
    if (!state->pixels || !state->row) {
        snprintf(state->message, 256, "There is not enough memory for the JPEG image.");
        longjmp(state->jump, 1);
    }
    while (state->decoder.output_scanline < height) {
        size_t y = state->decoder.output_scanline;
        JSAMPROW row = state->row;
        jpeg_read_scanlines(&state->decoder, &row, 1);
        for (size_t x = 0; x < width; ++x) {
            uint8_t *target = state->pixels + (y * width + x) * 4;
            if (cmyk) {
                const uint8_t *source = state->row + x * 4;
                for (int channel = 0; channel < 3; ++channel)
                    target[channel] = state->decoder.saw_Adobe_marker
                        ? (uint8_t)((source[channel] * source[3] + 127) / 255)
                        : (uint8_t)(((255 - source[channel]) * (255 - source[3]) + 127) / 255);
            } else memcpy(target, state->row + x * 3, 3);
            target[3] = 255;
        }
    }
    jpeg_finish_decompress(&state->decoder);
    jpeg_destroy_decompress(&state->decoder);
    image->width = (uint32_t)width; image->height = (uint32_t)height;
    image->pixels = state->pixels;
    free(state->row); free(state);
    return 1;
}
