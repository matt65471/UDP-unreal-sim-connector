#include "rocket/telemetry.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool close_enough(double left, double right) {
    return std::abs(left - right) < 0.0001;
}

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}

}  // namespace

int main() {
    const std::string packet =
        R"({"met":12.345,"phase":"POWERED_ASCENT","ignited":true,"liftoff":true,)"
        R"("apogee":false,"separated":false,"drogue":false,"main":false,"landed":false,)"
        R"("agl_ft":842.10,"asl_ft":868.40,"vs_fps":401.20,"thrust_lbf":428.00,)"
        R"("max_agl_ft":842.10,"apogee_agl_ft":0.00,"lat":37.0,"lon":-122.0,)"
        R"("yaw_deg":180.0,"pitch_deg":88.2,"roll_deg":0.4})";

    std::string error;
    const auto telemetry = rocket::parse_telemetry(packet, error);
    if (!telemetry) {
        return fail("valid packet rejected: " + error);
    }
    if (telemetry->phase != "POWERED_ASCENT") {
        return fail("phase was not parsed");
    }
    if (!telemetry->ignited || !telemetry->liftoff || telemetry->landed) {
        return fail("state flags were not parsed");
    }
    if (!close_enough(telemetry->altitude_agl_ft, 842.10) ||
        !close_enough(telemetry->vertical_speed_fps, 401.20)) {
        return fail("flight values were not parsed");
    }

    const auto invalid = rocket::parse_telemetry(R"({"phase":"COAST"})", error);
    if (invalid || error.empty()) {
        return fail("incomplete packet was accepted");
    }

    std::cout << "All telemetry parser tests passed.\n";
    return 0;
}
