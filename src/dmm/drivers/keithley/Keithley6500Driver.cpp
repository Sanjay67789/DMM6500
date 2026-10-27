#include "dmm/drivers/keithley/Keithley6500Driver.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

using namespace std;

namespace {
string Trim(string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

struct ModeInfo { const char* scpi; const char* unit; const char* name; };
ModeInfo Info(MeasurementMode m) {
    switch (m) {
        case MeasurementMode::DC_VOLTAGE: return {"VOLT:DC", "V", "DC_VOLTAGE"};
        case MeasurementMode::AC_VOLTAGE: return {"VOLT:AC", "V", "AC_VOLTAGE"};
        case MeasurementMode::DC_CURRENT: return {"CURR:DC", "A", "DC_CURRENT"};
        case MeasurementMode::AC_CURRENT: return {"CURR:AC", "A", "AC_CURRENT"};
        case MeasurementMode::TWO_WIRE_RES: return {"RES", "ohm", "TWO_WIRE_RESISTANCE"};
        case MeasurementMode::FOUR_WIRE_RES: return {"FRES", "ohm", "FOUR_WIRE_RESISTANCE"};
        case MeasurementMode::TEMPERATURE: return {"TEMP", "degC_or_configured_unit", "TEMPERATURE"};
        case MeasurementMode::CONTINUITY: return {"CONT", "bool_or_ohm", "CONTINUITY"};
        case MeasurementMode::DIODE: return {"DIOD", "V", "DIODE"};
        case MeasurementMode::FREQUENCY: return {"FREQ", "Hz", "FREQUENCY"};
        case MeasurementMode::PERIOD: return {"PER", "s", "PERIOD"};
        case MeasurementMode::CAPACITANCE: return {"CAP", "F", "CAPACITANCE"};
    }
    return {"", "", "UNKNOWN"};
}
}

Keithley6500Driver::~Keithley6500Driver() { Disconnect(); }
void Keithley6500Driver::SetError(const string& m) { lastError_ = m; }

bool Keithley6500Driver::Connect(const string& visaAddress) {
    Disconnect(); lastError_.clear();
    ViStatus status = viOpenDefaultRM(&defaultRM_);
    if (status < VI_SUCCESS) {
        defaultRM_ = VI_NULL;
        SetError("viOpenDefaultRM failed; VISA status=" + to_string(status));
        return false;
    }
    status = viOpen(defaultRM_, const_cast<ViRsrc>(visaAddress.c_str()), VI_NULL, 3000, &session_);
    if (status < VI_SUCCESS) {
        session_ = VI_NULL;
        viClose(defaultRM_); defaultRM_ = VI_NULL;
        SetError("viOpen failed for '" + visaAddress + "'; VISA status=" + to_string(status));
        return false;
    }
    address_ = visaAddress;
    status = viSetAttribute(session_, VI_ATTR_TMO_VALUE, 5000);
    if (status < VI_SUCCESS) { SetError("Cannot set VISA timeout; status=" + to_string(status)); Disconnect(); return false; }
    status = viSetAttribute(session_, VI_ATTR_TERMCHAR, '\n');
    if (status < VI_SUCCESS) { SetError("Cannot set VISA termination character; status=" + to_string(status)); Disconnect(); return false; }
    status = viSetAttribute(session_, VI_ATTR_TERMCHAR_EN, VI_TRUE);
    if (status < VI_SUCCESS) { SetError("Cannot enable VISA termination character; status=" + to_string(status)); Disconnect(); return false; }

    string id;
    if (!QuerySCPI("*IDN?", id)) { string why = lastError_; Disconnect(); SetError("Connected but *IDN? failed: " + why); return false; }
    if (id.empty()) { Disconnect(); SetError("Instrument returned an empty *IDN? response."); return false; }
    return true;
}

void Keithley6500Driver::Disconnect() {
    if (session_ != VI_NULL) { viClose(session_); session_ = VI_NULL; }
    if (defaultRM_ != VI_NULL) { viClose(defaultRM_); defaultRM_ = VI_NULL; }
    address_.clear();
}
bool Keithley6500Driver::IsConnected() const { return session_ != VI_NULL; }
string Keithley6500Driver::GetAddress() const { return address_; }
string Keithley6500Driver::GetIdentity() { string s; QuerySCPI("*IDN?", s); return s; }
bool Keithley6500Driver::Reset() { return SendSCPI("*RST"); }

bool Keithley6500Driver::Supports(MeasurementMode m) const {
    // Candidate function mappings for DMM6500. Firmware/configuration and
    // option support must be verified on the actual instrument.
    switch (m) {
        case MeasurementMode::DC_VOLTAGE: case MeasurementMode::AC_VOLTAGE:
        case MeasurementMode::DC_CURRENT: case MeasurementMode::AC_CURRENT:
        case MeasurementMode::TWO_WIRE_RES: case MeasurementMode::FOUR_WIRE_RES:
        case MeasurementMode::TEMPERATURE: case MeasurementMode::CONTINUITY:
        case MeasurementMode::DIODE: case MeasurementMode::FREQUENCY:
        case MeasurementMode::PERIOD: case MeasurementMode::CAPACITANCE: return true;
    }
    return false;
}

bool Keithley6500Driver::ConfigureMeasurement(MeasurementMode mode) {
    if (!Supports(mode)) {
        SetError("Measurement mode is not supported by this adapter.");
        return false;
    }

    const ModeInfo info = Info(mode);
    const string command = string(":SENS:FUNC \"")
                     + info.scpi + "\"";
    if (!SendSCPI(command)) return false;

    currentMode_ = mode;
    if (!CheckInstrumentError()) return false;
    lastError_.clear();
    return true;
}

bool Keithley6500Driver::GetCurrentFunction(string& function) { return QuerySCPI(":SENS:FUNC?", function); }

bool Keithley6500Driver::SendSCPI(const string& command) {
    if (!IsConnected()) { SetError("Instrument is not connected."); return false; }
    string msg = command;
    if (Trim(msg).empty()) { SetError("SCPI command cannot be empty."); return false; }
    if (msg.back() != '\n') msg += '\n';
    ViUInt32 written = 0;
    ViStatus status = viWrite(session_, reinterpret_cast<ViBuf>(const_cast<char*>(msg.data())), static_cast<ViUInt32>(msg.size()), &written);
    if (status < VI_SUCCESS) { SetError("viWrite failed; VISA status=" + to_string(status) + ", command='" + Trim(command) + "'"); return false; }
    if (written != msg.size()) { SetError("Short SCPI write: sent " + to_string(written) + " of " + to_string(msg.size()) + " bytes."); return false; }
    lastError_.clear(); return true;
}

bool Keithley6500Driver::QuerySCPI(const string& command, string& response) {
    response.clear();
    if (!SendSCPI(command)) return false;
    string all;
    char buffer[1024] = {};
    for (int i = 0; i < 16; ++i) {
        ViUInt32 got = 0;
        ViStatus status = viRead(session_, reinterpret_cast<ViBuf>(buffer), sizeof(buffer), &got);
        if (got > 0) all.append(buffer, got);
        if (status == VI_SUCCESS_TERM_CHAR || status == VI_SUCCESS_MAX_CNT || status == VI_SUCCESS) break;
        if (status < VI_SUCCESS) {
            SetError("viRead failed; VISA status=" + to_string(status) + ", query='" + Trim(command) + "'"); return false;
        }
        if (got == 0) break;
    }
    response = Trim(all);
    if (response.empty()) { SetError("Empty response to query '" + Trim(command) + "'. Check SCPI support, termination, and instrument state."); return false; }
    lastError_.clear(); return true;
}

bool Keithley6500Driver::CheckInstrumentError() {
    string response;
    if (!QuerySCPI(":SYST:ERR?", response)) return false;
    // Expected SCPI error format begins with an integer code, e.g. 0,"No error".
    char* end = nullptr;
    long code = strtol(response.c_str(), &end, 10);
    if (end == response.c_str()) { SetError("Could not parse instrument error response: " + response); return false; }
    if (code != 0) { SetError("Instrument SCPI error " + to_string(code) + ": " + response); return false; }
    return true;
}

bool Keithley6500Driver::ReadMeasurement(Measurement& m) {
    m = Measurement{};
    m.function = Info(currentMode_).name; m.unit = Info(currentMode_).unit;
    string response;
    if (!QuerySCPI(":READ?", response)) { m.error = lastError_; return false; }
    m.raw = response;
    // Many DMMs return comma-separated readings/status fields. Parse the first field only.
    string first = response.substr(0, response.find(','));
    first = Trim(first);
    char* end = nullptr;
    const double value = strtod(first.c_str(), &end);
    if (end == first.c_str() || *end != '\0' || !isfinite(value) || fabs(value) > 1.0e30) {
        SetError("Response is not a valid finite numeric reading: '" + response + "'"); m.error = lastError_; return false;
    }
    if (!CheckInstrumentError()) { m.error = lastError_; return false; }
    m.ok = true; m.value = value; m.error.clear(); lastError_.clear(); return true;
}

string Keithley6500Driver::ExecuteCommand(const string& input) {
    string cmd = input;
    const auto a = cmd.find_first_not_of(" \t\r\n");
    if (a == string::npos) { SetError("SCPI command cannot be empty."); return {}; }
    const auto b = cmd.find_last_not_of(" \t\r\n"); cmd = cmd.substr(a, b-a+1);
    if (cmd.find('?') != string::npos) { string response; return QuerySCPI(cmd, response) ? response : string{}; }
    return SendSCPI(cmd) ? "Command sent; instrument acceptance not yet verified." : string{};
}
string Keithley6500Driver::LastError() const { return lastError_; }
