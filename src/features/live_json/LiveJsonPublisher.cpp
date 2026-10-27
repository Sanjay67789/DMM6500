#include "features/live_json/LiveJsonPublisher.h"
#include "features/formatting/EngineeringFormat.h"
#include <cstdio>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

namespace {
string EscapeJson(const string& s) {
    ostringstream o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default: if (c < 0x20) { o << "\\u" << hex << setw(4) << setfill('0') << (int)c << dec; } else o << c;
        }
    }
    return o.str();
}
string UtcNow() {
    const auto now = chrono::system_clock::now();
    const time_t t = chrono::system_clock::to_time_t(now);
    tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &t);
#else
    gmtime_r(&t, &utc);
#endif
    ostringstream o; o << put_time(&utc, "%Y-%m-%dT%H:%M:%SZ"); return o.str();
}
bool AtomicWrite(const string& path, const string& contents) {
    const string temp = path + ".tmp";
    { ofstream f(temp, ios::binary | ios::trunc); if (!f) return false; f << contents; f.flush(); if (!f) return false; }
#ifdef _WIN32
    if (!MoveFileExA(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { remove(temp.c_str()); return false; }
#else
    if (rename(temp.c_str(), path.c_str()) != 0) { remove(temp.c_str()); return false; }
#endif
    return true;
}
string ExtractModel(const string& id) {
    // VISA *IDN? normally returns manufacturer, model, serial, firmware.
    size_t first = id.find(','); if (first == string::npos) return id;
    size_t second = id.find(',', first + 1); return id.substr(first + 1, second == string::npos ? string::npos : second - first - 1);
}
string ExtractManufacturer(const string& id) { size_t p = id.find(','); return p == string::npos ? "unknown" : id.substr(0, p); }
}

bool LiveJsonPublisher::Publish(const IDMMDriver& driver, const string& identity,
                                const Measurement& m, const string& status, const string& error) {
    const auto display = m.ok ? FormatMeasurement(m.value, m.unit) : FormattedMeasurement{"", m.unit};
    ostringstream j;
    j << "{\n"
      << "  \"schema_version\": 1,\n"
      << "  \"timestamp_utc\": \"" << EscapeJson(UtcNow()) << "\",\n"
      << "  \"connected\": " << (driver.IsConnected() ? "true" : "false") << ",\n"
      << "  \"manufacturer\": \"" << EscapeJson(ExtractManufacturer(identity)) << "\",\n"
      << "  \"model\": \"" << EscapeJson(ExtractModel(identity)) << "\",\n"
      << "  \"identity\": \"" << EscapeJson(identity) << "\",\n"
      << "  \"visa_address\": \"" << EscapeJson(driver.GetAddress()) << "\",\n"
      << "  \"function\": \"" << EscapeJson(m.function) << "\",\n"
      << "  \"value\": ";
    if (m.ok) j << setprecision(15) << m.value; else j << "null";
    j << ",\n  \"display_value\": \"" << EscapeJson(display.value) << "\",\n"
      << "  \"display_unit\": \"" << EscapeJson(display.unit) << "\",\n"
      << "  \"raw\": \"" << EscapeJson(m.raw) << "\",\n"
      << "  \"unit\": \"" << EscapeJson(m.unit) << "\",\n"
      << "  \"status\": \"" << EscapeJson(status) << "\",\n"
      << "  \"error\": \"" << EscapeJson(error) << "\"\n}\n";
    return AtomicWrite(path_, j.str());
}

bool LiveJsonPublisher::PublishDisconnected(const string& identity, const string& error) {
    Measurement m; m.function = "UNKNOWN"; m.unit = ""; m.ok = false;
    // No active driver available; publish a minimal schema-compatible state.
    ostringstream j;
    j << "{\n  \"schema_version\": 1,\n  \"timestamp_utc\": \"" << EscapeJson(UtcNow())
      << "\",\n  \"connected\": false,\n  \"manufacturer\": \"" << EscapeJson(ExtractManufacturer(identity))
      << "\",\n  \"model\": \"" << EscapeJson(ExtractModel(identity))
      << "\",\n  \"identity\": \"" << EscapeJson(identity)
      << "\",\n  \"visa_address\": \"\",\n  \"function\": \"UNKNOWN\",\n  \"value\": null,\n  \"raw\": \"\",\n  \"unit\": \"\",\n  \"status\": \"DISCONNECTED\",\n  \"error\": \"" << EscapeJson(error) << "\"\n}\n";
    return AtomicWrite(path_, j.str());
}
