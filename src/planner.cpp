#include "planner.hpp"
#include "random.hpp"

#include "qrcodegen.hpp"
#define POCKETFFT_NO_MULTITHREADING
#include "pocketfft_hdronly.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <numeric>
#include <stdexcept>
#include <string>

namespace openqr {

QrCode encode_qr(std::span<const std::uint8_t> payload, int version, char error_correction, int mask) {
    using NativeQr = qrcodegen::QrCode;
    NativeQr::Ecc correction;
    switch (error_correction) {
    case 'L': correction = NativeQr::Ecc::LOW; break;
    case 'M': correction = NativeQr::Ecc::MEDIUM; break;
    case 'Q': correction = NativeQr::Ecc::QUARTILE; break;
    case 'H': correction = NativeQr::Ecc::HIGH; break;
    default: throw std::invalid_argument("Choose a valid QR error correction level.");
    }
    std::vector<qrcodegen::QrSegment> segments;
    const bool ascii = std::all_of(payload.begin(), payload.end(), [](std::uint8_t value) { return value < 128; });
    const bool numeric = std::all_of(payload.begin(), payload.end(), [](std::uint8_t value) { return value >= '0' && value <= '9'; });
    const bool alphanumeric = std::all_of(payload.begin(), payload.end(), [](std::uint8_t value) {
        return std::string_view("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:").find(static_cast<char>(value)) != std::string_view::npos;
    });
    if (ascii && (numeric || alphanumeric)) {
        const std::string text(payload.begin(), payload.end());
        segments.push_back(numeric ? qrcodegen::QrSegment::makeNumeric(text.c_str()) : qrcodegen::QrSegment::makeAlphanumeric(text.c_str()));
    } else {
        if (!ascii)
            segments.push_back(qrcodegen::QrSegment::makeEci(26));
        segments.push_back(qrcodegen::QrSegment::makeBytes(std::vector<std::uint8_t>(payload.begin(), payload.end())));
    }
    const auto encoded = NativeQr::encodeSegments(segments, correction, version, version, mask, false);
    QrCode qr;
    qr.version = encoded.getVersion();
    qr.size = encoded.getSize();
    qr.mask = encoded.getMask();
    qr.error_correction = error_correction;
    qr.data_capacity_bits = encoded.getDataCapacityBits();
    qr.codeword_blocks = encoded.getCodewordBlocks();
    qr.block_capacities = encoded.getBlockCapacities();
    qr.matrix.resize(qr.size * qr.size);
    qr.roles.resize(qr.matrix.size());
    for (int row = 0; row < qr.size; ++row) {
        for (int column = 0; column < qr.size; ++column) {
            const int index = row * qr.size + column;
            qr.matrix[index] = encoded.getModule(column, row);
            qr.roles[index] = static_cast<ModuleRole>(encoded.getModuleType(column, row));
        }
    }
    return qr;
}

namespace {

std::array<double, 3> oklab(const std::uint8_t* rgb) {
    static const auto linear = [] {
        std::array<double, 256> values{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double value = i / 255.;
            values[i] = value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4);
        }
        return values;
    }();
    const double r = linear[rgb[0]], g = linear[rgb[1]], b = linear[rgb[2]];
    const double l = std::cbrt(.4122214708 * r + .5363325363 * g + .0514459929 * b);
    const double m = std::cbrt(.2119034982 * r + .6806995451 * g + .1073969566 * b);
    const double s = std::cbrt(.0883024619 * r + .2817188376 * g + .6299787005 * b);
    return {
        .2104542553 * l + .7936177850 * m - .0040720468 * s,
        1.9779984951 * l - 2.4285922050 * m + .4505937099 * s,
        .0259040371 * l + .7827717662 * m - .8086757660 * s
    };
}

}

MosaicReference prepare_mosaic_reference(const PreparedArtwork& artwork, int size) {
    const Image pixels = resize_image(artwork.animation.frames.front().image, size, size, true);
    const Image unadjusted = artwork.unadjusted.pixels.empty()
        ? Image{} : resize_image(artwork.unadjusted, size, size, true);
    const auto& comparison = unadjusted.pixels.empty() ? pixels : unadjusted;
    const Image overlay = artwork.backdrop_overlay.pixels.empty()
        ? Image{} : resize_image(artwork.backdrop_overlay, size, size, true);
    constexpr std::array<double, 3> white{1, 0, 0};
    const auto matte = oklab(artwork.matte.data());
    const double matte_depth = std::hypot(white[0] - matte[0], matte[1], matte[2]);
    const double matte_contrast = std::clamp((matte_depth - .065) / .235, 0., 1.);
    MosaicReference reference;
    reference.size = size;
    reference.ink_colors.resize(size * size * 3);
    reference.target.resize(size * size);
    reference.contours.resize(size * size);
    reference.weights.resize(size * size);
    reference.pixel_art = Image{size, size, std::vector<std::uint8_t>(size * size * 4, 255)};
    std::vector<double> matte_match(size * size);
    std::vector<std::uint8_t> levels(size * size);
    std::array<double, 256> histogram{};
    for (int index = 0; index < size * size; ++index) {
        const auto appearance = oklab(comparison.pixels.data() + index * 4);
        const double matte_visibility = overlay.pixels.empty() ? 1. : 1 - overlay.pixels[index * 4 + 3] / 255.;
        const double distance = std::lerp(
            std::hypot(appearance[0] - white[0], appearance[1], appearance[2]),
            std::hypot(appearance[0] - matte[0], appearance[1] - matte[1], appearance[2] - matte[2]),
            matte_visibility);
        const double difference = std::clamp((distance - .035) / .03, 0., 1.);
        const double match = 1 - difference * difference * (3 - 2 * difference);
        matte_match[index] = match;
        reference.has_artwork |= difference > 0;
        std::array<double, 3> color;
        for (int channel = 0; channel < 3; ++channel)
            color[channel] = pixels.pixels[index * 4 + channel] / 255.0;
        const double luminance = color[0] * 0.299 + color[1] * 0.587 + color[2] * 0.114;
        const double ink_scale = 1 / (1 + 0.65 * luminance);
        for (int channel = 0; channel < 3; ++channel) {
            const double art_ink = color[channel] * ink_scale;
            const double quiet_ink = .65 + .32 * (color[channel] - luminance);
            reference.ink_colors[index * 3 + channel] = static_cast<std::uint8_t>(std::nearbyint(255 * std::lerp(art_ink, quiet_ink, match)));
        }
        const double darkness = std::max(1 - luminance, *std::max_element(color.begin(), color.end()) - *std::min_element(color.begin(), color.end()));
        const double contrast = std::min(1., 1.15 * distance);
        const double tone = std::lerp(darkness, contrast, matte_contrast * matte_visibility);
        levels[index] = static_cast<std::uint8_t>(std::nearbyint(tone * (1 - match) * 255));
        ++histogram[levels[index]];
    }
    std::array<double, 256> counts{}, moments{};
    for (int i = 0; i < 256; ++i) {
        counts[i] = histogram[i] + (i ? counts[i - 1] : 0);
        moments[i] = histogram[i] * i + (i ? moments[i - 1] : 0);
    }
    int split = 0, threshold = 127;
    double best_separation = 0;
    for (int i = 0; i < 256; ++i) {
        const double pairs = counts[i] * (counts.back() - counts[i]);
        const double difference = moments.back() * counts[i] - moments[i] * counts.back();
        const double separation = pairs > 0 ? difference * difference / pairs : 0;
        if (separation > best_separation) {
            best_separation = separation;
            split = i;
        }
    }
    if (counts[split] > 0 && counts[split] < counts.back()) {
        const double light_mean = moments[split] / counts[split];
        const double dark_mean = (moments.back() - moments[split]) / (counts.back() - counts[split]);
        if (dark_mean - light_mean >= 8)
            threshold = std::clamp(split, 12, threshold);
    }
    for (int index = 0; index < size * size; ++index) {
        reference.target[index] = levels[index] > threshold;
        if (reference.target[index]) {
            for (int channel = 0; channel < 3; ++channel)
                reference.pixel_art.pixels[index * 4 + channel] = pixels.pixels[index * 4 + channel];
        }
    }
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            int low = 1, high = 0;
            for (int y = std::max(0, row - 1); y <= std::min(size - 1, row + 1); ++y) {
                for (int x = std::max(0, column - 1); x <= std::min(size - 1, column + 1); ++x) {
                    low = std::min(low, static_cast<int>(reference.target[y * size + x]));
                    high = std::max(high, static_cast<int>(reference.target[y * size + x]));
                }
            }
            const int index = row * size + column;
            reference.contours[index] = high != low;
            reference.weights[index] = 1 + 4 * reference.contours[index]
                + (reference.target[index] ? 0. : 2.5 * matte_match[index]);
        }
    }
    return reference;
}

std::vector<MosaicDesign> design_mosaics(const QrCode& qr, const MosaicReference* reference,
                                       std::uint32_t seed, int recovery_margin,
                                       bool color, double edit_fraction) {
    if (!(edit_fraction >= 0 && edit_fraction <= 1))
        throw std::invalid_argument("Artwork edit fraction must be between 0 and 1.");
    const int size = qr.size;
    const int cells = size * size;
    std::vector<std::uint8_t> writable(cells);
    for (int index = 0; index < cells; ++index)
        writable[index] = qr.roles[index] == ModuleRole::data || qr.roles[index] == ModuleRole::remainder;

    std::vector<MosaicDesign> candidates;
    candidates.reserve(4);
    if (!reference) {
        for (int turn = 0; turn < 4; ++turn) {
            MosaicDesign design;
            design.size = size;
            design.matrix.resize(cells);
            design.writable.resize(cells);
            design.colors.assign(cells * 3, 0);
            design.block_errors.assign(qr.block_capacities.size(), 0);
            design.block_capacities = qr.block_capacities;
            design.rotation = (360 - turn * 90) % 360;
            for (int row = 0; row < size; ++row) {
                for (int column = 0; column < size; ++column) {
                    int destination;
                    switch (turn) {
                    case 0: destination = row * size + column; break;
                    case 1: destination = column * size + size - 1 - row; break;
                    case 2: destination = (size - 1 - row) * size + size - 1 - column; break;
                    default: destination = (size - 1 - column) * size + row; break;
                    }
                    design.matrix[destination] = qr.matrix[row * size + column];
                    design.writable[destination] = writable[row * size + column];
                }
            }
            candidates.push_back(std::move(design));
        }
        return candidates;
    }
    if (reference->size != size)
        throw std::invalid_argument("Artwork dimensions must match the QR matrix.");

    const auto colors = color ? reference->ink_colors : std::vector<std::uint8_t>(cells * 3, 0);

    const std::size_t word_count = qr.codeword_blocks.size();
    std::vector<std::array<int, 8>> positions(word_count);
    std::size_t bit = 0;
    for (int right = size - 1; right > 0; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vertical = 0; vertical < size; ++vertical) {
            for (int offset = 0; offset < 2; ++offset) {
                const int column = right - offset;
                const bool upwards = ((right + 1) & 2) == 0;
                const int row = upwards ? size - 1 - vertical : vertical;
                if (qr.roles[row * size + column] == ModuleRole::data) {
                    positions.at(bit / 8)[bit % 8] = row * size + column;
                    ++bit;
                }
            }
        }
    }
    if (bit != word_count * 8)
        throw std::logic_error("QR codeword metadata does not match its placement.");
    NumPyPcg64 rng(seed);
    std::vector<double> ties(word_count);
    for (double& tie : ties)
        tie = rng.random();

    const int padding = static_cast<int>(std::bit_ceil(static_cast<unsigned int>(2 * size)));
    const int lag = std::min(size / 2, 32);
    const int offsets = 2 * lag + 1;
    const std::size_t real_count = static_cast<std::size_t>(padding) * padding;
    const std::size_t complex_count = static_cast<std::size_t>(padding) * (padding / 2 + 1);
    const pocketfft::shape_t shape{static_cast<std::size_t>(padding), static_cast<std::size_t>(padding)};
    const pocketfft::shape_t axes{0, 1};
    const pocketfft::stride_t real_stride{static_cast<std::ptrdiff_t>(padding * sizeof(double)), sizeof(double)};
    const pocketfft::stride_t complex_stride{static_cast<std::ptrdiff_t>((padding / 2 + 1) * sizeof(std::complex<double>)), sizeof(std::complex<double>)};
    std::vector<double> region_pixels(real_count), data_pixels(real_count), correlation(real_count);
    std::vector<std::complex<double>> region_fft(complex_count), data_fft(complex_count), product(complex_count);
    std::vector<double> pairs(offsets * offsets), products(offsets * offsets), left(offsets * offsets);

    for (int turn = 0; turn < 4; ++turn) {
        std::vector<int> rotation(cells);
        std::vector<std::uint8_t> target(cells);
        std::vector<std::uint8_t> contours(cells);
        std::vector<double> weights(cells);
        for (int row = 0; row < size; ++row) {
            for (int column = 0; column < size; ++column) {
                const int index = row * size + column;
                switch (turn) {
                case 0: rotation[index] = index; break;
                case 1: rotation[index] = column * size + size - 1 - row; break;
                case 2: rotation[index] = (size - 1 - row) * size + size - 1 - column; break;
                default: rotation[index] = (size - 1 - column) * size + row; break;
                }
                target[index] = reference->target[rotation[index]];
                contours[index] = reference->contours[rotation[index]];
                weights[index] = reference->weights[rotation[index]];
            }
        }
        std::vector<double> gains(word_count);
        std::vector<std::size_t> ranking(word_count);
        std::iota(ranking.begin(), ranking.end(), 0);
        for (std::size_t word = 0; word < word_count; ++word) {
            for (int index : positions[word]) {
                if (qr.matrix[index] != target[index])
                    gains[word] += weights[index];
            }
        }
        std::sort(ranking.begin(), ranking.end(), [&gains, &ties](std::size_t a, std::size_t b) {
            if (gains[a] != gains[b])
                return gains[a] > gains[b];
            if (ties[a] != ties[b])
                return ties[a] > ties[b];
            return a > b;
        });
        std::vector<std::uint8_t> matrix = qr.matrix;
        MosaicDesign design;
        design.size = size;
        design.colors = colors;
        design.block_capacities = qr.block_capacities;
        design.block_errors.assign(qr.block_capacities.size(), 0);
        for (std::size_t word : ranking) {
            const int block = qr.codeword_blocks[word];
            const int limit = static_cast<int>(std::max(0, qr.block_capacities[block] - recovery_margin) * edit_fraction);
            if (gains[word] > 0 && design.block_errors[block] < limit) {
                for (int index : positions[word])
                    matrix[index] = target[index];
                ++design.block_errors[block];
            }
        }
        double weighted_error = 0, weight_sum = 0;
        for (int index = 0; index < cells; ++index) {
            weighted_error += (matrix[index] != target[index]) * weights[index];
            weight_sum += weights[index];
            design.changed_modules += matrix[index] != qr.matrix[index];
        }
        design.visual_error = weighted_error / weight_sum;
        design.changed_codewords = std::accumulate(design.block_errors.begin(), design.block_errors.end(), 0);

        double balance_sum = 0;
        int balance_samples = 0;
        for (int tone = 0; tone <= 1; ++tone) {
            std::vector<std::uint8_t> region(cells);
            int sample_count = 0;
            for (int index = 0; index < cells; ++index) {
                region[index] = !contours[index] && writable[index] && target[index] == tone;
                sample_count += region[index];
            }
            if (sample_count < 24)
                continue;
            std::vector<int> window_counts;
            std::vector<double> densities;
            const int step = size / 4;
            int total_count = 0, total_errors = 0;
            for (int top = 0; top < size - 1; top += step) {
                for (int left_column = 0; left_column < size - 1; left_column += step) {
                    int count = 0, errors = 0;
                    for (int y = top; y < std::min(size, top + step + 1); ++y) {
                        for (int x = left_column; x < std::min(size, left_column + step + 1); ++x) {
                            const int index = y * size + x;
                            if (region[index]) {
                                ++count;
                                errors += matrix[index] != target[index];
                            }
                        }
                    }
                    if (count) {
                        window_counts.push_back(count);
                        densities.push_back(static_cast<double>(errors) / count);
                        total_count += count;
                        total_errors += errors;
                    }
                }
            }
            const double mean = static_cast<double>(total_errors) / total_count;
            for (std::size_t i = 0; i < densities.size(); ++i)
                balance_sum += window_counts[i] * (densities[i] - mean) * (densities[i] - mean);
            balance_samples += total_count;

            std::fill(region_pixels.begin(), region_pixels.end(), 0);
            std::fill(data_pixels.begin(), data_pixels.end(), 0);
            for (int row = 0; row < size; ++row) {
                for (int column = 0; column < size; ++column) {
                    const int index = row * size + column;
                    region_pixels[row * padding + column] = region[index];
                    data_pixels[row * padding + column] = region[index] * matrix[index];
                }
            }
            pocketfft::r2c(shape, real_stride, complex_stride, axes, true, region_pixels.data(), region_fft.data(), 1.0, 1);
            pocketfft::r2c(shape, real_stride, complex_stride, axes, true, data_pixels.data(), data_fft.data(), 1.0, 1);
            for (int statistic = 0; statistic < 3; ++statistic) {
                for (std::size_t i = 0; i < complex_count; ++i) {
                    if (statistic == 0)
                        product[i] = region_fft[i] * std::conj(region_fft[i]);
                    else if (statistic == 1)
                        product[i] = data_fft[i] * std::conj(data_fft[i]);
                    else
                        product[i] = data_fft[i] * std::conj(region_fft[i]);
                }
                pocketfft::c2r(shape, complex_stride, real_stride, axes, false, product.data(), correlation.data(), 1.0 / real_count, 1);
                for (int y = -lag; y <= lag; ++y) {
                    for (int x = -lag; x <= lag; ++x) {
                        const int index = (y + lag) * offsets + x + lag;
                        const double value = std::nearbyint(correlation[((y + padding) % padding) * padding + (x + padding) % padding]);
                        if (statistic == 0)
                            pairs[index] = value;
                        else if (statistic == 1)
                            products[index] = value;
                        else
                            left[index] = value;
                    }
                }
            }
            for (int i = 0; i < offsets * offsets; ++i) {
                if (i == lag * offsets + lag || pairs[i] < std::max(24.0, sample_count * 0.1))
                    continue;
                const double right = left[offsets * offsets - 1 - i];
                const double denominator = std::max(pairs[i], 1.0);
                const double covariance = products[i] - left[i] * right / denominator;
                const double variance = (left[i] - left[i] * left[i] / denominator) * (right - right * right / denominator);
                if (variance > 0)
                    design.texture_error = std::max(design.texture_error, covariance / std::sqrt(variance));
            }
        }
        design.balance_error = balance_samples ? std::sqrt(balance_sum / balance_samples) : 0;
        design.score = design.visual_error + 0.1 * design.texture_error + 0.2 * design.balance_error;
        design.rotation = (360 - turn * 90) % 360;
        design.matrix.resize(cells);
        design.writable.resize(cells);
        for (int index = 0; index < cells; ++index) {
            design.matrix[rotation[index]] = matrix[index];
            design.writable[rotation[index]] = writable[index];
        }
        candidates.push_back(std::move(design));
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const MosaicDesign& a, const MosaicDesign& b) { return a.score < b.score; });
    return candidates;
}

}
