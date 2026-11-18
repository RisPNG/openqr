#include "image.hpp"
#include "image_codec.h"

#include <gif_lib.h>
#include <webp/demux.h>
extern "C" {
#include "QuantOctree.h"
}

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <numbers>
#include <numeric>
#include <string>

namespace openqr {
namespace {

using NativePixels = std::unique_ptr<std::uint8_t, decltype(&std::free)>;

void check_dimensions(std::uint32_t width, std::uint32_t height, std::uint32_t limit) {
    if (!width || !height || static_cast<std::uint64_t>(width) * height > limit)
        throw ImageError("Images must contain at most 16 million pixels.");
}

std::uint32_t read_number(std::span<const std::uint8_t> data, std::size_t position,
                          int count, bool little = true) {
    if (position > data.size() || static_cast<std::size_t>(count) > data.size() - position)
        throw ImageError("The image is incomplete.");
    std::uint32_t result = 0;
    for (int i = 0; i < count; ++i)
        result |= static_cast<std::uint32_t>(data[position + i]) << ((little ? i : count - i - 1) * 8);
    return result;
}

int exif_orientation(std::span<const std::uint8_t> data) {
    if (data.size() >= 6 && !std::memcmp(data.data(), "Exif\0\0", 6)) data = data.subspan(6);
    if (data.size() < 8 || (std::memcmp(data.data(), "II", 2) && std::memcmp(data.data(), "MM", 2))) return 1;
    const bool little = data[0] == 'I';
    if (read_number(data, 2, 2, little) != 42) return 1;
    const auto offset = read_number(data, 4, 4, little);
    if (offset > data.size() - 2) return 1;
    const auto count = read_number(data, offset, 2, little);
    if (count > (data.size() - offset - 2) / 12) return 1;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto entry = offset + 2 + i * 12;
        if (read_number(data, entry, 2, little) == 0x112 &&
            read_number(data, entry + 2, 2, little) == 3 && read_number(data, entry + 4, 4, little) == 1) {
            const int orientation = static_cast<int>(read_number(data, entry + 8, 2, little));
            return orientation >= 1 && orientation <= 8 ? orientation : 1;
        }
    }
    return 1;
}

int image_orientation(std::span<const std::uint8_t> data) {
    if (data.size() >= 2 && data[0] == 0xff && data[1] == 0xd8) {
        for (std::size_t position = 2; position + 4 <= data.size();) {
            if (data[position++] != 0xff) break;
            while (position < data.size() && data[position] == 0xff) ++position;
            if (position >= data.size()) break;
            const auto marker = data[position++];
            if (marker == 0xda || marker == 0xd9) break;
            if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) continue;
            const auto length = read_number(data, position, 2, false);
            if (length < 2 || length > data.size() - position) break;
            if (marker == 0xe1 && length >= 8 && !std::memcmp(data.data() + position + 2, "Exif\0\0", 6))
                return exif_orientation(data.subspan(position + 2, length - 2));
            position += length;
        }
    } else if (data.size() >= 12 && !std::memcmp(data.data(), "RIFF", 4) &&
               !std::memcmp(data.data() + 8, "WEBP", 4)) {
        for (std::size_t position = 12; position + 8 <= data.size();) {
            const auto length = read_number(data, position + 4, 4);
            if (length > data.size() - position - 8) break;
            if (!std::memcmp(data.data() + position, "EXIF", 4))
                return exif_orientation(data.subspan(position + 8, length));
            position += 8 + static_cast<std::size_t>(length) + (length & 1);
        }
    } else if (data.size() >= 8 && data[0] == 137 && !std::memcmp(data.data() + 1, "PNG", 3)) {
        for (std::size_t position = 8; position + 12 <= data.size();) {
            const auto length = read_number(data, position, 4, false);
            if (length > data.size() - position - 12) break;
            if (!std::memcmp(data.data() + position + 4, "eXIf", 4))
                return exif_orientation(data.subspan(position + 8, length));
            position += 12 + static_cast<std::size_t>(length);
        }
    }
    return 1;
}

Image transpose_image(const Image& source, int orientation) {
    if (orientation == 1) return source;
    const bool swap = orientation >= 5;
    Image result{swap ? source.height : source.width, swap ? source.width : source.height,
                 std::vector<std::uint8_t>(source.pixels.size())};
    for (int y = 0; y < result.height; ++y) for (int x = 0; x < result.width; ++x) {
        int source_x = x, source_y = y;
        switch (orientation) {
        case 2: source_x = source.width - 1 - x; break;
        case 3: source_x = source.width - 1 - x; source_y = source.height - 1 - y; break;
        case 4: source_y = source.height - 1 - y; break;
        case 5: source_x = y; source_y = x; break;
        case 6: source_x = y; source_y = source.height - 1 - x; break;
        case 7: source_x = source.width - 1 - y; source_y = source.height - 1 - x; break;
        case 8: source_x = source.width - 1 - y; source_y = x; break;
        }
        std::memcpy(result.pixels.data() + (static_cast<std::size_t>(y) * result.width + x) * 4,
                    source.pixels.data() + (static_cast<std::size_t>(source_y) * source.width + source_x) * 4, 4);
    }
    return result;
}

void composite_image(Image& target, const Image& source, int left = 0, int top = 0, bool replace = false) {
    if (left < 0 || top < 0 || source.width > target.width - left || source.height > target.height - top)
        throw ImageError("An animation frame extends beyond its canvas.");
    for (int y = 0; y < source.height; ++y) for (int x = 0; x < source.width; ++x) {
        const auto* input = source.pixels.data() + (static_cast<std::size_t>(y) * source.width + x) * 4;
        auto* output = target.pixels.data() + (static_cast<std::size_t>(y + top) * target.width + x + left) * 4;
        if (replace || input[3] == 255) { std::memcpy(output, input, 4); continue; }
        if (!input[3]) continue;
        const unsigned blend = output[3] * (255 - input[3]);
        const unsigned alpha = input[3] * 255 + blend;
        const unsigned coefficient = input[3] * 255 * 255 * 128 / alpha;
        const unsigned inverse = 255 * 128 - coefficient;
        for (int c = 0; c < 3; ++c) {
            const unsigned value = input[c] * coefficient + output[c] * inverse + (128 << 7);
            output[c] = static_cast<std::uint8_t>((((value >> 8) + value) >> 8) >> 7);
        }
        output[3] = static_cast<std::uint8_t>(((alpha + 128) + ((alpha + 128) >> 8)) >> 8);
    }
}

struct ResampleWeights {
    std::vector<int> begin;
    std::vector<std::vector<std::int32_t>> coefficients;
};

ResampleWeights resampling_weights(int input, int output, bool box, double first = 0, double last = -1) {
    if (last < 0) last = input;
    const double scale = (last - first) / output;
    const double filter_scale = std::max(1.0, scale);
    const double support = (box ? .5 : 3.0) * filter_scale;
    ResampleWeights weights;
    weights.begin.resize(output);
    weights.coefficients.resize(output);
    for (int x = 0; x < output; ++x) {
        const double center = first + (x + .5) * scale;
        const int start = std::max(0, static_cast<int>(center - support + .5));
        const int end = std::min(input, static_cast<int>(center + support + .5));
        weights.begin[x] = start;
        std::vector<double> kernel(static_cast<std::size_t>(end - start));
        double total = 0;
        for (int i = start; i < end; ++i) {
            const double position = (i - center + .5) / filter_scale;
            double value = 0;
            if (box) value = position > -.5 && position <= .5 ? 1 : 0;
            else if (position >= -3 && position < 3) {
                if (position == 0) value = 1;
                else {
                    const double angle = position * std::numbers::pi;
                    value = std::sin(angle) / angle * std::sin(angle / 3) / (angle / 3);
                }
            }
            kernel[i - start] = value;
            total += value;
        }
        auto& coefficients = weights.coefficients[x];
        coefficients.resize(kernel.size());
        for (std::size_t i = 0; i < kernel.size(); ++i) {
            const double weight = total ? kernel[i] / total : 0;
            coefficients[i] = static_cast<std::int32_t>(weight * (1 << 22) + (weight < 0 ? -.5 : .5));
        }
    }
    return weights;
}

Image resample_image(const Image& source, int width, int height, bool box,
                     double left = 0, double top = 0, double right = -1, double bottom = -1) {
    if (width <= 0 || height <= 0 || static_cast<std::uint64_t>(width) * height > 100000000)
        throw ImageError("The requested image dimensions are too large.");
    if (source.width <= 0 || source.height <= 0 || source.pixels.size() != static_cast<std::size_t>(source.width) * source.height * 4)
        throw ImageError("The image pixels do not match its dimensions.");
    if (right < 0) right = source.width;
    if (bottom < 0) bottom = source.height;
    if (width == source.width && height == source.height && left == 0 && top == 0 &&
        right == source.width && bottom == source.height) return source;
    Image premultiplied = source;
    for (std::size_t i = 0; i < premultiplied.pixels.size(); i += 4)
        for (int c = 0; c < 3; ++c) {
            const unsigned product = premultiplied.pixels[i + c] * premultiplied.pixels[i + 3] + 128;
            premultiplied.pixels[i + c] = static_cast<std::uint8_t>((product + (product >> 8)) >> 8);
        }
    const auto horizontal = resampling_weights(source.width, width, box, left, right);
    const auto vertical = resampling_weights(source.height, height, box, top, bottom);
    const bool vertical_first = static_cast<std::uint64_t>(width) * source.height > 16000000 &&
                                static_cast<std::uint64_t>(source.width) * height < static_cast<std::uint64_t>(width) * source.height;
    Image intermediate{vertical_first ? source.width : width, vertical_first ? height : source.height, {}};
    intermediate.pixels.resize(static_cast<std::size_t>(intermediate.width) * intermediate.height * 4);
    Image result{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    for (int pass = 0; pass < 2; ++pass) {
        const Image& input = pass == 0 ? premultiplied : intermediate;
        Image& output = pass == 0 ? intermediate : result;
        const bool along_y = vertical_first ? pass == 0 : pass == 1;
        const auto& weights = along_y ? vertical : horizontal;
        for (int y = 0; y < output.height; ++y) for (int x = 0; x < output.width; ++x) {
            const int position = along_y ? y : x;
            for (int c = 0; c < 4; ++c) {
                std::int64_t sum = 1 << 21;
                for (std::size_t k = 0; k < weights.coefficients[position].size(); ++k) {
                    const int input_x = along_y ? x : weights.begin[position] + static_cast<int>(k);
                    const int input_y = along_y ? weights.begin[position] + static_cast<int>(k) : y;
                    sum += static_cast<std::int64_t>(input.pixels[(static_cast<std::size_t>(input_y) * input.width + input_x) * 4 + c])
                           * weights.coefficients[position][k];
                }
                output.pixels[(static_cast<std::size_t>(y) * output.width + x) * 4 + c] =
                    static_cast<std::uint8_t>(std::clamp<std::int64_t>(sum >> 22, 0, 255));
            }
        }
    }
    for (std::size_t i = 0; i < result.pixels.size(); i += 4)
        if (result.pixels[i + 3] != 0 && result.pixels[i + 3] != 255)
            for (int c = 0; c < 3; ++c)
                result.pixels[i + c] = static_cast<std::uint8_t>(std::min(255, result.pixels[i + c] * 255 / result.pixels[i + 3]));
    return result;
}

Image contain_reference(const Image& image) {
    const double scale = std::min(512.0 / image.width, 512.0 / image.height);
    const int width = std::max(1, static_cast<int>(std::nearbyint(image.width * scale)));
    const int height = std::max(1, static_cast<int>(std::nearbyint(image.height * scale)));
    Image result{512, 512, std::vector<std::uint8_t>(512 * 512 * 4)};
    Image scaled = resize_image(image, width, height);
    const int x = static_cast<int>(std::nearbyint((512 - width) * .5));
    const int y = static_cast<int>(std::nearbyint((512 - height) * .5));
    composite_image(result, scaled, x, y, true);
    return result;
}

Animation decode_png(std::span<const std::uint8_t> bytes, DecodeOptions options, std::uint32_t limit, int orientation) {
    std::uint32_t width = 0, height = 0, count = 0, loop = 0;
    int hidden = 0;
    char error[256]{};
    std::unique_ptr<OqrPngReader, decltype(&oqr_png_close)> reader(
        oqr_png_open(bytes.data(), bytes.size(), &width, &height, &count, &loop, &hidden, limit, options.still_only, error), oqr_png_close);
    if (!reader) throw ImageError(error);
    Image canvas{static_cast<int>(width), static_cast<int>(height),
                 std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    Animation animation;
    animation.loop = static_cast<int>(loop);
    for (std::uint32_t i = 0; i < count; ++i) {
        OqrPixels pixels{};
        if (!oqr_png_next(reader.get(), &pixels, error)) throw ImageError(error);
        NativePixels allocated(pixels.pixels, std::free);
        Image patch{static_cast<int>(pixels.width), static_cast<int>(pixels.height),
                    std::vector<std::uint8_t>(pixels.pixels, pixels.pixels + static_cast<std::size_t>(pixels.width) * pixels.height * 4)};
        Image previous;
        if (pixels.dispose == 2) previous = canvas;
        composite_image(canvas, patch, static_cast<int>(pixels.x), static_cast<int>(pixels.y), pixels.blend == 0);
        Image output = transpose_image(canvas, orientation);
        if (options.normalize_animation && count > 1) output = contain_reference(output);
        animation.frames.push_back({std::move(output), pixels.duration_ms});
        if (pixels.dispose == 1)
            for (std::uint32_t y = 0; y < pixels.height; ++y)
                std::fill_n(canvas.pixels.begin() + ((static_cast<std::size_t>(y) + pixels.y) * width + pixels.x) * 4,
                            static_cast<std::size_t>(pixels.width) * 4, 0);
        else if (pixels.dispose == 2) canvas = std::move(previous);
    }
    if (animation.frames.empty()) throw ImageError("The PNG animation contains no frames.");
    return animation;
}

Animation decode_webp(std::span<const std::uint8_t> bytes, DecodeOptions options, std::uint32_t limit, int orientation) {
    const WebPData data{bytes.data(), bytes.size()};
    std::unique_ptr<WebPDemuxer, decltype(&WebPDemuxDelete)> demux(WebPDemux(&data), WebPDemuxDelete);
    if (!demux) throw ImageError("The WebP image cannot be decoded.");
    check_dimensions(WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_WIDTH), WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_HEIGHT), limit);
    const auto count = WebPDemuxGetI(demux.get(), WEBP_FF_FRAME_COUNT);
    if (!count || count > 60) throw ImageError("Use an animation with at most 60 frames.");
    if (options.still_only && count > 1) throw ImageError("Use a still image for the background.");
    WebPAnimDecoderOptions decoder_options;
    if (!WebPAnimDecoderOptionsInit(&decoder_options)) throw ImageError("Cannot initialize the WebP decoder.");
    decoder_options.color_mode = MODE_RGBA;
    decoder_options.use_threads = 0;
    std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)> decoder(
        WebPAnimDecoderNew(&data, &decoder_options), WebPAnimDecoderDelete);
    WebPAnimInfo info{};
    if (!decoder || !WebPAnimDecoderGetInfo(decoder.get(), &info)) throw ImageError("The WebP image cannot be decoded.");
    Animation result;
    result.loop = static_cast<int>(info.loop_count);
    int previous_timestamp = 0;
    while (WebPAnimDecoderHasMoreFrames(decoder.get())) {
        std::uint8_t* pixels = nullptr;
        int timestamp = 0;
        if (!WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp)) throw ImageError("The WebP animation is incomplete.");
        Image frame{static_cast<int>(info.canvas_width), static_cast<int>(info.canvas_height),
                    std::vector<std::uint8_t>(pixels, pixels + static_cast<std::size_t>(info.canvas_width) * info.canvas_height * 4)};
        frame = transpose_image(frame, orientation);
        if (options.normalize_animation && count > 1) frame = contain_reference(frame);
        result.frames.push_back({std::move(frame), timestamp - previous_timestamp});
        previous_timestamp = timestamp;
    }
    if (result.frames.size() != count) throw ImageError("The WebP animation is incomplete.");
    return result;
}

Animation decode_ico(std::span<const std::uint8_t> bytes, DecodeOptions options, std::uint32_t limit) {
    const auto count = read_number(bytes, 4, 2);
    if (!count || count > (bytes.size() - 6) / 16) throw ImageError("The icon directory is invalid.");
    std::size_t chosen = 6;
    std::uint64_t score = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t entry = 6 + i * 16;
        const auto width = bytes[entry] ? bytes[entry] : 256;
        const auto height = bytes[entry + 1] ? bytes[entry + 1] : 256;
        const auto candidate = static_cast<std::uint64_t>(width) * height * 65536 + read_number(bytes, entry + 6, 2);
        if (candidate > score) { chosen = entry; score = candidate; }
    }
    const auto size = read_number(bytes, chosen + 8, 4);
    const auto offset = read_number(bytes, chosen + 12, 4);
    if (offset > bytes.size() || size > bytes.size() - offset) throw ImageError("The icon image is incomplete.");
    const auto dib = bytes.subspan(offset, size);
    if (dib.size() >= 8 && dib[0] == 137 && !std::memcmp(dib.data() + 1, "PNG", 3))
        return decode_png(dib, options, limit, image_orientation(dib));
    const auto header = read_number(dib, 0, 4);
    if (header < 40 || header > dib.size()) throw ImageError("The icon bitmap header is unsupported.");
    const auto width = read_number(dib, 4, 4);
    const auto height = read_number(dib, 8, 4) / 2;
    check_dimensions(width, height, limit);
    const auto depth = read_number(dib, 14, 2);
    if (read_number(dib, 16, 4) != 0 || (depth != 1 && depth != 4 && depth != 8 && depth != 24 && depth != 32))
        throw ImageError("The icon bitmap format is unsupported.");
    const auto palette_count = depth <= 8 ? (read_number(dib, 32, 4) ? read_number(dib, 32, 4) : 1U << depth) : 0;
    if (palette_count > 256) throw ImageError("The icon color table is invalid.");
    const std::size_t pixel_offset = header + palette_count * 4;
    const std::size_t stride = ((static_cast<std::size_t>(width) * depth + 31) / 32) * 4;
    if (pixel_offset > dib.size() || stride * height > dib.size() - pixel_offset)
        throw ImageError("The icon bitmap is incomplete.");
    Image image{static_cast<int>(width), static_cast<int>(height),
                std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    bool has_alpha = false;
    for (std::uint32_t y = 0; y < height; ++y) for (std::uint32_t x = 0; x < width; ++x) {
        const auto row = pixel_offset + static_cast<std::size_t>(height - 1 - y) * stride;
        std::size_t position = row + x * (depth / 8);
        if (depth <= 8) {
            const auto index = (dib[row + x * depth / 8] >> (8 - depth - x * depth % 8)) & ((1U << depth) - 1);
            if (index >= palette_count) throw ImageError("The icon color index is invalid.");
            position = header + index * 4;
        }
        auto* output = image.pixels.data() + (static_cast<std::size_t>(y) * width + x) * 4;
        output[0] = dib[position + 2]; output[1] = dib[position + 1]; output[2] = dib[position];
        output[3] = depth == 32 ? dib[position + 3] : 255;
        has_alpha |= output[3] != 0;
    }
    const std::size_t mask_offset = pixel_offset + stride * height;
    const std::size_t mask_stride = ((width + 31) / 32) * 4;
    if (depth != 32 || !has_alpha) {
        for (std::uint32_t y = 0; y < height; ++y) for (std::uint32_t x = 0; x < width; ++x) {
            const bool transparent = mask_offset <= dib.size() && mask_stride * height <= dib.size() - mask_offset &&
                (dib[mask_offset + (height - 1 - y) * mask_stride + x / 8] & (0x80 >> (x % 8)));
            image.pixels[(static_cast<std::size_t>(y) * width + x) * 4 + 3] = transparent ? 0 : 255;
        }
    }
    return {{{std::move(image), 100}}, 0};
}

struct GifInput {
    std::span<const std::uint8_t> bytes;
    std::size_t position = 0;
};

int read_gif_bytes(GifFileType* file, GifByteType* output, int count) {
    auto& input = *static_cast<GifInput*>(file->UserData);
    const auto available = std::min<std::size_t>(static_cast<std::size_t>(count), input.bytes.size() - input.position);
    std::memcpy(output, input.bytes.data() + input.position, available);
    input.position += available;
    return static_cast<int>(available);
}

struct GifCloser {
    void operator()(GifFileType* file) const { int error = 0; DGifCloseFile(file, &error); }
};

struct GifWriterCloser {
    void operator()(GifFileType* file) const { int error = 0; EGifCloseFile(file, &error); }
};

Animation decode_gif(std::span<const std::uint8_t> bytes, DecodeOptions options, std::uint32_t limit) {
    GifInput input{bytes};
    int error = 0;
    std::unique_ptr<GifFileType, GifCloser> file(DGifOpen(&input, read_gif_bytes, &error));
    if (!file) throw ImageError("The GIF image cannot be decoded.");
    check_dimensions(file->SWidth, file->SHeight, limit);
    Image canvas{file->SWidth, file->SHeight,
                 std::vector<std::uint8_t>(static_cast<std::size_t>(file->SWidth) * file->SHeight * 4)};
    Animation result;
    GraphicsControlBlock control{DISPOSAL_UNSPECIFIED, false, 10, NO_TRANSPARENT_COLOR};
    GifRecordType record = UNDEFINED_RECORD_TYPE;
    while (true) {
        if (DGifGetRecordType(file.get(), &record) != GIF_OK) throw ImageError("The GIF animation is incomplete.");
        if (record == TERMINATE_RECORD_TYPE) break;
        if (record == EXTENSION_RECORD_TYPE) {
            int code = 0;
            GifByteType* extension = nullptr;
            if (DGifGetExtension(file.get(), &code, &extension) != GIF_OK) throw ImageError("The GIF extension is incomplete.");
            bool loop_extension = false;
            if (extension && code == GRAPHICS_EXT_FUNC_CODE && extension[0] == 4)
                DGifExtensionToGCB(4, extension + 1, &control);
            if (extension && code == APPLICATION_EXT_FUNC_CODE && extension[0] == 11)
                loop_extension = !std::memcmp(extension + 1, "NETSCAPE2.0", 11) || !std::memcmp(extension + 1, "ANIMEXTS1.0", 11);
            while (extension) {
                if (loop_extension && extension[0] >= 3 && extension[1] == 1)
                    result.loop = extension[2] | (extension[3] << 8);
                if (DGifGetExtensionNext(file.get(), &extension) != GIF_OK) throw ImageError("The GIF extension is incomplete.");
            }
        } else if (record == IMAGE_DESC_RECORD_TYPE) {
            if (options.still_only && !result.frames.empty()) throw ImageError("Use a still image for the background.");
            if (result.frames.size() >= 60) throw ImageError("Use an animation with at most 60 frames.");
            if (DGifGetImageDesc(file.get()) != GIF_OK) throw ImageError("The GIF frame is incomplete.");
            const auto& descriptor = file->Image;
            if (descriptor.Left < 0 || descriptor.Top < 0 || descriptor.Width <= 0 || descriptor.Height <= 0 ||
                descriptor.Width > canvas.width - descriptor.Left || descriptor.Height > canvas.height - descriptor.Top)
                throw ImageError("A GIF frame extends beyond its canvas.");
            const auto* palette = descriptor.ColorMap ? descriptor.ColorMap : file->SColorMap;
            if (!palette) throw ImageError("The GIF frame has no color table.");
            if (result.frames.empty() && control.TransparentColor == NO_TRANSPARENT_COLOR && file->SColorMap &&
                file->SBackGroundColor >= 0 && file->SBackGroundColor < file->SColorMap->ColorCount) {
                const auto background = file->SColorMap->Colors[file->SBackGroundColor];
                for (std::size_t i = 0; i < canvas.pixels.size(); i += 4) {
                    canvas.pixels[i] = background.Red; canvas.pixels[i + 1] = background.Green;
                    canvas.pixels[i + 2] = background.Blue; canvas.pixels[i + 3] = 255;
                }
            }
            Image previous;
            if (control.DisposalMode == DISPOSE_PREVIOUS) previous = canvas;
            std::vector<GifPixelType> row(descriptor.Width);
            std::vector<int> rows;
            if (descriptor.Interlace) {
                constexpr int starts[]{0, 4, 2, 1}, steps[]{8, 8, 4, 2};
                for (int pass = 0; pass < 4; ++pass)
                    for (int y = starts[pass]; y < descriptor.Height; y += steps[pass]) rows.push_back(y);
            } else {
                rows.resize(descriptor.Height);
                std::iota(rows.begin(), rows.end(), 0);
            }
            for (int y : rows) {
                if (DGifGetLine(file.get(), row.data(), descriptor.Width) != GIF_OK) throw ImageError("The GIF pixels are incomplete.");
                for (int x = 0; x < descriptor.Width; ++x) {
                    if (row[x] == control.TransparentColor) continue;
                    if (row[x] >= palette->ColorCount) throw ImageError("The GIF color index is invalid.");
                    const auto color = palette->Colors[row[x]];
                    auto* pixel = canvas.pixels.data() + (static_cast<std::size_t>(descriptor.Top + y) * canvas.width + descriptor.Left + x) * 4;
                    pixel[0] = color.Red; pixel[1] = color.Green; pixel[2] = color.Blue; pixel[3] = 255;
                }
            }
            Image output = options.normalize_animation ? contain_reference(canvas) : canvas;
            result.frames.push_back({std::move(output), control.DelayTime * 10});
            if (control.DisposalMode == DISPOSE_BACKGROUND) {
                std::array<std::uint8_t, 4> background{};
                if (control.TransparentColor == NO_TRANSPARENT_COLOR && file->SColorMap &&
                    file->SBackGroundColor >= 0 && file->SBackGroundColor < file->SColorMap->ColorCount) {
                    const auto color = file->SColorMap->Colors[file->SBackGroundColor];
                    background = {color.Red, color.Green, color.Blue, 255};
                }
                for (int y = 0; y < descriptor.Height; ++y) for (int x = 0; x < descriptor.Width; ++x)
                    std::memcpy(canvas.pixels.data() + (static_cast<std::size_t>(descriptor.Top + y) * canvas.width + descriptor.Left + x) * 4,
                                background.data(), 4);
            } else if (control.DisposalMode == DISPOSE_PREVIOUS) canvas = std::move(previous);
            control = {DISPOSAL_UNSPECIFIED, false, 10, NO_TRANSPARENT_COLOR};
        }
    }
    if (result.frames.empty()) throw ImageError("The GIF image contains no frames.");
    return result;
}

struct GifOutput {
    std::vector<std::uint8_t> bytes;
    bool failed = false;
};

int write_gif_bytes(GifFileType* file, const GifByteType* data, int size) {
    auto& output = *static_cast<GifOutput*>(file->UserData);
    try { output.bytes.insert(output.bytes.end(), data, data + size); }
    catch (const std::bad_alloc&) { output.failed = true; return 0; }
    return size;
}

}

Animation decode_image(std::span<const std::uint8_t> bytes, DecodeOptions options) {
    if (bytes.empty()) throw ImageError("The reference image is empty.");
    if (options.input_limits && bytes.size() > 10 * 1024 * 1024) throw ImageError("Images must be at most 10 MiB.");
    const std::uint32_t limit = options.input_limits ? 16000000 : 100000000;
    if (bytes.size() >= 8 && bytes[0] == 137 && !std::memcmp(bytes.data() + 1, "PNG\r\n\032\n", 7))
        return decode_png(bytes, options, limit, image_orientation(bytes));
    if (bytes.size() >= 6 && (!std::memcmp(bytes.data(), "GIF87a", 6) || !std::memcmp(bytes.data(), "GIF89a", 6)))
        return decode_gif(bytes, options, limit);
    if (bytes.size() >= 12 && !std::memcmp(bytes.data(), "RIFF", 4) && !std::memcmp(bytes.data() + 8, "WEBP", 4))
        return decode_webp(bytes, options, limit, image_orientation(bytes));
    if (bytes.size() >= 6 && read_number(bytes, 0, 2) == 0 && read_number(bytes, 2, 2) == 1)
        return decode_ico(bytes, options, limit);
    if (bytes.size() >= 2 && bytes[0] == 0xff && bytes[1] == 0xd8) {
        OqrPixels pixels{};
        char error[256]{};
        if (!oqr_jpeg_decode(bytes.data(), bytes.size(), &pixels, limit, error)) throw ImageError(error);
        NativePixels allocated(pixels.pixels, std::free);
        Image image{static_cast<int>(pixels.width), static_cast<int>(pixels.height),
                    std::vector<std::uint8_t>(pixels.pixels, pixels.pixels + static_cast<std::size_t>(pixels.width) * pixels.height * 4)};
        return {{{transpose_image(image, image_orientation(bytes)), 100}}, 0};
    }
    throw ImageError("Choose a readable PNG, JPEG, WebP, GIF, or ICO image.");
}

std::vector<std::uint8_t> encode_png(const Image& image) {
    return encode_apng({{{image, 100}}, 0});
}

std::vector<std::uint8_t> encode_apng(const Animation& animation) {
    if (animation.frames.empty() || animation.frames.size() > 60) throw ImageError("An image must contain between 1 and 60 frames.");
    std::vector<OqrPixels> frames;
    for (const auto& frame : animation.frames) {
        if (frame.image.width != animation.frames[0].image.width || frame.image.height != animation.frames[0].image.height ||
            frame.image.width <= 0 || frame.image.height <= 0 || frame.duration_ms < 0 ||
            frame.image.pixels.size() != static_cast<std::size_t>(frame.image.width) * frame.image.height * 4)
            throw ImageError("Animation frames must have matching dimensions and RGBA pixels.");
        frames.push_back({const_cast<std::uint8_t*>(frame.image.pixels.data()), static_cast<std::uint32_t>(frame.image.width),
                          static_cast<std::uint32_t>(frame.image.height), 0, 0, frame.duration_ms, 0, 0});
    }
    std::uint8_t* bytes = nullptr;
    std::size_t size = 0;
    char error[256]{};
    if (!oqr_png_encode(frames.data(), static_cast<std::uint32_t>(frames.size()),
                        static_cast<std::uint32_t>(animation.loop), &bytes, &size, error)) throw ImageError(error);
    NativePixels allocated(bytes, std::free);
    return {bytes, bytes + size};
}

std::vector<std::uint8_t> encode_gif(const Animation& animation) {
    if (animation.frames.empty() || animation.frames.size() > 60) throw ImageError("An image must contain between 1 and 60 frames.");
    const auto& first = animation.frames.front().image;
    if (first.width <= 0 || first.height <= 0 || first.width > 65535 || first.height > 65535)
        throw ImageError("The GIF dimensions are invalid.");
    GifOutput output;
    int error = 0;
    GifFileType* file = EGifOpen(&output, write_gif_bytes, &error);
    if (!file) throw ImageError("Cannot create the GIF encoder.");
    std::unique_ptr<GifFileType, GifWriterCloser> encoded(file);
    EGifSetGifVersion(file, true);
    if (EGifPutScreenDesc(file, first.width, first.height, 8, 0, nullptr) != GIF_OK ||
        EGifPutExtensionLeader(file, APPLICATION_EXT_FUNC_CODE) != GIF_OK ||
        EGifPutExtensionBlock(file, 11, "NETSCAPE2.0") != GIF_OK) throw ImageError("Cannot write the GIF header.");
    if (animation.loop < 0 || animation.loop > 65535) throw ImageError("The loop count cannot be represented in GIF.");
    const int loop = animation.loop;
    const std::uint8_t repetition[]{1, static_cast<std::uint8_t>(loop), static_cast<std::uint8_t>(loop >> 8)};
    if (EGifPutExtensionBlock(file, 3, repetition) != GIF_OK || EGifPutExtensionTrailer(file) != GIF_OK)
        throw ImageError("Cannot write the GIF loop count.");
    for (const auto& frame : animation.frames) {
        const auto& image = frame.image;
        if (image.width != first.width || image.height != first.height || frame.duration_ms < 0 ||
            frame.duration_ms > 655350 ||
            image.pixels.size() != static_cast<std::size_t>(image.width) * image.height * 4)
            throw ImageError("Animation frames must have matching dimensions and RGBA pixels.");
        const auto count = static_cast<std::uint32_t>(image.width * image.height);
        bool transparent = false;
        for (std::size_t i = 3; i < image.pixels.size(); i += 4) transparent |= image.pixels[i] < 128;
        Pixel* palette = nullptr;
        std::uint32_t palette_size = 0;
        std::uint32_t* indices = nullptr;
        if (!quantize_octree(reinterpret_cast<Pixel*>(const_cast<std::uint8_t*>(image.pixels.data())), count,
                             transparent ? 255 : 256, &palette, &palette_size, &indices, 0))
            throw ImageError("There is not enough memory for the GIF palette.");
        std::unique_ptr<Pixel, decltype(&std::free)> palette_storage(palette, std::free);
        std::unique_ptr<std::uint32_t, decltype(&std::free)> index_storage(indices, std::free);
        std::array<GifColorType, 256> colors{};
        for (std::uint32_t i = 0; i < palette_size; ++i) colors[i] = {palette[i].c.r, palette[i].c.g, palette[i].c.b};
        int map_size = 2;
        while (map_size < static_cast<int>(palette_size) + (transparent ? 1 : 0)) map_size *= 2;
        const int transparency = transparent ? map_size - 1 : NO_TRANSPARENT_COLOR;
        std::unique_ptr<ColorMapObject, decltype(&GifFreeMapObject)> map(GifMakeMapObject(map_size, colors.data()), GifFreeMapObject);
        if (!map) throw ImageError("Cannot create the GIF color table.");
        const GraphicsControlBlock control{DISPOSE_BACKGROUND, false, frame.duration_ms / 10, transparency};
        std::array<GifByteType, 4> extension{};
        EGifGCBToExtension(&control, extension.data());
        if (EGifPutExtension(file, GRAPHICS_EXT_FUNC_CODE, 4, extension.data()) != GIF_OK ||
            EGifPutImageDesc(file, 0, 0, image.width, image.height, false, map.get()) != GIF_OK)
            throw ImageError("Cannot write the GIF frame header.");
        std::vector<GifPixelType> row(image.width);
        for (int y = 0; y < image.height; ++y) {
            for (int x = 0; x < image.width; ++x) {
                const auto position = static_cast<std::size_t>(y) * image.width + x;
                row[x] = transparent && image.pixels[position * 4 + 3] < 128
                    ? static_cast<GifPixelType>(transparency) : static_cast<GifPixelType>(indices[position]);
            }
            if (EGifPutLine(file, row.data(), image.width) != GIF_OK) throw ImageError("Cannot write the GIF pixels.");
        }
    }
    encoded.release();
    if (EGifCloseFile(file, &error) != GIF_OK || output.failed) throw ImageError("Cannot finish the GIF image.");
    return std::move(output.bytes);
}

Image resize_image(const Image& image, int width, int height, bool box) {
    return resample_image(image, width, height, box);
}

Animation normalize_reference(const Animation& reference) {
    Animation result;
    result.loop = reference.loop;
    for (const auto& frame : reference.frames)
        result.frames.push_back({contain_reference(frame.image), frame.duration_ms});
    return result;
}

Image prepare_background(const Image& image) {
    const double side = std::min(image.width, image.height);
    const double left = (image.width - side) * .5;
    const double top = (image.height - side) * .5;
    return resample_image(image, 512, 512, false, left, top, left + side, top + side);
}

PreparedArtwork prepare_reference(const Animation& reference, const Image* background,
                                  std::uint32_t background_rgba, bool color,
                                  double contrast, double brightness, bool mosaic) {
    if (!std::isfinite(contrast) || contrast < .1 || contrast > 3 || !std::isfinite(brightness) || brightness < .1 || brightness > 3)
        throw ImageError("Contrast and brightness must be between 0.1 and 3.");
    Image backdrop{512, 512, std::vector<std::uint8_t>(512 * 512 * 4)};
    for (std::size_t i = 0; i < backdrop.pixels.size(); i += 4) {
        backdrop.pixels[i] = static_cast<std::uint8_t>(background_rgba >> 24);
        backdrop.pixels[i + 1] = static_cast<std::uint8_t>(background_rgba >> 16);
        backdrop.pixels[i + 2] = static_cast<std::uint8_t>(background_rgba >> 8);
        backdrop.pixels[i + 3] = static_cast<std::uint8_t>(background_rgba);
    }
    PreparedArtwork result;
    result.animation.loop = reference.loop;
    if (background_rgba & 255) {
        for (int c = 0; c < 3; ++c)
            result.matte[c] = static_cast<std::uint8_t>(background_rgba >> (24 - c * 8));
    }
    if (background) {
        Image overlay = prepare_background(*background);
        composite_image(backdrop, overlay);
        if (mosaic && result.matte != std::array<std::uint8_t, 3>{255, 255, 255})
            result.backdrop_overlay = std::move(overlay);
    }
    Image shaping{512, 512, std::vector<std::uint8_t>(512 * 512 * 4, 255)};
    composite_image(shaping, backdrop);
    const bool adjust = !color || contrast != 1 || brightness != 1;
    for (const auto& frame : reference.frames) {
        Image image = shaping;
        composite_image(image, contain_reference(frame.image));
        if (!adjust) {
            result.animation.frames.push_back({std::move(image), frame.duration_ms});
            continue;
        }
        if (mosaic) result.unadjusted = image;
        std::uint64_t luminance_sum = 0;
        for (std::size_t i = 0; i < image.pixels.size(); i += 4) {
            const auto luminance = static_cast<std::uint8_t>((19595U * image.pixels[i] + 38470U * image.pixels[i + 1]
                                                            + 7471U * image.pixels[i + 2] + 32768U) >> 16);
            luminance_sum += luminance;
            if (!color) image.pixels[i] = image.pixels[i + 1] = image.pixels[i + 2] = luminance;
        }
        const int mean = static_cast<int>(static_cast<double>(luminance_sum) / (512 * 512) + .5);
        const float contrast_factor = static_cast<float>(contrast), brightness_factor = static_cast<float>(brightness);
        for (std::size_t i = 0; i < image.pixels.size(); i += 4)
            for (int c = 0; c < 3; ++c) {
                const int contrasted = std::clamp(static_cast<int>(mean + contrast_factor * (static_cast<int>(image.pixels[i + c]) - mean)), 0, 255);
                image.pixels[i + c] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(brightness_factor * contrasted), 0, 255));
            }
        result.animation.frames.push_back({std::move(image), frame.duration_ms});
    }
    return result;
}

}
