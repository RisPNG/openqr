#include "planner.hpp"
#include "random.hpp"
#include "planner_vectors.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

template<class T> std::uint64_t digest(const T& values) {
    std::uint64_t result = 14695981039346656037ULL;
    for (const auto value : values)
        result = (result ^ static_cast<std::uint64_t>(value)) * 1099511628211ULL;
    return result;
}

std::span<const std::uint8_t> bytes(std::string_view value) {
    return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

}

int main() {
    try {
        for (const auto& expected : planner_vectors::python_random) {
            openqr::CPythonRandom rng(expected.seed);
            require(rng.random() == expected.first, "CPython initial random draw differs");
            require(rng.getrandbits32() == expected.word, "CPython getrandbits differs");
            double last = 0;
            for (int i = 0; i < 1000; ++i)
                last = rng.random();
            require(last == expected.thousandth, "CPython state transition differs");
            std::array<int, 8> values{0, 1, 2, 3, 4, 5, 6, 7};
            rng.shuffle(values);
            require(values == expected.shuffled, "CPython shuffle differs");
        }
        for (const auto& expected : planner_vectors::numpy_random) {
            openqr::NumPyPcg64 rng(expected.seed);
            for (int i = 0; i < 5; ++i)
                require(rng.random() == expected.samples[i], "NumPy PCG64 initial draws differ");
            for (int i = 0; i < 1000; ++i)
                rng.random();
            require(rng.random() == expected.samples[5], "NumPy PCG64 state transition differs");
        }

        for (const auto& expected : planner_vectors::encodings) {
            const auto qr = openqr::encode_qr(bytes("QR"), expected.version, expected.correction, 5);
            const std::string description = "version " + std::to_string(expected.version) + " ECC " + expected.correction;
            require(digest(qr.matrix) == expected.matrix, "Segno fixed-mask matrix differs: " + description);
            require(digest(qr.codeword_blocks) == expected.blocks, "RS block ownership differs: " + description);
            require(digest(qr.block_capacities) == expected.capacities, "RS block capacities differ: " + description);
            require(qr.data_capacity_bits == expected.bits, "QR payload capacity differs: " + description);
            require(std::count(qr.roles.begin(), qr.roles.end(), openqr::ModuleRole::data) == qr.codeword_blocks.size() * 8,
                    "Codeword placement includes protected or remainder modules: " + description);
            require(std::count(qr.roles.begin(), qr.roles.end(), openqr::ModuleRole::finder) == 147,
                    "Finder module roles differ: " + description);
            require(qr.roles[(qr.size - 8) * qr.size + 8] == openqr::ModuleRole::dark,
                    "Fixed dark module role differs: " + description);
            const auto plain = openqr::design_mosaics(qr, nullptr, 42, 2, true, 1);
            require(plain.size() == 4 && plain[0].matrix == qr.matrix, "Plain QR must retain the canonical orientation first");
            for (const auto& design : plain) {
                require(design.changed_modules == 0 && design.changed_codewords == 0 && design.score == 0,
                        "Plain QR must not consume its recovery reserve");
                require(std::all_of(design.colors.begin(), design.colors.end(), [](auto channel) { return channel == 0; }),
                        "Plain QR palette must be black");
            }
        }
        for (const auto& expected : planner_vectors::payload_encodings) {
            const auto qr = openqr::encode_qr(bytes(expected.payload), expected.version, expected.correction, 5);
            require(digest(qr.matrix) == expected.matrix,
                    "Literal bytes, text mode, or UTF-8 ECI differs at version " + std::to_string(expected.version) + " ECC " + expected.correction);
        }

        constexpr int size = 73;
        openqr::Image image{size, size, std::vector<std::uint8_t>(size * size * 4, 255)};
        for (int row = 0; row < size; ++row) {
            for (int column = 0; column < size; ++column) {
                const int index = (row * size + column) * 4;
                if (row >= 8 && row < 65 && column >= 7 && column < 54) {
                    image.pixels[index] = 10 + row % 4;
                    image.pixels[index + 1] = 102;
                    image.pixels[index + 2] = 255;
                }
                if (row >= 20 && row < 54 && column >= 26 && column < 39) {
                    image.pixels[index] = 255;
                    image.pixels[index + 1] = 255;
                    image.pixels[index + 2] = 255;
                }
            }
        }
        const auto reference = openqr::prepare_mosaic_reference(image, size);
        require(digest(reference.target) == planner_vectors::target, "Artwork darkness/threshold target differs");
        require(digest(reference.weights) == planner_vectors::weights, "Artwork contour weighting differs");
        const auto qr = openqr::encode_qr(bytes("https://example.com/#keep"), 14, 'H', 5);
        const auto canonical_hash = digest(qr.matrix);
        const auto target_hash = digest(reference.target);
        const auto designs = openqr::design_mosaics(qr, &reference, 42, 2, true, 1);
        require(designs.size() == 4, "All four artistic orientations must remain available");
        for (std::size_t i = 0; i < designs.size(); ++i) {
            const auto& actual = designs[i];
            const auto& expected = planner_vectors::designs[i];
            require(actual.rotation == expected.rotation, "Artistic orientation ranking differs");
            require(digest(actual.matrix) == expected.matrix, "Seeded weighted codeword edits differ");
            require(digest(actual.block_errors) == expected.errors, "Per-block artistic edit accounting differs");
            require(actual.changed_modules == expected.changed_modules && actual.changed_codewords == expected.changed_words,
                    "Artistic damage metadata differs");
            require(std::abs(actual.visual_error - expected.visual) < 1e-12, "Weighted visual score differs");
            require(std::abs(actual.texture_error - expected.texture) < 1e-12, "FFT texture score differs");
            require(std::abs(actual.balance_error - expected.balance) < 1e-12, "Tone-specific density balance differs");
            require(std::abs(actual.score - expected.score) < 1e-12, "Combined artistic ranking score differs");
            for (std::size_t block = 0; block < actual.block_errors.size(); ++block)
                require(actual.block_errors[block] <= actual.block_capacities[block] - 2, "Artistic edits consumed the recovery reserve");
        }
        require(digest(qr.matrix) == canonical_hash && digest(reference.target) == target_hash,
                "Planning mutated its QR or shared reference");
        for (double fraction : {0.0, 0.5}) {
            const auto reduced = openqr::design_mosaics(qr, &reference, 42, 2, true, fraction);
            for (const auto& design : reduced) {
                for (std::size_t block = 0; block < design.block_errors.size(); ++block)
                    require(design.block_errors[block] <= static_cast<int>((design.block_capacities[block] - 2) * fraction),
                            "Reduced artwork did not reduce the per-block codeword budget");
                if (fraction == 0)
                    require(design.changed_codewords == 0 && design.changed_modules == 0, "Zero artwork fraction changed QR modules");
                require(design.colors == designs.front().colors, "Artwork reduction changed original ink colors");
            }
        }
        for (std::uint8_t gray : {127, 128}) {
            openqr::Image flat{21, 21, std::vector<std::uint8_t>(21 * 21 * 4, 255)};
            for (int index = 0; index < 21 * 21; ++index) {
                for (int channel = 0; channel < 3; ++channel)
                    flat.pixels[index * 4 + channel] = gray;
            }
            const auto prepared = openqr::prepare_mosaic_reference(flat, 21);
            require(std::all_of(prepared.target.begin(), prepared.target.end(), [gray](auto value) { return value == (gray == 127); }),
                    "Flat grayscale threshold introduced dithering or changed the 127/128 boundary");
        }
        std::cout << "Planner oracle vectors passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
