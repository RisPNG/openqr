#include "render.hpp"

#include <blend2d/blend2d.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <new>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace openqr {
namespace {

constexpr Style bodies[] = {
    {"square", "Square"}, {"rounded", "Rounded dots"}, {"circle", "Circle"},
    {"small-dots", "Small dots"}, {"random-dots", "Variable dots"},
    {"diamond", "Diamond"}, {"horizontal", "Horizontal"}, {"vertical", "Vertical"},
    {"connected-rounded", "Connected rounded"}, {"mosaic", "Mosaic"},
    {"classy", "Classy corner"}, {"leaf", "Leaf"}, {"teardrop", "Teardrop"},
    {"cut-corners", "Cut corners"}, {"kite", "Kite"}, {"diagonal", "Diagonal"},
    {"triangle", "Triangle"}, {"star", "Star"}, {"cross", "Cross"},
    {"inset-square", "Inset square"}, {"horizontal-bars", "Joined horizontal"},
    {"vertical-bars", "Joined vertical"}
};
constexpr Style frames[] = {
    {"follow-body", "Follow body"},
    {"rounded-dots", "Rounded modules"}, {"square", "Square"}, {"rounded", "Rounded"},
    {"circle", "Circle"}, {"rounded-top-left", "Rounded top left"},
    {"rounded-top-right", "Rounded top right"}, {"rounded-bottom-left", "Rounded bottom left"},
    {"rounded-bottom-right", "Rounded bottom right"}, {"leaf", "Leaf"},
    {"leaf-reverse", "Reverse leaf"}, {"octagon", "Octagon"}, {"dots", "Beaded"},
    {"dashed", "Dashed"}, {"slanted", "Slanted"}, {"concave", "Concave"},
    {"scalloped", "Scalloped"}
};
constexpr Style balls[] = {
    {"follow-body", "Follow body"},
    {"square", "Square"}, {"rounded", "Rounded"}, {"circle", "Circle"},
    {"dots", "Rounded dots"}, {"small-dots", "Linked dots"}, {"leaf", "Leaf"},
    {"leaf-reverse", "Reverse leaf"}, {"rounded-top-left", "Rounded top left"},
    {"rounded-bottom-right", "Rounded bottom right"}, {"octagon", "Octagon"},
    {"diamond", "Diamond"}, {"vertical-bars", "Vertical bars"},
    {"horizontal-bars", "Horizontal bars"}, {"slanted", "Slanted"},
    {"diagonal-cut", "Diagonal cut"}, {"concave", "Concave"},
    {"soft-star", "Soft star"}, {"cross", "Cross"}
};

struct Part {
    enum class Kind { rectangle, ellipse, polygon } kind;
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;
    double radius = 0;
    int corners = 15;
    std::vector<BLPoint> points;
};

struct Primitive {
    std::string_view shape;
    int column;
    int row;
    int side;
    std::array<std::uint8_t, 3> color;
    int corners;
    std::size_t geometry = 0;
};

struct Shape {
    int side;
    std::vector<Part> parts;
};

std::vector<Part> shape_geometry(std::string_view shape, int side, int corners, int size) {
    std::vector<Part> parts;
    if (((shape == "circle" || shape == "small-dots" || shape == "random-dots") && side == 1) || shape == "circle") {
        double diameter = side;
        if (shape == "small-dots") diameter *= 0.65;
        else if (shape == "random-dots") diameter *= 0.65 + corners / 50.0;
        const double margin = (side - diameter) / 2;
        parts.push_back({Part::Kind::ellipse, margin, margin, diameter, diameter});
    } else if (shape == "small-dots") {
        for (int offset = 0; offset < 3; ++offset) {
            parts.push_back({Part::Kind::rectangle, 0.5, offset + 0.1, 2, 0.8});
            parts.push_back({Part::Kind::rectangle, offset + 0.1, 0.5, 0.8, 2});
        }
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                parts.push_back({Part::Kind::ellipse, double(x), double(y), 1, 1});
    } else if (shape == "frame-dots" || shape == "frame-dashed") {
        if (shape == "frame-dashed") {
            parts.push_back({Part::Kind::rectangle, 0.25, 0.25, side - 0.5, 0.5});
            parts.push_back({Part::Kind::rectangle, 0.25, side - 0.75, side - 0.5, 0.5});
            parts.push_back({Part::Kind::rectangle, 0.25, 0.25, 0.5, side - 0.5});
            parts.push_back({Part::Kind::rectangle, side - 0.75, 0.25, 0.5, side - 0.5});
        }
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 7; ++x) {
                if (x != 0 && x != 6 && y != 0 && y != 6) continue;
                if (shape == "frame-dots")
                    parts.push_back({Part::Kind::ellipse, x * (side - 1.2) / 6, y * (side - 1.2) / 6, 1.2, 1.2});
                else
                    parts.push_back({Part::Kind::rectangle, x + 0.05, y + 0.05, 0.9, 0.9, 0.08});
            }
        }
    } else if ((shape == "horizontal-bars" || shape == "vertical-bars") && side == 3) {
        for (int offset = 0; offset < 3; ++offset) {
            if (shape == "horizontal-bars")
                parts.push_back({Part::Kind::rectangle, 0, offset + 0.05, 3, 0.9, 0.45});
            else
                parts.push_back({Part::Kind::rectangle, offset + 0.05, 0, 0.9, 3, 0.45});
        }
        if (shape == "horizontal-bars") parts.push_back({Part::Kind::rectangle, 1.35, 0, 0.3, 3});
        else parts.push_back({Part::Kind::rectangle, 0, 1.35, 3, 0.3});
    } else if (shape == "diamond" || shape == "cut-corners" || shape == "octagon" || shape == "kite" || shape == "diagonal" || shape == "slanted" || shape == "triangle" || shape == "star" || shape == "soft-star" || shape == "cross" || shape == "mosaic" || shape == "diagonal-cut" || shape == "concave" || shape == "scalloped") {
        Part polygon{Part::Kind::polygon};
        auto& points = polygon.points;
        if (shape == "diamond") points = {{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}};
        else if (shape == "cut-corners" || shape == "octagon") {
            const double cut = shape == "cut-corners" ? 0.2 : 0.14;
            points = {{cut, 0}, {1 - cut, 0}, {1, cut}, {1, 1 - cut}, {1 - cut, 1}, {cut, 1}, {0, 1 - cut}, {0, cut}};
        } else if (shape == "kite") points = {{0.5, 0}, {1, 0.62}, {0.5, 1}, {0, 0.62}};
        else if (shape == "diagonal" || shape == "slanted") {
            const double slant = side == 1 ? 0.22 : side >= 5 ? 0.075 : 0.1;
            points = {{slant, 0}, {1, 0}, {1 - slant, 1}, {0, 1}};
        } else if (shape == "triangle") points = {{0.5, 0}, {1, 1}, {0, 1}};
        else if (shape == "soft-star") points = {{0, 0}, {0.5, 0.18}, {1, 0}, {0.82, 0.5}, {1, 1}, {0.5, 0.82}, {0, 1}, {0.18, 0.5}};
        else if (shape == "star") {
            constexpr double inset = 0.35;
            points = {{0.5, 0}, {1 - inset, inset}, {1, 0.5}, {1 - inset, 1 - inset}, {0.5, 1}, {inset, 1 - inset}, {0, 0.5}, {inset, inset}};
        } else if (shape == "cross") {
            constexpr double inset = 0.23;
            points = {{inset, 0}, {1 - inset, 0}, {1 - inset, inset}, {1, inset}, {1, 1 - inset}, {1 - inset, 1 - inset}, {1 - inset, 1}, {inset, 1}, {inset, 1 - inset}, {0, 1 - inset}, {0, inset}, {inset, inset}};
        } else if (shape == "mosaic") {
            const double angle = (-12 + corners * 24.0 / 15) * std::numbers::pi / 180;
            const double cosine = std::cos(angle), sine = std::sin(angle);
            for (const auto point : {BLPoint{-0.4, -0.4}, BLPoint{0.4, -0.4}, BLPoint{0.4, 0.4}, BLPoint{-0.4, 0.4}})
                points.push_back({0.5 + point.x * cosine - point.y * sine, 0.5 + point.x * sine + point.y * cosine});
        } else if (shape == "diagonal-cut") points = {{0.3, 0}, {1, 0}, {1, 0.7}, {0.7, 1}, {0, 1}, {0, 0.3}};
        else if (shape == "concave") {
            const double inset = side >= 5 ? 0.015 : 0.06;
            points = {{0, 0}, {0.5, inset}, {1, 0}, {1 - inset, 0.5}, {1, 1}, {0.5, 1 - inset}, {0, 1}, {inset, 0.5}};
        } else {
            for (int edge = 0; edge < 4; ++edge) {
                for (int index = 0; index < 57; ++index) {
                    const double position = index / 56.0;
                    const double sine = std::sin(position * std::numbers::pi * 7);
                    BLPoint point{position, 0.025 * sine * sine};
                    for (int turn = 0; turn < edge; ++turn) point = {1 - point.y, point.x};
                    points.push_back(point);
                }
            }
        }
        for (auto& point : points) { point.x *= side; point.y *= side; }
        parts.push_back(std::move(polygon));
    } else {
        Part rectangle{Part::Kind::rectangle, 0, 0, double(side), double(side), 0, corners};
        if (shape == "rounded") rectangle.radius = side * (side == 1 ? (size >= 21 && size < 37 ? 0.1 : 0.2) : 0.25);
        else if (shape == "connected-rounded") rectangle.radius = side / 3.0;
        else if (shape == "classy" || shape == "leaf" || shape == "leaf-reverse" || shape == "teardrop" || shape == "rounded-top-left" || shape == "rounded-top-right" || shape == "rounded-bottom-left" || shape == "rounded-bottom-right") {
            rectangle.radius = side * (side >= 5 && (shape.starts_with("rounded-") || shape == "leaf" || shape == "leaf-reverse") ? 0.35 : 0.5);
            if (shape == "classy" || shape == "rounded-top-left") rectangle.corners = 1;
            else if (shape == "leaf") rectangle.corners = 5;
            else if (shape == "leaf-reverse") rectangle.corners = 10;
            else if (shape == "teardrop") rectangle.corners = 14;
            else if (shape == "rounded-top-right") rectangle.corners = 2;
            else if (shape == "rounded-bottom-right") rectangle.corners = 4;
            else rectangle.corners = 8;
        } else if (shape == "horizontal" || shape == "horizontal-bars") {
            rectangle.y = side * 0.15; rectangle.height = side * 0.7; rectangle.radius = side * 0.35;
        } else if (shape == "vertical" || shape == "vertical-bars") {
            rectangle.x = side * 0.15; rectangle.width = side * 0.7; rectangle.radius = side * 0.35;
        } else if (shape == "inset-square") {
            rectangle.x = rectangle.y = side * 0.12;
            rectangle.width = rectangle.height = side * 0.76;
        }
        parts.push_back(std::move(rectangle));
    }
    return parts;
}

std::vector<std::uint8_t> rasterize_shape(const std::vector<Part>& parts, int side, int scale) {
    const int resolution = scale * 4;
    const int dimension = side * resolution;
    BLImage image;
    if (image.create(dimension, dimension, BL_FORMAT_PRGB32) != BL_SUCCESS) throw std::bad_alloc();
    BLContext drawing(image);
    drawing.clear_all();
    drawing.scale(resolution);
    for (const auto& part : parts) {
        BLPath path;
        if (part.kind == Part::Kind::polygon) {
            const double factor = 1 - 1.0 / dimension;
            path.move_to(part.points.front().x * factor, part.points.front().y * factor);
            for (std::size_t index = 1; index < part.points.size(); ++index)
                path.line_to(part.points[index].x * factor, part.points[index].y * factor);
            path.close();
        } else if (part.kind == Part::Kind::ellipse) {
            path.add_ellipse(BLEllipse{part.x + part.width / 2, part.y + part.height / 2, part.width / 2, part.height / 2});
        } else if (part.radius == 0) {
            path.add_rect(part.x, part.y, part.width, part.height);
        } else {
            const double left = part.x, top = part.y, right = left + part.width, bottom = top + part.height;
            const double radius = std::min({part.radius, part.width / 2, part.height / 2});
            const double tl = part.corners & 1 ? radius : 0;
            const double tr = part.corners & 2 ? radius : 0;
            const double br = part.corners & 4 ? radius : 0;
            const double bl = part.corners & 8 ? radius : 0;
            constexpr double curve = 0.5522847498307936;
            path.move_to(left + tl, top);
            path.line_to(right - tr, top);
            if (tr) path.cubic_to(right - tr + tr * curve, top, right, top + tr - tr * curve, right, top + tr);
            else path.line_to(right, top);
            path.line_to(right, bottom - br);
            if (br) path.cubic_to(right, bottom - br + br * curve, right - br + br * curve, bottom, right - br, bottom);
            else path.line_to(right, bottom);
            path.line_to(left + bl, bottom);
            if (bl) path.cubic_to(left + bl - bl * curve, bottom, left, bottom - bl + bl * curve, left, bottom - bl);
            else path.line_to(left, bottom);
            path.line_to(left, top + tl);
            if (tl) path.cubic_to(left, top + tl - tl * curve, left + tl - tl * curve, top, left + tl, top);
            else path.line_to(left, top);
            path.close();
        }
        if (drawing.fill_path(path, BLRgba32{0xffffffffu}) != BL_SUCCESS) throw std::runtime_error("Unable to rasterize QR geometry.");
    }
    drawing.end();
    BLImageData data;
    if (image.get_data(&data) != BL_SUCCESS) throw std::runtime_error("Unable to read QR raster mask.");
    const int output_side = side * scale;
    std::vector<std::uint8_t> result(std::size_t(output_side) * output_side);
    for (int y = 0; y < output_side; ++y) {
        for (int x = 0; x < output_side; ++x) {
            unsigned sum = 0;
            for (int dy = 0; dy < 4; ++dy) {
                const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(data.pixel_data) + (y * 4 + dy) * data.stride);
                for (int dx = 0; dx < 4; ++dx) sum += row[x * 4 + dx] >> 24;
            }
            result[std::size_t(y) * output_side + x] = static_cast<std::uint8_t>((sum + 8) / 16);
        }
    }
    return result;
}

}

struct MosaicScene::Geometry {
    int size = 0;
    int scale = 0;
    std::vector<Primitive> primitives;
    std::vector<Shape> shapes;
};

std::span<const Style> body_styles() { return bodies; }
std::span<const Style> eye_frame_styles() { return frames; }
std::span<const Style> eye_ball_styles() { return balls; }

MosaicScene::MosaicScene(const MosaicDesign& design, int scale, std::string_view body_shape, std::string_view eye_frame, std::string_view eye_ball)
    : geometry_(std::make_unique<Geometry>()) {
    if (std::none_of(std::begin(bodies), std::end(bodies), [body_shape](const auto& style) { return style.id == body_shape; })) throw std::invalid_argument("Choose a valid QR body shape.");
    if (std::none_of(std::begin(frames), std::end(frames), [eye_frame](const auto& style) { return style.id == eye_frame; })) throw std::invalid_argument("Choose a valid eye frame shape.");
    if (std::none_of(std::begin(balls), std::end(balls), [eye_ball](const auto& style) { return style.id == eye_ball; })) throw std::invalid_argument("Choose a valid eye ball shape.");
    if (scale < 3 || scale > 24 || scale % 3) throw std::invalid_argument("QR scale must be a multiple of 3 between 3 and 24.");
    const int size = design.size;
    geometry_->size = size;
    geometry_->scale = scale;
    const std::array module_shapes{
        body_shape,
        eye_frame == "follow-body" ? body_shape : eye_frame == "rounded-dots" ? std::string_view{"rounded"} : std::string_view{},
        eye_ball == "follow-body" ? body_shape : eye_ball == "dots" ? std::string_view{"rounded"} : std::string_view{}
    };
    std::vector<std::uint8_t> finder_roles(std::size_t(size) * size);
    std::vector<std::pair<int, int>> finders;
    auto& primitives = geometry_->primitives;
    for (const auto [row, column] : {std::pair{0, 0}, std::pair{0, size - 7}, std::pair{size - 7, 0}, std::pair{size - 7, size - 7}}) {
        bool is_finder = true;
        for (int y = 0; y < 7 && is_finder; ++y) {
            for (int x = 0; x < 7; ++x) {
                const auto index = std::size_t(row + y) * size + column + x;
                const bool expected = x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4);
                if (design.writable[index] || bool(design.matrix[index]) != expected) { is_finder = false; break; }
            }
        }
        if (!is_finder) continue;
        finders.emplace_back(row, column);
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 7; ++x)
                finder_roles[std::size_t(row + y) * size + column + x] = x >= 2 && x <= 4 && y >= 2 && y <= 4 ? 2 : 1;
        if (module_shapes[1].empty()) {
            const auto center = (std::size_t(row + 3) * size + column + 3) * 3;
            const std::array<std::uint8_t, 3> color{design.colors[center], design.colors[center + 1], design.colors[center + 2]};
            if (eye_frame == "dots" || eye_frame == "dashed")
                primitives.push_back({eye_frame == "dots" ? "frame-dots" : "frame-dashed", column + 4, row + 4, 7, color, 15});
            else {
                primitives.push_back({eye_frame, column + 4, row + 4, 7, color, 15});
                primitives.push_back({eye_frame == "slanted" ? "square" : eye_frame, column + 5, row + 5, 5, {255, 255, 255}, 15});
            }
        }
    }
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            const auto index = std::size_t(row) * size + column;
            if (!design.matrix[index]) continue;
            const auto role = finder_roles[index];
            const auto shape = module_shapes[role];
            if (shape.empty()) continue;
            const std::array<std::uint8_t, 3> color{design.colors[index * 3], design.colors[index * 3 + 1], design.colors[index * 3 + 2]};
            int corners = 15;
            if (shape == "connected-rounded" || shape == "horizontal-bars" || shape == "vertical-bars") {
                const bool above = row > 0 && design.matrix[index - size];
                const bool below = row + 1 < size && design.matrix[index + size];
                const bool left = column > 0 && design.matrix[index - 1];
                const bool right = column + 1 < size && design.matrix[index + 1];
                corners = (!above && !left) | ((!above && !right) << 1) | ((!below && !right) << 2) | ((!below && !left) << 3);
                if (shape == "horizontal-bars") corners = (left ? 0 : 9) | (right ? 0 : 6);
                else if (shape == "vertical-bars") corners = (above ? 0 : 3) | (below ? 0 : 12);
            } else if (shape == "random-dots" || shape == "mosaic") corners = (row * 17 + column * 29 + row * column * 7) % 16;
            primitives.push_back({shape, column + 4, row + 4, 1, color, corners});
        }
    }
    if (module_shapes[2].empty()) {
        for (const auto [row, column] : finders) {
            const auto center = (std::size_t(row + 3) * size + column + 3) * 3;
            primitives.push_back({eye_ball, column + 6, row + 6, 3, {design.colors[center], design.colors[center + 1], design.colors[center + 2]}, 15});
        }
    }
    std::map<std::tuple<std::string_view, int, int>, std::size_t> shapes;
    for (auto& primitive : primitives) {
        const auto key = std::tuple{primitive.shape, primitive.side, primitive.corners};
        auto found = shapes.find(key);
        if (found == shapes.end()) {
            const auto index = geometry_->shapes.size();
            geometry_->shapes.push_back({primitive.side, shape_geometry(primitive.shape, primitive.side, primitive.corners, size)});
            found = shapes.emplace(key, index).first;
        }
        primitive.geometry = found->second;
    }
}

MosaicScene::~MosaicScene() = default;

Image MosaicScene::rasterize() const {
    const int scale = geometry_->scale;
    const int image_size = (geometry_->size + 8) * scale;
    Image result{image_size, image_size, std::vector<std::uint8_t>(std::size_t(image_size) * image_size * 4, 255)};
    std::vector<std::vector<std::uint8_t>> masks;
    masks.reserve(geometry_->shapes.size());
    for (const auto& shape : geometry_->shapes) masks.push_back(rasterize_shape(shape.parts, shape.side, scale));
    for (const auto& primitive : geometry_->primitives) {
        const auto& mask = masks[primitive.geometry];
        const int side = primitive.side * scale;
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                const unsigned alpha = mask[std::size_t(y) * side + x];
                const auto target = (std::size_t(primitive.row * scale + y) * image_size + primitive.column * scale + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    result.pixels[target + channel] = static_cast<std::uint8_t>((result.pixels[target + channel] * (255 - alpha) + primitive.color[channel] * alpha + 127) / 255);
            }
        }
    }
    return result;
}

std::string MosaicScene::svg() const {
    const int size = geometry_->size;
    const int image_size = (size + 8) * geometry_->scale;
    std::ostringstream svg;
    svg.imbue(std::locale::classic());
    svg << std::setprecision(12) << "<?xml version=\"1.0\" encoding=\"utf-8\"?><svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << image_size << "\" height=\"" << image_size << "\" viewBox=\"0 0 " << size + 8 << ' ' << size + 8 << "\"><rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
    for (const auto& primitive : geometry_->primitives) {
        std::ostringstream color;
        color << '#' << std::hex << std::setfill('0');
        for (const auto channel : primitive.color) color << std::setw(2) << int(channel);
        for (const auto& part : geometry_->shapes[primitive.geometry].parts) {
            const double x = primitive.column + part.x, y = primitive.row + part.y;
            if (part.kind == Part::Kind::polygon) {
                svg << "<polygon points=\"";
                for (const auto point : part.points) svg << primitive.column + point.x << ',' << primitive.row + point.y << ' ';
                svg << '"';
            } else if (part.kind == Part::Kind::ellipse) {
                svg << "<ellipse cx=\"" << x + part.width / 2 << "\" cy=\"" << y + part.height / 2 << "\" rx=\"" << part.width / 2 << "\" ry=\"" << part.height / 2 << '"';
            } else if (part.radius && part.corners != 15) {
                const double tl = part.corners & 1 ? part.radius : 0;
                const double tr = part.corners & 2 ? part.radius : 0;
                const double br = part.corners & 4 ? part.radius : 0;
                const double bl = part.corners & 8 ? part.radius : 0;
                svg << "<path d=\"M " << x + tl << ' ' << y << " H " << x + part.width - tr << " A " << tr << ' ' << tr << " 0 0 1 " << x + part.width << ' ' << y + tr;
                svg << " V " << y + part.height - br << " A " << br << ' ' << br << " 0 0 1 " << x + part.width - br << ' ' << y + part.height;
                svg << " H " << x + bl << " A " << bl << ' ' << bl << " 0 0 1 " << x << ' ' << y + part.height - bl;
                svg << " V " << y + tl << " A " << tl << ' ' << tl << " 0 0 1 " << x + tl << ' ' << y << " Z\"";
            } else {
                svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << part.width << "\" height=\"" << part.height << "\" rx=\"" << part.radius << '"';
            }
            svg << " fill=\"" << color.str() << "\"/>";
        }
    }
    svg << "</svg>";
    return svg.str();
}

Image render_picture(const QrCode& qr, const Image& reference, int scale) {
    if (scale < 3 || scale > 24 || scale % 3) throw std::invalid_argument("QR scale must be a multiple of 3 between 3 and 24.");
    const int interior = qr.size * 3;
    const auto artwork = resize_image(reference, interior, interior);
    const int base_size = (qr.size + 8) * 3;
    Image base{base_size, base_size, std::vector<std::uint8_t>(std::size_t(base_size) * base_size * 4, 255)};
    for (int row = 0; row < interior; ++row) {
        for (int column = 0; column < interior; ++column) {
            const auto module = std::size_t(row / 3) * qr.size + column / 3;
            const auto role = qr.roles[module];
            const bool protected_module = role == ModuleRole::finder || role == ModuleRole::separator || role == ModuleRole::alignment || role == ModuleRole::timing;
            const bool keep_center = row % 3 == 1 && column % 3 == 1;
            const auto target = (std::size_t(row + 12) * base_size + column + 12) * 4;
            const auto source = (std::size_t(row) * interior + column) * 4;
            const auto tone = qr.matrix[module] ? std::uint8_t{0} : std::uint8_t{255};
            for (int channel = 0; channel < 3; ++channel)
                base.pixels[target + channel] = protected_module || keep_center || !artwork.pixels[source + 3] ? tone : artwork.pixels[source + channel];
        }
    }
    const int factor = scale / 3;
    Image result{base_size * factor, base_size * factor, std::vector<std::uint8_t>(std::size_t(base_size * factor) * base_size * factor * 4)};
    for (int row = 0; row < result.height; ++row) {
        for (int column = 0; column < result.width; ++column) {
            const auto source = (std::size_t(row / factor) * base_size + column / factor) * 4;
            const auto target = (std::size_t(row) * result.width + column) * 4;
            std::copy_n(base.pixels.begin() + source, 4, result.pixels.begin() + target);
        }
    }
    return result;
}

std::string render_style_preview(std::string_view category, std::string_view key) {
    MosaicDesign design;
    std::string_view body = "rounded", frame = "rounded-dots", ball = "dots", view_box;
    if (category == "body") {
        design.size = 7;
        design.matrix = {1,0,0,1,1,0,1, 1,1,0,0,1,0,1, 0,1,0,1,1,1,0, 1,1,1,0,1,0,1, 1,0,1,0,0,1,1, 0,1,1,1,0,0,1, 1,0,0,1,1,1,0};
        design.writable.assign(49, 1);
        body = key;
        view_box = "3.7 3.7 7.6 7.6";
    } else if (category == "eye_frame" || category == "eye_ball") {
        design.size = 21;
        design.matrix.assign(441, 0);
        design.writable.assign(441, 1);
        for (const auto [row, column] : {std::pair{0, 0}, std::pair{0, 14}, std::pair{14, 0}}) {
            for (int y = 0; y < 7; ++y) {
                for (int x = 0; x < 7; ++x) {
                    const auto index = std::size_t(row + y) * 21 + column + x;
                    design.matrix[index] = x == 0 || x == 6 || y == 0 || y == 6 || (x >= 2 && x <= 4 && y >= 2 && y <= 4);
                    design.writable[index] = 0;
                }
            }
        }
        for (int position = 8; position < 13; ++position) {
            design.matrix[6 * 21 + position] = design.matrix[position * 21 + 6] = position % 2 == 0;
            design.writable[6 * 21 + position] = design.writable[position * 21 + 6] = 0;
        }
        if (category == "eye_frame") { frame = key; view_box = "3.6 3.6 7.8 7.8"; }
        else { ball = key; view_box = "5.7 5.7 3.6 3.6"; }
    } else throw std::invalid_argument("Choose a valid QR style preview.");
    design.colors.assign(std::size_t(design.size) * design.size * 3, 0);
    auto result = MosaicScene(design, 12, body, frame, ball).svg();
    const auto width = result.find(" width=\"");
    const auto viewport = result.find("><rect", width);
    result.replace(width, viewport - width, " width=\"88\" height=\"88\" viewBox=\"" + std::string(view_box) + '"');
    if (category == "eye_frame") result.insert(result.size() - 6, "<rect x=\"6\" y=\"6\" width=\"3\" height=\"3\" fill=\"white\"/>");
    return result;
}

}
