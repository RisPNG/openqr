#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace openqr {

class ImageError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct Frame {
    Image image;
    int duration_ms = 100;
};

struct Animation {
    std::vector<Frame> frames;
    int loop = 0;
};

struct PreparedArtwork {
    Animation animation;
    Image unadjusted;
    std::array<std::uint8_t, 3> matte{255, 255, 255};
    Image backdrop_overlay;
};

struct DecodeOptions {
    bool normalize_animation = false;
    bool input_limits = true;
    bool still_only = false;
};

Animation decode_image(std::span<const std::uint8_t> bytes, DecodeOptions options = {});
std::vector<std::uint8_t> encode_png(const Image& image);
std::vector<std::uint8_t> encode_apng(const Animation& animation);
std::vector<std::uint8_t> encode_gif(const Animation& animation);
Image resize_image(const Image& image, int width, int height, bool box = false);
Animation normalize_reference(const Animation& reference);
PreparedArtwork prepare_reference(const Animation& reference, const Image* background,
                                  std::uint32_t background_rgba, bool color,
                                  double contrast, double brightness, bool mosaic);
Image prepare_background(const Image& image);

}
