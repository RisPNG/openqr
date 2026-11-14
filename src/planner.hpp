#pragma once

#include "image.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace openqr {

enum class ModuleRole : std::uint8_t { data, finder, separator, alignment, timing, format, version, dark, remainder };

struct QrCode {
    int version = 0;
    int size = 0;
    int mask = 0;
    char error_correction = 'H';
    std::vector<std::uint8_t> matrix;
    std::vector<ModuleRole> roles;
    std::vector<int> codeword_blocks;
    std::vector<int> block_capacities;
    int data_capacity_bits = 0;
};

struct MosaicReference {
    int size = 0;
    std::vector<std::uint8_t> colors;
    std::vector<std::uint8_t> target;
    std::vector<double> weights;
    Image pixel_art;
};

struct MosaicDesign {
    int size = 0;
    std::vector<std::uint8_t> matrix;
    std::vector<std::uint8_t> colors;
    std::vector<std::uint8_t> writable;
    std::vector<int> block_errors;
    std::vector<int> block_capacities;
    int changed_modules = 0;
    int changed_codewords = 0;
    int rotation = 0;
    double visual_error = 0;
    double texture_error = 0;
    double balance_error = 0;
    double score = 0;
};

QrCode encode_qr(std::span<const std::uint8_t> payload, int version, char error_correction, int mask = -1);
MosaicReference prepare_mosaic_reference(const Image& source, int size);
std::vector<MosaicDesign> design_mosaics(const QrCode& qr, const MosaicReference* reference,
                                       std::uint32_t seed, int recovery_margin,
                                       bool color, double edit_fraction);

}
