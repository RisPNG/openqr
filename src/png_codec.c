#include "image_codec.h"

#include <png.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct OqrPngReader {
    png_structp png;
    png_infop info;
    const uint8_t *data;
    size_t size;
    size_t offset;
    uint32_t frames;
    uint32_t index;
    uint32_t pixel_limit;
    int animated;
    uint8_t *pixels;
    png_bytep *rows;
    char message[256];
};

static void png_failure(png_structp png, png_const_charp message) {
    char *error = (char *)png_get_error_ptr(png);
    snprintf(error, 256, "%s", message);
    png_longjmp(png, 1);
}

static void png_notice(png_structp png, png_const_charp message) {
    (void)png;
    (void)message;
}

static void read_png_bytes(png_structp png, png_bytep output, size_t count) {
    OqrPngReader *reader = (OqrPngReader *)png_get_io_ptr(png);
    if (count > reader->size - reader->offset) png_error(png, "The PNG image is incomplete.");
    memcpy(output, reader->data + reader->offset, count);
    reader->offset += count;
}

OqrPngReader *oqr_png_open(const uint8_t *data, size_t size, uint32_t *width,
                          uint32_t *height, uint32_t *frames, uint32_t *loop,
                          int *hidden, uint32_t pixel_limit, int still_only, char *error) {
    OqrPngReader *reader = (OqrPngReader *)calloc(1, sizeof(*reader));
    if (!reader) { snprintf(error, 256, "There is not enough memory for the PNG image."); return NULL; }
    reader->data = data;
    reader->size = size;
    reader->pixel_limit = pixel_limit;
    reader->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, reader->message, png_failure, png_notice);
    if (!reader->png) { free(reader); snprintf(error, 256, "Cannot create the PNG decoder."); return NULL; }
    reader->info = png_create_info_struct(reader->png);
    if (!reader->info) { oqr_png_close(reader); snprintf(error, 256, "Cannot create the PNG decoder."); return NULL; }
    if (setjmp(png_jmpbuf(reader->png))) {
        snprintf(error, 256, "%s", reader->message);
        oqr_png_close(reader);
        return NULL;
    }
    png_set_read_fn(reader->png, reader, read_png_bytes);
    png_set_user_limits(reader->png, pixel_limit, pixel_limit);
    png_set_chunk_malloc_max(reader->png, 1024 * 1024);
    png_set_chunk_cache_max(reader->png, 32);
    static const png_byte ignored_chunks[] = "eXIf\0iCCP\0tEXt\0zTXt\0iTXt";
    png_set_keep_unknown_chunks(reader->png, PNG_HANDLE_CHUNK_NEVER, ignored_chunks, 5);
    png_read_info(reader->png, reader->info);
    *width = png_get_image_width(reader->png, reader->info);
    *height = png_get_image_height(reader->png, reader->info);
    if (!*width || !*height || (uint64_t)*width * *height > pixel_limit)
        png_error(reader->png, "Images must contain at most 16 million pixels.");
    reader->animated = png_get_acTL(reader->png, reader->info, frames, loop) != 0;
    *hidden = reader->animated && png_get_first_frame_is_hidden(reader->png, reader->info);
    if (!reader->animated) { *frames = 1; *loop = 0; }
    reader->frames = *frames;
    if (!reader->frames || reader->frames > 60)
        png_error(reader->png, "Use an animation with at most 60 frames.");
    if (still_only && reader->frames > 1)
        png_error(reader->png, "Use a still image for the background.");
    int type = png_get_color_type(reader->png, reader->info);
    int depth = png_get_bit_depth(reader->png, reader->info);
    if (depth == 16) png_set_strip_16(reader->png);
    if (type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(reader->png);
    if (type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(reader->png);
    int transparent = png_get_valid(reader->png, reader->info, PNG_INFO_tRNS) != 0;
    if (transparent) png_set_tRNS_to_alpha(reader->png);
    if (type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(reader->png);
    if (!(type & PNG_COLOR_MASK_ALPHA) && !transparent)
        png_set_add_alpha(reader->png, 255, PNG_FILLER_AFTER);
    png_set_interlace_handling(reader->png);
    png_read_update_info(reader->png, reader->info);
    return reader;
}

int oqr_png_next(OqrPngReader *reader, OqrPixels *frame, char *error) {
    if (setjmp(png_jmpbuf(reader->png))) {
        snprintf(error, 256, "%s", reader->message);
        free(reader->rows); reader->rows = NULL;
        free(reader->pixels); reader->pixels = NULL;
        return 0;
    }
    memset(frame, 0, sizeof(*frame));
    frame->duration_ms = 100;
    frame->width = png_get_image_width(reader->png, reader->info);
    frame->height = png_get_image_height(reader->png, reader->info);
    if (reader->animated) {
        png_byte dispose = 0, blend = 0;
        png_uint_16 numerator = 0, denominator = 100;
        png_read_frame_head(reader->png, reader->info);
        if (png_get_next_frame_fcTL(reader->png, reader->info, &frame->width, &frame->height,
                                   &frame->x, &frame->y, &numerator, &denominator, &dispose, &blend)) {
            frame->duration_ms = (int)((1000ULL * numerator + (denominator ? denominator : 100) / 2)
                                       / (denominator ? denominator : 100));
            frame->dispose = dispose;
            frame->blend = blend;
        }
    }
    if (!frame->width || !frame->height || (uint64_t)frame->width * frame->height > reader->pixel_limit)
        png_error(reader->png, "Images must contain at most 16 million pixels.");
    reader->pixels = (uint8_t *)malloc((size_t)frame->width * frame->height * 4);
    reader->rows = (png_bytep *)malloc((size_t)frame->height * sizeof(png_bytep));
    if (!reader->pixels || !reader->rows) png_error(reader->png, "There is not enough memory for the PNG image.");
    for (uint32_t y = 0; y < frame->height; ++y)
        reader->rows[y] = reader->pixels + (size_t)y * frame->width * 4;
    png_read_image(reader->png, reader->rows);
    ++reader->index;
    if (reader->index == reader->frames) png_read_end(reader->png, reader->info);
    free(reader->rows); reader->rows = NULL;
    frame->pixels = reader->pixels; reader->pixels = NULL;
    return 1;
}

void oqr_png_close(OqrPngReader *reader) {
    if (!reader) return;
    png_destroy_read_struct(&reader->png, &reader->info, NULL);
    free(reader->rows);
    free(reader->pixels);
    free(reader);
}

typedef struct {
    uint8_t *bytes;
    size_t size;
    size_t capacity;
    png_bytep *rows;
    char message[256];
} PngOutput;

static void write_png_bytes(png_structp png, png_bytep bytes, size_t size) {
    PngOutput *output = (PngOutput *)png_get_io_ptr(png);
    if (size > SIZE_MAX - output->size) png_error(png, "The PNG image is too large.");
    if (output->size + size > output->capacity) {
        size_t capacity = output->size + size;
        if (capacity < output->capacity * 2) capacity = output->capacity * 2;
        uint8_t *data = (uint8_t *)realloc(output->bytes, capacity);
        if (!data) png_error(png, "There is not enough memory for the PNG image.");
        output->bytes = data;
        output->capacity = capacity;
    }
    memcpy(output->bytes + output->size, bytes, size);
    output->size += size;
}

static void flush_png_bytes(png_structp png) { (void)png; }

int oqr_png_encode(const OqrPixels *frames, uint32_t count, uint32_t loop,
                   uint8_t **bytes, size_t *size, char *error) {
    PngOutput *output = (PngOutput *)calloc(1, sizeof(*output));
    if (!output) { snprintf(error, 256, "There is not enough memory for the PNG image."); return 0; }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, output->message, png_failure, png_notice);
    png_infop info = png ? png_create_info_struct(png) : NULL;
    if (!png || !info) {
        png_destroy_write_struct(&png, &info); free(output);
        snprintf(error, 256, "Cannot create the PNG encoder."); return 0;
    }
    if (setjmp(png_jmpbuf(png))) {
        snprintf(error, 256, "%s", output->message);
        png_destroy_write_struct(&png, &info);
        free(output->rows); free(output->bytes); free(output); return 0;
    }
    png_set_write_fn(png, output, write_png_bytes, flush_png_bytes);
    png_set_IHDR(png, info, frames[0].width, frames[0].height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    if (count > 1) png_set_acTL(png, info, count, loop);
    png_write_info(png, info);
    output->rows = (png_bytep *)malloc((size_t)frames[0].height * sizeof(png_bytep));
    if (!output->rows) png_error(png, "There is not enough memory for the PNG image.");
    for (uint32_t i = 0; i < count; ++i) {
        for (uint32_t y = 0; y < frames[i].height; ++y)
            output->rows[y] = frames[i].pixels + (size_t)y * frames[i].width * 4;
        if (count > 1) {
            uint32_t duration = (uint32_t)frames[i].duration_ms;
            uint32_t denominator = 1000;
            uint32_t a = duration, b = denominator;
            while (b) { uint32_t remainder = a % b; a = b; b = remainder; }
            duration /= a; denominator /= a;
            if (duration > 65535) png_error(png, "The frame duration cannot be represented in APNG.");
            png_write_frame_head(png, info, output->rows, frames[i].width, frames[i].height,
                                 0, 0, (png_uint_16)duration, (png_uint_16)denominator,
                                 PNG_fcTL_DISPOSE_OP_NONE, PNG_fcTL_BLEND_OP_SOURCE);
        }
        png_write_image(png, output->rows);
        if (count > 1) png_write_frame_tail(png, info);
    }
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    free(output->rows);
    *bytes = output->bytes; *size = output->size;
    free(output);
    return 1;
}
