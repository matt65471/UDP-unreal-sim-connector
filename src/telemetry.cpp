#include "rocket/telemetry.hpp"

#include <cerrno>
#include <cstdlib>

namespace rocket {
namespace {

std::optional<std::string_view> value_after_key(std::string_view json, std::string_view key) {
    const std::string quoted_key = "\"" + std::string(key) + "\"";
    std::size_t position = json.find(quoted_key);
    if (position == std::string_view::npos) {
        return std::nullopt;
    }

    position = json.find(':', position + quoted_key.size());
    if (position == std::string_view::npos) {
        return std::nullopt;
    }

    ++position;
    while (position < json.size() &&
           (json[position] == ' ' || json[position] == '\t' ||
            json[position] == '\r' || json[position] == '\n')) {
        ++position;
    }
    return json.substr(position);
}

bool read_number(std::string_view json, std::string_view key, double& output) {
    const auto value = value_after_key(json, key);
    if (!value) {
        return false;
    }

    const std::string text(*value);
    char* end = nullptr;
    errno = 0;
    const double parsed = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || errno == ERANGE) {
        return false;
    }

    output = parsed;
    return true;
}

bool read_bool(std::string_view json, std::string_view key, bool& output) {
    const auto value = value_after_key(json, key);
    if (!value) {
        return false;
    }
    if (value->substr(0, 4) == "true") {
        output = true;
        return true;
    }
    if (value->substr(0, 5) == "false") {
        output = false;
        return true;
    }
    return false;
}

bool read_string(std::string_view json, std::string_view key, std::string& output) {
    const auto value = value_after_key(json, key);
    if (!value || value->empty() || (*value)[0] != '"') {
        return false;
    }

    const std::size_t closing_quote = value->find('"', 1);
    if (closing_quote == std::string_view::npos) {
        return false;
    }

    output.assign(value->substr(1, closing_quote - 1));
    return true;
}

}  // namespace

std::optional<Telemetry> parse_telemetry(std::string_view json, std::string& error) {
    Telemetry telemetry;

    if (json.empty() || json.front() != '{' || json.find('}') == std::string_view::npos) {
        error = "packet is not a JSON object";
        return std::nullopt;
    }

    // The fields below form the minimum useful schema. New fields can be added
    // by Unreal without breaking an older receiver.
    if (!read_number(json, "met", telemetry.mission_time_seconds) ||
        !read_string(json, "phase", telemetry.phase) ||
        !read_number(json, "agl_ft", telemetry.altitude_agl_ft) ||
        !read_number(json, "vs_fps", telemetry.vertical_speed_fps) ||
        !read_number(json, "thrust_lbf", telemetry.thrust_lbf)) {
        error = "packet is missing a required telemetry field or contains an invalid value";
        return std::nullopt;
    }

    // Remaining fields are optional for forward/backward compatibility.
    read_bool(json, "ignited", telemetry.ignited);
    read_bool(json, "liftoff", telemetry.liftoff);
    read_bool(json, "apogee", telemetry.apogee);
    read_bool(json, "separated", telemetry.separated);
    read_bool(json, "drogue", telemetry.drogue);
    read_bool(json, "main", telemetry.main);
    read_bool(json, "landed", telemetry.landed);
    read_number(json, "asl_ft", telemetry.altitude_asl_ft);
    read_number(json, "max_agl_ft", telemetry.max_altitude_agl_ft);
    read_number(json, "apogee_agl_ft", telemetry.apogee_altitude_agl_ft);
    read_number(json, "lat", telemetry.latitude_deg);
    read_number(json, "lon", telemetry.longitude_deg);
    read_number(json, "yaw_deg", telemetry.yaw_deg);
    read_number(json, "pitch_deg", telemetry.pitch_deg);
    read_number(json, "roll_deg", telemetry.roll_deg);

    error.clear();
    return telemetry;
}

}  // namespace rocket
