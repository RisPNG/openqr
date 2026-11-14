#pragma once

#include <openqr/openqr.h>

#include <cstdint>
#include <string>
#include <vector>

namespace openqr {

struct Generated {
    std::uint32_t version = 0;
    std::uint32_t seed = 0;
    openqr_mode mode = OPENQR_MOSAIC;
    openqr_format format = OPENQR_PNG;
    std::uint32_t verified_frames = 0;
    std::uint32_t total_frames = 0;
    std::uint32_t changed_codewords = 0;
    std::uint32_t layout_attempts = 0;
    char error_correction = 'H';
    double artwork_fraction = -1;
    double color_exposure = 1;
    std::vector<std::uint8_t> payload, image, preview, svg, target_preview;
    std::vector<std::int32_t> block_errors, block_capacities;
};

struct SkippedVersion { std::uint32_t version; std::string reason; };

struct Batch {
    std::uint32_t seed = 0;
    std::vector<Generated> entries;
    std::vector<SkippedVersion> skipped;
    std::vector<std::uint8_t> reference_preview, reference_input;
};

Batch generate(const openqr_request& request);

}
