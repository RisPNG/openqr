#include <openqr/openqr.h>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> read_image_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Could not open input file: " + path.string());
    const auto length = file.tellg();
    if (length < 0 || length > 10 * 1024 * 1024) throw std::invalid_argument("Input files must be at most 10 MiB.");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), length)) throw std::runtime_error("Could not read input file: " + path.string());
    return bytes;
}

void write_artifact(const std::filesystem::path& path, openqr_bytes bytes) {
    std::ofstream file(path, std::ios::binary);
    if (!file || !file.write(reinterpret_cast<const char*>(bytes.data), static_cast<std::streamsize>(bytes.size)))
        throw std::runtime_error("Could not write output file: " + path.string());
}

void write_json_string(std::ostream& output, std::string_view text) {
    constexpr auto hex = "0123456789abcdef";
    output << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') output << '\\' << c;
        else if (c < 32) output << "\\u00" << hex[c >> 4] << hex[c & 15];
        else output << c;
    }
    output << '"';
}

}

int main(int argc, char** argv) {
    try {
        openqr_request request;
        openqr_request_init(&request);
        std::string payload, body = request.options.body_shape, frame = request.options.eye_frame, ball = request.options.eye_ball;
        std::vector<std::uint8_t> payload_file, reference, background;
        std::filesystem::path output_directory = ".";
        bool has_payload = false, payload_from_file = false, has_reference = false, has_background = false;
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--help" || option == "-h") {
                std::cout << "OpenQR " << openqr_version() << "\n"
                    "Usage: openqr --payload TEXT [--output DIRECTORY] [options]\n"
                    "  --payload-file FILE       Read literal payload bytes (including NUL)\n"
                    "  --reference FILE          Artwork PNG/APNG, JPEG, WebP, GIF, or ICO\n"
                    "  --background FILE         Still background image\n"
                    "  --mode mosaic|picture     Artwork method (default mosaic)\n"
                    "  --version 1..40           Single version (default all versions)\n"
                    "  --seed 0..4294967295      Deterministic search seed\n"
                    "  --randomize-seed          Exclude the supplied seed\n"
                    "  --error-correction L|M|Q|H Choose a fixed level (default automatic)\n"
                    "  --scale 3..24             Pixels per module, multiple of 3\n"
                    "  --recovery-margin N       Reserved correction words per block\n"
                    "  --monochrome              Use black modules\n"
                    "  --vary-url                Allow a generated URL fragment\n"
                    "  --contrast N              Reference contrast, 0.1..3\n"
                    "  --brightness N            Reference brightness, 0.1..3\n"
                    "  --background-rgba HEX     RRGGBBAA, alpha 00 or ff\n"
                    "  --body STYLE --eye-frame STYLE --eye-ball STYLE\n"
                    "  --styles                  Print available style IDs\n"
                    "Results and metadata are written to DIRECTORY. JSON metadata is also printed to stdout.\n";
                return 0;
            }
            if (option == "--styles") {
                for (auto category : {OPENQR_BODY, OPENQR_EYE_FRAME, OPENQR_EYE_BALL}) {
                    for (std::size_t index = 0; index < openqr_style_count(category); ++index)
                        std::cout << category << '\t' << openqr_style_id(category, index) << '\t' << openqr_style_label(category, index) << '\n';
                }
                return 0;
            }
            if (option == "--monochrome") { request.options.flags &= ~OPENQR_COLOR; continue; }
            if (option == "--vary-url") { request.options.flags |= OPENQR_VARY_URL; continue; }
            if (option == "--randomize-seed") { request.options.flags |= OPENQR_RANDOMIZE_SEED; continue; }
            if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + std::string(option));
            const std::string value = argv[++i];
            if (option == "--payload") { payload = value; has_payload = true; payload_from_file = false; }
            else if (option == "--payload-file") { payload_file = read_image_file(value); has_payload = true; payload_from_file = true; }
            else if (option == "--reference") { reference = read_image_file(value); has_reference = true; }
            else if (option == "--background") { background = read_image_file(value); has_background = true; }
            else if (option == "--output") output_directory = value;
            else if (option == "--body") body = value;
            else if (option == "--eye-frame") frame = value;
            else if (option == "--eye-ball") ball = value;
            else if (option == "--mode") {
                if (value != "mosaic" && value != "picture") throw std::invalid_argument("Mode must be mosaic or picture.");
                request.options.mode = value == "mosaic" ? OPENQR_MOSAIC : OPENQR_PICTURE;
            } else if (option == "--error-correction") {
                request.options.flags &= ~OPENQR_ERROR_CORRECTION_MASK;
                if (value == "L") request.options.flags |= OPENQR_ERROR_CORRECTION_L;
                else if (value == "M") request.options.flags |= OPENQR_ERROR_CORRECTION_M;
                else if (value == "Q") request.options.flags |= OPENQR_ERROR_CORRECTION_Q;
                else if (value == "H") request.options.flags |= OPENQR_ERROR_CORRECTION_H;
                else throw std::invalid_argument("Error correction must be L, M, Q, or H.");
            } else if (option == "--brightness" || option == "--contrast") {
                double number = 0;
                std::istringstream input(value);
                input.imbue(std::locale::classic());
                input >> std::noskipws >> number;
                if (!input || !input.eof() || value.starts_with('+') || value.find_first_not_of("0123456789.eE+-") != std::string::npos) throw std::invalid_argument("Invalid number: " + value);
                if (option == "--brightness") request.options.brightness = number;
                else request.options.contrast = number;
            } else {
                std::uint32_t number = 0;
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number, option == "--background-rgba" ? 16 : 10);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) throw std::invalid_argument("Invalid number or unknown option: " + std::string(option));
                if (option == "--version") request.options.version = number;
                else if (option == "--scale") request.options.scale = number;
                else if (option == "--recovery-margin") request.options.recovery_margin = number;
                else if (option == "--background-rgba") request.options.background_rgba = number;
                else if (option == "--seed") { request.options.seed = number; request.options.flags |= OPENQR_SEED_SET; }
                else throw std::invalid_argument("Unknown option: " + std::string(option));
            }
        }
        if (!has_payload) throw std::invalid_argument("Use --payload TEXT or --payload-file FILE.");
        const std::uint8_t empty_image = 0;
        request.payload = payload_from_file ? openqr_bytes{payload_file.data(), payload_file.size()} : openqr_bytes{reinterpret_cast<const std::uint8_t*>(payload.data()), payload.size()};
        if (has_reference) request.reference = {reference.empty() ? &empty_image : reference.data(), reference.size()};
        if (has_background) request.background = {background.empty() ? &empty_image : background.data(), background.size()};
        request.options.body_shape = body.c_str();
        request.options.eye_frame = frame.c_str();
        request.options.eye_ball = ball.c_str();
        openqr_result* raw_result = nullptr;
        openqr_error* raw_error = nullptr;
        const auto status = openqr_generate(&request, &raw_result, &raw_error);
        const std::unique_ptr<openqr_result, decltype(&openqr_result_destroy)> result(raw_result, openqr_result_destroy);
        const std::unique_ptr<openqr_error, decltype(&openqr_error_destroy)> error(raw_error, openqr_error_destroy);
        if (status != OPENQR_OK) {
            std::cerr << (error ? openqr_error_message(error.get()) : "OpenQR generation failed.") << '\n';
            return static_cast<int>(status);
        }
        std::filesystem::create_directories(output_directory);
        std::ostringstream manifest;
        manifest << "{\"seed\":" << openqr_result_seed(result.get()) << ",\"results\":[";
        for (std::size_t index = 0; index < openqr_result_count(result.get()); ++index) {
            openqr_entry entry;
            openqr_result_entry(result.get(), index, &entry);
            const auto stem = "qr-v" + std::to_string(entry.version);
            const auto filename = stem + (entry.format == OPENQR_GIF ? ".gif" : ".png");
            write_artifact(output_directory / filename, entry.image);
            write_artifact(output_directory / (stem + "-preview" + (entry.format == OPENQR_GIF ? ".gif" : ".png")), entry.preview);
            if (entry.svg.size) write_artifact(output_directory / (stem + ".svg"), entry.svg);
            if (entry.target_preview.size) write_artifact(output_directory / (stem + "-target.png"), entry.target_preview);
            if (index) manifest << ',';
            manifest << "{\"version\":" << entry.version << ",\"seed\":" << entry.seed << ",\"file\":";
            write_json_string(manifest, filename);
            manifest << ",\"payload_hex\":\"";
            constexpr auto hex = "0123456789abcdef";
            for (std::size_t i = 0; i < entry.payload.size; ++i) manifest << hex[entry.payload.data[i] >> 4] << hex[entry.payload.data[i] & 15];
            manifest << "\",\"error_correction\":\"" << entry.error_correction << "\",\"verified_frames\":" << entry.verified_frames
                     << ",\"total_frames\":" << entry.total_frames << ",\"changed_codewords\":" << entry.changed_codewords
                     << ",\"layout_attempts\":" << entry.layout_attempts << ",\"artwork_fraction\":" << entry.artwork_fraction
                     << ",\"color_exposure\":" << entry.color_exposure << ",\"block_errors\":[";
            for (std::size_t i = 0; i < entry.block_count; ++i) { if (i) manifest << ','; manifest << entry.block_errors[i]; }
            manifest << "],\"block_capacities\":[";
            for (std::size_t i = 0; i < entry.block_count; ++i) { if (i) manifest << ','; manifest << entry.block_capacities[i]; }
            manifest << "]}";
        }
        manifest << "],\"skipped\":[";
        for (std::size_t index = 0; index < openqr_result_skipped_count(result.get()); ++index) {
            if (index) manifest << ',';
            manifest << "{\"version\":" << openqr_result_skipped_version(result.get(), index) << ",\"reason\":";
            write_json_string(manifest, openqr_result_skipped_reason(result.get(), index));
            manifest << '}';
        }
        manifest << "]}\n";
        const auto json = manifest.str();
        write_artifact(output_directory / "manifest.json", {reinterpret_cast<const std::uint8_t*>(json.data()), json.size()});
        const auto reference_preview = openqr_result_reference_preview(result.get());
        if (reference_preview.size) write_artifact(output_directory / "reference-preview.png", reference_preview);
        const auto reference_input = openqr_result_reference_input(result.get());
        if (reference_input.size) write_artifact(output_directory / "reference-input.png", reference_input);
        std::cout << json;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
