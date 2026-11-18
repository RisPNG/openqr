#include "image.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

std::vector<std::uint8_t> load_fixture(const std::string& name) {
    std::ifstream source(std::filesystem::path(OPENQR_TEST_FIXTURES) / name, std::ios::binary);
    if (!source) throw std::runtime_error("Cannot open image fixture " + name);
    return {std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
}

void compare_pixels(const openqr::Image& actual, const openqr::Image& expected, const std::string& name) {
    if (actual.width != expected.width || actual.height != expected.height)
        throw std::runtime_error(name + " dimensions differ from the Pillow fixture.");
    for (std::size_t i = 0; i < actual.pixels.size(); ++i)
        if (actual.pixels[i] != expected.pixels[i])
            throw std::runtime_error(name + " differs at channel " + std::to_string(i) + ": " +
                                     std::to_string(actual.pixels[i]) + " instead of " + std::to_string(expected.pixels[i]));
}

int main() {
    try {
        openqr::Image first{4, 3, std::vector<std::uint8_t>(4 * 3 * 4, 255)};
        for (std::size_t i = 0; i < first.pixels.size(); i += 4) {
            first.pixels[i] = static_cast<std::uint8_t>(i * 3);
            first.pixels[i + 1] = 30;
            first.pixels[i + 2] = 70;
            first.pixels[i + 3] = static_cast<std::uint8_t>(255 - (i / 4) % 4 * 85);
        }
        auto png = openqr::decode_image(openqr::encode_png(first));
        if (png.frames.size() != 1 || png.frames.front().image.pixels != first.pixels)
            throw std::runtime_error("PNG must preserve every RGBA pixel.");
        auto second = first;
        second.pixels[0] = 200;
        second.pixels[3] = 0;
        openqr::Animation animation{{{first, 90}, {second, 170}}, 3};
        auto apng = openqr::decode_image(openqr::encode_apng(animation));
        if (apng.loop != 3 || apng.frames.size() != 2 || apng.frames[0].duration_ms != 90 ||
            apng.frames[1].duration_ms != 170 || apng.frames[1].image.pixels != second.pixels)
            throw std::runtime_error("APNG must preserve pixels, loop count, and frame timing.");
        auto gif = openqr::decode_image(openqr::encode_gif(animation));
        if (gif.loop != 3 || gif.frames.size() != 2 || gif.frames[0].duration_ms != 90 ||
            gif.frames[1].duration_ms != 170 || gif.frames[1].image.pixels[3] != 0)
            throw std::runtime_error("GIF must preserve loop count, timing, and transparent disposal.");
        auto normalized = openqr::normalize_reference(animation);
        if (normalized.frames[0].image.width != 512 || normalized.frames[0].image.height != 512 || normalized.loop != 3)
            throw std::runtime_error("References must use a reusable square canvas.");
        auto source = openqr::decode_image(load_fixture("pipeline-source.png"));
        compare_pixels(openqr::decode_image(load_fixture("pipeline-metadata.png")).frames[0].image,
                       source.frames[0].image, "Ignored compressed metadata");
        auto padded = openqr::normalize_reference(source);
        compare_pixels(padded.frames[0].image,
                       openqr::decode_image(load_fixture("pipeline-normalized.png")).frames[0].image, "Alpha-aware reference padding");
        auto enhanced = openqr::prepare_reference(padded, nullptr, 0x203040ff, true, 1.3, .8, false);
        compare_pixels(enhanced.animation.frames[0].image,
                       openqr::decode_image(load_fixture("pipeline-enhanced.png")).frames[0].image, "Contrast and brightness");
        compare_pixels(openqr::resize_image(enhanced.animation.frames[0].image, 256, 256),
                       openqr::decode_image(load_fixture("pipeline-preview.png")).frames[0].image, "Lanczos preview");
        compare_pixels(openqr::prepare_background(openqr::decode_image(load_fixture("pipeline-background.png")).frames[0].image),
                       openqr::decode_image(load_fixture("pipeline-fit.png")).frames[0].image, "Background crop");
        compare_pixels(openqr::decode_image(load_fixture("orientation.png")).frames[0].image,
                       openqr::decode_image(load_fixture("orientation-expected.png")).frames[0].image, "PNG EXIF orientation");
        auto jpeg = openqr::decode_image(load_fixture("orientation.jpg"));
        if (jpeg.frames[0].image.width != 7 || jpeg.frames[0].image.height != 13)
            throw std::runtime_error("JPEG EXIF orientation must rotate the decoded image.");
        auto incomplete_jpeg = load_fixture("orientation.jpg");
        incomplete_jpeg.resize(incomplete_jpeg.size() - 2);
        try {
            openqr::decode_image(incomplete_jpeg);
            throw std::runtime_error("Truncated JPEG input must fail without writing decoder warnings.");
        } catch (const openqr::ImageError&) {}
        for (const std::string suffix : {"gif", "apng", "webp"}) {
            try {
                openqr::decode_image(load_fixture("composition." + suffix), {.still_only = true});
                throw std::runtime_error("An animated background must be rejected before caching its frames.");
            } catch (const openqr::ImageError& error) {
                if (std::string(error.what()) != "Use a still image for the background.") throw;
            }
            auto composition = openqr::decode_image(load_fixture("composition." + suffix));
            if (composition.loop != 3 || composition.frames.size() != 3)
                throw std::runtime_error("Animation loop and frame count must match the source.");
            constexpr int durations[]{90, 170, 230};
            for (int i = 0; i < 3; ++i) {
                if (composition.frames[i].duration_ms != durations[i]) throw std::runtime_error("Frame timing differs from the source.");
                compare_pixels(composition.frames[i].image,
                               openqr::decode_image(load_fixture("composition-" + suffix + "-" + std::to_string(i) + ".png")).frames[0].image,
                               suffix + " frame composition");
            }
        }
        auto poster = openqr::decode_image(load_fixture("poster.apng"));
        if (poster.frames.size() != 4 || poster.loop != 3 || poster.frames[0].duration_ms != 100)
            throw std::runtime_error("The reusable APNG sequence must retain its default image.");
        for (int i = 0; i < 4; ++i)
            compare_pixels(poster.frames[i].image,
                           openqr::decode_image(load_fixture("poster-" + std::to_string(i) + ".png")).frames[0].image,
                           "APNG default-image composition");
        try {
            openqr::decode_image(std::vector<std::uint8_t>{137, 80, 78, 71, 13, 10, 26, 10});
            throw std::runtime_error("Incomplete images must fail.");
        } catch (const openqr::ImageError&) {}
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
