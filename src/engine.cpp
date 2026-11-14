#include "engine.hpp"
#include "image.hpp"
#include "planner.hpp"
#include "random.hpp"
#include "render.hpp"
#include "url_unicode.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif

#include <ReadBarcode.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <string_view>

namespace openqr {
namespace {

bool verify_frames(const Animation& animation, std::span<const std::uint8_t> payload) {
    for (const auto& frame : animation.frames) {
        bool verified = false;
        const ZXing::ImageView image(frame.image.pixels.data(), frame.image.width,
                                     frame.image.height, ZXing::ImageFormat::RGBA);
        for (auto binarizer : {ZXing::Binarizer::LocalAverage, ZXing::Binarizer::GlobalHistogram}) {
            auto options = ZXing::ReaderOptions().formats(ZXing::BarcodeFormat::QRCode).binarizer(binarizer);
            for (const auto& barcode : ZXing::ReadBarcodes(image, options)) {
                if (std::ranges::equal(barcode.bytes(), payload)) {
                    verified = true;
                    break;
                }
            }
            if (verified) break;
        }
        if (!verified) return false;
    }
    return !animation.frames.empty();
}

bool can_vary_url(std::span<const std::uint8_t> payload) {
    const std::string_view text(reinterpret_cast<const char*>(payload.data()), payload.size());
    if (text.empty() || text.find('#') != text.npos) return false;
    std::vector<std::pair<std::size_t, char32_t>> characters;
    for (std::size_t index = 0; index < payload.size();) {
        const std::size_t start = index;
        const auto first = payload[index++];
        char32_t point = first;
        int continuation = 0;
        char32_t minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { point = first & 31; continuation = 1; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { point = first & 15; continuation = 2; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { point = first & 7; continuation = 3; minimum = 0x10000; }
        else if (first >= 0x80) return false;
        for (int offset = 0; offset < continuation; ++offset) {
            if (index == payload.size() || (payload[index] & 0xc0) != 0x80) return false;
            point = (point << 6) | (payload[index++] & 63);
        }
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) return false;
        const auto forbidden = std::lower_bound(std::begin(url_disallowed), std::end(url_disallowed), point,
                                               [](const auto& range, auto value) { return range.second < value; });
        if (forbidden != std::end(url_disallowed) && forbidden->first <= point) return false;
        if (point < 128 && std::string_view{"\\<>\"{}|^`"}.find(static_cast<char>(point)) != text.npos) return false;
        if (point == '%') {
            if (index + 1 >= payload.size()) return false;
            for (int offset = 0; offset < 2; ++offset) {
                const auto digit = payload[index + offset];
                if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f') || (digit >= 'A' && digit <= 'F'))) return false;
            }
        }
        characters.emplace_back(start, point);
    }
    const auto scheme_end = text.find("://");
    bool explicit_url = scheme_end != text.npos && scheme_end > 0;
    if (explicit_url) {
        for (std::size_t index = 0; index < scheme_end; ++index) {
            const char character = text[index];
            const bool letter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
            if (!letter && (!index || !((character >= '0' && character <= '9') || character == '+' || character == '.' || character == '-'))) { explicit_url = false; break; }
        }
    }
    std::size_t start = 0;
    if (explicit_url) {
        std::string scheme(text.substr(0, scheme_end));
        for (auto& character : scheme) if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (scheme != "http" && scheme != "https") return false;
        start = scheme_end + 3;
    }
    const auto authority = text.substr(start, text.find_first_of("/?", start) - start);
    if (authority.empty() || authority.find('@') != authority.npos) return false;
    std::string_view host, port;
    bool has_port = false;
    if (authority.front() == '[') {
        const auto close = authority.find(']');
        if (close == authority.npos || !explicit_url) return false;
        host = authority.substr(1, close - 1);
        std::array<unsigned char, 16> address{};
        const std::string literal(host);
        if (inet_pton(AF_INET6, literal.c_str(), address.data()) != 1) return false;
        if (close + 1 < authority.size()) {
            if (authority[close + 1] != ':') return false;
            has_port = true;
            port = authority.substr(close + 2);
        }
    } else {
        if (authority.find_first_of("[]") != authority.npos) return false;
        const auto colon = authority.find(':');
        host = authority.substr(0, colon);
        if (colon != authority.npos) { has_port = true; port = authority.substr(colon + 1); }
    }
    if (host.empty()) return false;
    if (has_port) {
        if (port.empty()) return false;
        unsigned number = 0;
        for (char character : port) {
            if (character < '0' || character > '9') return false;
            number = number * 10 + static_cast<unsigned>(character - '0');
            if (number > 65535) return false;
        }
        if (number == 0) return false;
    }
    if (authority.front() == '[') return true;
    std::array<unsigned char, 4> address{};
    const std::string literal(host);
    if (inet_pton(AF_INET, literal.c_str(), address.data()) == 1) return explicit_url;
    std::vector<std::vector<char32_t>> labels(1);
    const auto host_start = static_cast<std::size_t>(host.data() - text.data());
    for (const auto [offset, point] : characters) {
        if (offset < host_start || offset >= host_start + host.size()) continue;
        if (point == '.' || point == 0x3002 || point == 0xff0e || point == 0xff61) labels.emplace_back();
        else labels.back().push_back(point >= 'A' && point <= 'Z' ? point + 'a' - 'A' : point);
    }
    if (labels.size() > 1 && labels.back().empty()) labels.pop_back();
    std::size_t hostname_length = labels.size() - 1;
    bool last_alphabetic = false, last_ace = false;
    for (auto label : labels) {
        if (label.empty()) return false;
        const bool ascii = std::ranges::all_of(label, [](auto point) { return point < 128; });
        if (label.front() == '-' || label.back() == '-') return false;
        for (auto point : label)
            if (point < 128 && !((point >= 'a' && point <= 'z') || (point >= '0' && point <= '9') || point == '-')) return false;
        const bool ace = label.size() > 4 && label[0] == 'x' && label[1] == 'n' && label[2] == '-' && label[3] == '-';
        if (ace) {
            std::vector<char32_t> decoded;
            std::size_t position = 4;
            const auto delimiter = std::find(label.rbegin(), label.rend(), U'-');
            if (delimiter != label.rend()) {
                const auto index = label.size() - 1 - static_cast<std::size_t>(delimiter - label.rbegin());
                if (index >= 4) { decoded.assign(label.begin() + 4, label.begin() + index); position = index + 1; }
            }
            std::uint64_t codepoint = 128, insertion = 0, bias = 72;
            while (position < label.size()) {
                const auto previous = insertion;
                std::uint64_t weight = 1;
                for (std::uint64_t step = 36;; step += 36) {
                    if (position == label.size()) return false;
                    const auto character = label[position++];
                    std::uint64_t digit;
                    if (character >= 'a' && character <= 'z') digit = character - 'a';
                    else if (character >= '0' && character <= '9') digit = character - '0' + 26;
                    else return false;
                    if (weight > 0x10ffffull * (label.size() + 1) || digit * weight > std::numeric_limits<std::uint64_t>::max() - insertion) return false;
                    insertion += digit * weight;
                    const auto threshold = step <= bias ? 1ull : step >= bias + 26 ? 26ull : step - bias;
                    if (digit < threshold) break;
                    weight *= 36 - threshold;
                }
                auto delta = insertion - previous;
                delta = previous == 0 ? delta / 700 : delta / 2;
                delta += delta / (decoded.size() + 1);
                bias = 0;
                while (delta > 455) { delta /= 35; bias += 36; }
                bias += 36 * delta / (delta + 38);
                codepoint += insertion / (decoded.size() + 1);
                insertion %= decoded.size() + 1;
                if (codepoint > 0x10ffff) return false;
                const auto forbidden = std::lower_bound(std::begin(url_disallowed), std::end(url_disallowed), codepoint,
                                                       [](const auto& range, auto value) { return range.second < value; });
                if (forbidden != std::end(url_disallowed) && forbidden->first <= codepoint) return false;
                decoded.insert(decoded.begin() + insertion, static_cast<char32_t>(codepoint));
                ++insertion;
            }
            if (decoded.empty()) return false;
        }
        std::size_t encoded_length = label.size();
        if (!ascii) {
            const auto basic = std::ranges::count_if(label, [](auto point) { return point < 128; });
            std::size_t handled = static_cast<std::size_t>(basic);
            encoded_length = 4 + handled + (handled != 0);
            std::uint64_t next = 128, delta = 0, bias = 72;
            while (handled < label.size()) {
                char32_t minimum = 0x10ffff;
                for (auto point : label) if (point >= next && point < minimum) minimum = point;
                delta += (minimum - next) * (handled + 1);
                next = minimum;
                for (auto point : label) {
                    if (point < next) ++delta;
                    if (point != next) continue;
                    auto value = delta;
                    for (std::uint64_t step = 36;; step += 36) {
                        const auto threshold = step <= bias ? 1ull : step >= bias + 26 ? 26ull : step - bias;
                        if (value < threshold) break;
                        ++encoded_length;
                        value = (value - threshold) / (36 - threshold);
                    }
                    ++encoded_length;
                    delta = handled == static_cast<std::size_t>(basic) ? delta / 700 : delta / 2;
                    delta += delta / (handled + 1);
                    bias = 0;
                    while (delta > 455) { delta /= 35; bias += 36; }
                    bias += 36 * delta / (delta + 38);
                    delta = 0;
                    ++handled;
                }
                ++delta;
                ++next;
            }
        }
        if (encoded_length > 63) return false;
        hostname_length += encoded_length;
        last_alphabetic = std::ranges::all_of(label, [](auto point) {
            const auto range = std::lower_bound(std::begin(url_alphabetic), std::end(url_alphabetic), point,
                                               [](const auto& interval, auto value) { return interval.second < value; });
            return range != std::end(url_alphabetic) && range->first <= point;
        });
        last_ace = ace || !ascii;
    }
    if (hostname_length > 253) return false;
    if (labels.size() == 1) {
        const bool decimal = std::ranges::all_of(labels.front(), [](auto point) {
            const auto range = std::lower_bound(std::begin(url_decimal), std::end(url_decimal), point,
                                               [](const auto& interval, auto value) { return interval.second < value; });
            return range != std::end(url_decimal) && range->first <= point;
        });
        if (decimal) return false;
        if (!explicit_url && labels.front() != std::vector<char32_t>{'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't'}) return false;
    } else if (!last_alphabetic && !last_ace) return false;
    return true;
}

struct Layout {
    QrCode qr;
    std::optional<MosaicDesign> design;
};

}

Batch generate(const openqr_request& request) {
    const auto& options = request.options;
    if (!request.payload.data || !request.payload.size || request.payload.size > 16384 ||
        std::all_of(request.payload.data, request.payload.data + request.payload.size,
                    [](auto c) { return c == ' ' || (c >= 9 && c <= 13); }))
        throw std::invalid_argument("Enter between 1 and 16384 bytes of text or a URL to encode.");
    if ((request.reference.size && !request.reference.data) || (request.background.size && !request.background.data))
        throw std::invalid_argument("Image bytes require a valid input pointer.");
    if (request.reference.data && !request.reference.size) throw std::invalid_argument("The reference image is empty.");
    if (request.background.data && !request.background.size) throw std::invalid_argument("The background image is empty.");
    if (request.background.data && !request.reference.data) throw std::invalid_argument("Add a reference image before choosing a background image.");
    if (options.version > 40) throw std::invalid_argument("QR version must be between 1 and 40, or zero for all versions.");
    if (options.mode != OPENQR_MOSAIC && options.mode != OPENQR_PICTURE) throw std::invalid_argument("Choose mosaic or picture mode.");
    if (options.scale < 3 || options.scale > 24 || options.scale % 3) throw std::invalid_argument("Scale must be a multiple of 3 between 3 and 24.");
    if (!options.recovery_margin || options.recovery_margin > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) throw std::invalid_argument("Recovery margin must be a positive whole number.");
    if (!std::isfinite(options.contrast) || options.contrast < .1 || options.contrast > 3) throw std::invalid_argument("Contrast must be between 0.1 and 3.");
    if (!std::isfinite(options.brightness) || options.brightness < .1 || options.brightness > 3) throw std::invalid_argument("Brightness must be between 0.1 and 3.");
    if ((options.background_rgba & 255) != 0 && (options.background_rgba & 255) != 255) throw std::invalid_argument("Use a solid background color or transparent.");
    if (options.flags & ~(OPENQR_COLOR | OPENQR_VARY_URL | OPENQR_SEED_SET | OPENQR_RANDOMIZE_SEED)) throw std::invalid_argument("Unknown OpenQR option flags.");
    const std::array selected_styles{options.body_shape, options.eye_frame, options.eye_ball};
    const std::array catalogs{body_styles(), eye_frame_styles(), eye_ball_styles()};
    for (std::size_t i = 0; i < catalogs.size(); ++i) {
        if (!selected_styles[i] || std::ranges::none_of(catalogs[i], [&](const Style& style) { return style.id == selected_styles[i]; }))
            throw std::invalid_argument("Choose a valid body, eye frame, and eye center style.");
    }

    Batch batch;
    batch.seed = options.seed;
    const bool seed_set = (options.flags & OPENQR_SEED_SET) != 0;
    const bool randomize = (options.flags & OPENQR_RANDOMIZE_SEED) != 0;
    if (!seed_set || randomize) {
        std::random_device entropy;
        do { batch.seed = entropy(); } while (seed_set && randomize && batch.seed == options.seed);
    }
    const bool color = (options.flags & OPENQR_COLOR) != 0;
    const std::span<const std::uint8_t> payload(request.payload.data, request.payload.size);
    const bool vary_url = (options.flags & OPENQR_VARY_URL) && can_vary_url(payload);
    Animation reference;
    if (request.reference.data) {
        reference = decode_image({request.reference.data, request.reference.size}, {.normalize_animation = true});
        if (options.mode == OPENQR_MOSAIC && reference.frames.size() > 1) throw std::invalid_argument("Use picture mode for an animated image.");
        Animation background;
        if (request.background.data) {
            background = decode_image({request.background.data, request.background.size}, {.still_only = true});
        }
        auto normalized = normalize_reference(reference);
        batch.reference_input = normalized.frames.size() > 1 ? encode_apng(normalized) : encode_png(normalized.frames.front().image);
        reference = prepare_reference(normalized, background.frames.empty() ? nullptr : &background.frames.front().image,
                                      options.background_rgba, color, options.contrast, options.brightness);
        batch.reference_preview = encode_png(resize_image(reference.frames.front().image, 256, 256));
    }
    const bool mosaic_rendering = reference.frames.empty() || options.mode == OPENQR_MOSAIC;
    if (!mosaic_rendering && reference.frames.size() > 1) reference = decode_image(encode_gif(reference), {.input_limits = false});
    const int first_version = options.version ? static_cast<int>(options.version) : 1;
    const int last_version = options.version ? first_version : 40;
    for (int version = first_version; version <= last_version; ++version) {
        const int side = (25 + 4 * version) * static_cast<int>(options.scale);
        if (!mosaic_rendering && reference.frames.size() * static_cast<std::size_t>(side) * side > 100000000) {
            batch.skipped.push_back({static_cast<std::uint32_t>(version), "The animation exceeds the rendering memory limit at this size."});
            continue;
        }
        std::optional<MosaicReference> mosaic_reference;
        std::vector<std::uint8_t> target_preview;
        if (!reference.frames.empty() && mosaic_rendering) {
            mosaic_reference = prepare_mosaic_reference(reference.frames.front().image, 17 + 4 * version);
            target_preview = encode_png(mosaic_reference->pixel_art);
        }
        std::uint32_t attempts = 0;
        bool accepted = false;
        for (const char correction : {'H', 'Q', 'M', 'L'}) {
            QrCode base;
            try { base = encode_qr(payload, version, correction); }
            catch (const std::length_error&) { continue; }
            if (base.block_capacities.empty() || *std::ranges::min_element(base.block_capacities) <= static_cast<int>(options.recovery_margin)) continue;
            std::uint32_t version_seed = batch.seed;
            CPythonRandom version_rng(static_cast<std::uint64_t>(batch.seed) ^ (static_cast<std::uint64_t>(version) << 32));
            std::set<std::uint32_t> used_seeds{batch.seed};
            const std::array<double, 5> fractions{1, 1, .75, .5, 0};
            const int rounds = mosaic_reference || vary_url ? 5 : 1;
            for (int round = 0; round < rounds && !accepted; ++round) {
                const double fraction = fractions[round];
                CPythonRandom rng(version_seed);
                std::vector<std::uint8_t> encoded_payload(payload.begin(), payload.end());
                if (vary_url) {
                    const int count_bits = version < 10 ? 8 : 16;
                    const int eci_bits = std::ranges::any_of(payload, [](auto c) { return c >= 128; }) ? 12 : 0;
                    const int suffix_size = (base.data_capacity_bits - 4 - count_bits - eci_bits) / 8 - static_cast<int>(payload.size()) - 1;
                    if (suffix_size > 0) {
                        constexpr std::string_view alphabet = "abcdefghijklmnopqrstuvwxyz0123456789";
                        encoded_payload.push_back('#');
                        for (int i = 0; i < suffix_size; ++i) encoded_payload.push_back(alphabet[static_cast<std::size_t>(rng.random() * alphabet.size())]);
                    }
                }
                const auto qr = encoded_payload.size() == payload.size() ? base : encode_qr(encoded_payload, version, correction);
                std::vector<int> masks(8);
                std::iota(masks.begin(), masks.end(), 0);
                rng.shuffle(masks);
                if (!mosaic_rendering) {
                    masks.erase(std::find(masks.begin(), masks.end(), qr.mask));
                    masks.insert(masks.begin(), qr.mask);
                }
                std::vector<Layout> layouts;
                for (int mask : masks) {
                    auto candidate = mask == qr.mask ? qr : encode_qr(encoded_payload, version, correction, mask);
                    if (mosaic_rendering) {
                        for (auto& design : design_mosaics(candidate, mosaic_reference ? &*mosaic_reference : nullptr,
                                                          version_seed, static_cast<int>(options.recovery_margin), color, fraction))
                            layouts.push_back({candidate, std::move(design)});
                    } else layouts.push_back({std::move(candidate), std::nullopt});
                }
                if (mosaic_rendering) std::stable_sort(layouts.begin(), layouts.end(), [](const auto& left, const auto& right) { return left.design->score < right.design->score; });
                std::vector<std::uint8_t> original_colors, previous_colors;
                if (mosaic_rendering && !layouts.empty()) original_colors = previous_colors = layouts.front().design->colors;
                const bool expose_colors = std::ranges::any_of(original_colors, [](auto value) { return value != 0; });
                for (int level = 20; level >= 0 && !accepted; --level) {
                    if (level != 20 && !expose_colors) break;
                    const double exposure = level / 20.;
                    std::vector<std::uint8_t> colors;
                    if (level != 20) {
                        colors.reserve(original_colors.size());
                        for (auto channel : original_colors) {
                            double value = channel / 255.;
                            value = (value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4)) * exposure;
                            value = value <= .0031308 ? value * 12.92 : 1.055 * std::pow(value, 1 / 2.4) - .055;
                            colors.push_back(static_cast<std::uint8_t>(std::nearbyint(std::clamp(value, 0., 1.) * 255)));
                        }
                        if (colors == previous_colors) continue;
                        previous_colors = colors;
                    }
                    for (auto& layout : layouts) {
                        ++attempts;
                        Animation rendered;
                        std::string svg;
                        rendered.loop = reference.loop;
                        if (layout.design) {
                            if (level != 20) layout.design->colors = colors;
                            auto mosaic = render_mosaic(*layout.design, static_cast<int>(options.scale), options.body_shape, options.eye_frame, options.eye_ball);
                            rendered.frames.push_back({std::move(mosaic.image), 100});
                            svg = std::move(mosaic.svg);
                        } else {
                            for (const auto& frame : reference.frames) rendered.frames.push_back({render_picture(layout.qr, frame.image, static_cast<int>(options.scale)), frame.duration_ms});
                        }
                        const bool animated = rendered.frames.size() > 1;
                        auto image = animated ? encode_gif(rendered) : encode_png(rendered.frames.front().image);
                        auto serialized = decode_image(image, {.input_limits = false});
                        if (!verify_frames(serialized, encoded_payload)) continue;
                        const auto frame_count = static_cast<std::uint32_t>(serialized.frames.size());
                        const int preview_side = std::min(side, std::max(360, (25 + 4 * version) * 2));
                        for (auto& frame : serialized.frames) frame.image = resize_image(frame.image, preview_side, preview_side);
                        Generated entry;
                        entry.version = version;
                        entry.seed = version_seed;
                        entry.mode = options.mode;
                        entry.format = animated ? OPENQR_GIF : OPENQR_PNG;
                        entry.verified_frames = entry.total_frames = frame_count;
                        entry.layout_attempts = attempts;
                        entry.error_correction = correction;
                        entry.artwork_fraction = mosaic_reference ? fraction : -1;
                        entry.color_exposure = exposure;
                        entry.payload = encoded_payload;
                        entry.image = std::move(image);
                        entry.preview = animated ? encode_gif(serialized) : encode_png(serialized.frames.front().image);
                        entry.svg.assign(svg.begin(), svg.end());
                        entry.target_preview = target_preview;
                        entry.block_capacities.assign(layout.qr.block_capacities.begin(), layout.qr.block_capacities.end());
                        if (layout.design) {
                            entry.block_errors.assign(layout.design->block_errors.begin(), layout.design->block_errors.end());
                            entry.changed_codewords = layout.design->changed_codewords;
                        } else entry.block_errors.resize(entry.block_capacities.size(), 0);
                        batch.entries.push_back(std::move(entry));
                        accepted = true;
                        break;
                    }
                }
                version_seed = version_rng.getrandbits32();
                while (used_seeds.contains(version_seed) || (randomize && seed_set && version_seed == options.seed)) ++version_seed;
                used_seeds.insert(version_seed);
            }
            if (accepted) break;
        }
        if (!accepted) batch.skipped.push_back({static_cast<std::uint32_t>(version), attempts ?
            "No verified layout was found after " + std::to_string(attempts) + " attempts with the selected design." :
            "The content and recovery reserve do not fit this version at any error correction level."});
    }
    return batch;
}

}
