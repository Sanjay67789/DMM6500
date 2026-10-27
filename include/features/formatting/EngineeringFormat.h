#pragma once
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

struct FormattedMeasurement {
    std::string value;
    std::string unit;
};

inline FormattedMeasurement FormatMeasurement(double value, const std::string& unit) {
    if (!std::isfinite(value)) return {"invalid", unit};

    const double magnitude = std::fabs(value);
    double scaled = value;
    std::string outUnit = unit;
    auto scale = [&](double divisor, const char* prefix) {
        scaled = value / divisor;
        outUnit = std::string(prefix) + unit;
    };

    if (unit == "ohm" || unit == "Ω") {
        if (magnitude != 0 && magnitude < 1e-3) scale(1e-6, "µ");
        else if (magnitude != 0 && magnitude < 1) scale(1e-3, "m");
        else if (magnitude >= 1e6) scale(1e6, "M");
        else if (magnitude >= 1e3) scale(1e3, "k");
        if (outUnit == "ohm") outUnit = "Ω";
    } else if (unit == "V" || unit == "A") {
        if (magnitude != 0 && magnitude < 1e-6) scale(1e-9, "n");
        else if (magnitude != 0 && magnitude < 1e-3) scale(1e-6, "µ");
        else if (magnitude != 0 && magnitude < 1) scale(1e-3, "m");
        else if (magnitude >= 1e3) scale(1e3, "k");
    } else if (unit == "Hz") {
        if (magnitude != 0 && magnitude < 1) scale(1e-3, "m");
        else if (magnitude >= 1e9) scale(1e9, "G");
        else if (magnitude >= 1e6) scale(1e6, "M");
        else if (magnitude >= 1e3) scale(1e3, "k");
    } else if (unit == "s") {
        if (magnitude != 0 && magnitude < 1e-6) scale(1e-9, "n");
        else if (magnitude != 0 && magnitude < 1e-3) scale(1e-6, "µ");
        else if (magnitude != 0 && magnitude < 1) scale(1e-3, "m");
    } else if (unit == "F") {
        if (magnitude != 0 && magnitude < 1e-9) scale(1e-12, "p");
        else if (magnitude != 0 && magnitude < 1e-6) scale(1e-9, "n");
        else if (magnitude != 0 && magnitude < 1e-3) scale(1e-6, "µ");
        else if (magnitude != 0 && magnitude < 1) scale(1e-3, "m");
    }

    std::ostringstream stream;
    stream << std::setprecision(5) << std::defaultfloat << scaled;
    return {stream.str(), outUnit};
}
