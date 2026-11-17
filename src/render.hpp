#pragma once

#include "planner.hpp"

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace openqr {

struct Style {
    std::string_view id;
    std::string_view label;
};

class MosaicScene {
public:
    MosaicScene(const MosaicDesign& design, int scale = 12,
                std::string_view body_shape = "rounded",
                std::string_view eye_frame = "rounded-dots",
                std::string_view eye_ball = "dots");
    ~MosaicScene();
    Image rasterize() const;
    std::string svg() const;

private:
    struct Geometry;
    std::unique_ptr<Geometry> geometry_;
};

std::span<const Style> body_styles();
std::span<const Style> eye_frame_styles();
std::span<const Style> eye_ball_styles();
Image render_picture(const QrCode& qr, const Image& reference, int scale);
std::string render_style_preview(std::string_view category, std::string_view key);

}
