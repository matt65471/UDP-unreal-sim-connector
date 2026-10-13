#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace rocket {

struct Telemetry {
    double mission_time_seconds = 0.0;
    std::string phase;

    bool ignited = false;
    bool liftoff = false;
    bool apogee = false;
    bool separated = false;
    bool drogue = false;
    bool main = false;
    bool landed = false;

    double altitude_agl_ft = 0.0;
    double altitude_asl_ft = 0.0;
    double vertical_speed_fps = 0.0;
    double thrust_lbf = 0.0;
    double max_altitude_agl_ft = 0.0;
    double apogee_altitude_agl_ft = 0.0;

    double latitude_deg = 0.0;
    double longitude_deg = 0.0;
    double yaw_deg = 0.0;
    double pitch_deg = 0.0;
    double roll_deg = 0.0;
};

// Parses the fixed JSON schema emitted by URocketUdpBridge::BuildTelemetryJson.
// This intentionally avoids a desktop-only JSON dependency so the telemetry
// model and validation rules can later be reused in embedded firmware.
std::optional<Telemetry> parse_telemetry(std::string_view json, std::string& error);

}  // namespace rocket
