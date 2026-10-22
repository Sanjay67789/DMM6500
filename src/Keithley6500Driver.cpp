
#include "Keithley6500Driver.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

using namespace std;

// Remove extra spaces and newline characters.
string Trim(string text) {
    size_t start = text.find_first_not_of(" \t\r\n");

    if (start == string::npos) {
        return "";
    }

    size_t end = text.find_last_not_of(" \t\r\n");

    return text.substr(start, end - start + 1);
}

// Close the connection automatically when the object is destroyed.
Keithley6500Driver::~Keithley6500Driver() {
    Disconnect();
}

void Keithley6500Driver::SetError(const string& message) {
    lastError_ = message;
}

// Connect to the instrument through VISA.
bool Keithley6500Driver::Connect(const string& visaAddress) {
    Disconnect();
    lastError_.clear();

    ViStatus status = viOpenDefaultRM(&defaultRM_);

    if (status < VI_SUCCESS) {
        defaultRM_ = VI_NULL;
        SetError("Cannot open VISA resource manager.");
        return false;
    }

    status = viOpen(
        defaultRM_,
        const_cast<ViRsrc>(visaAddress.c_str()),
        VI_NULL,
        3000,
        &session_
    );

    if (status < VI_SUCCESS) {
        session_ = VI_NULL;
        viClose(defaultRM_);
        defaultRM_ = VI_NULL;

        SetError("Cannot connect to instrument. VISA status: "
                 + to_string(status));
        return false;
    }

    address_ = visaAddress;

    viSetAttribute(session_, VI_ATTR_TMO_VALUE, 3000);
    viSetAttribute(session_, VI_ATTR_TERMCHAR, '\n');
    viSetAttribute(session_, VI_ATTR_TERMCHAR_EN, VI_TRUE);

    return true;
}

// Disconnect from the instrument.
void Keithley6500Driver::Disconnect() {
    if (session_ != VI_NULL) {
        viClose(session_);
        session_ = VI_NULL;
    }

    if (defaultRM_ != VI_NULL) {
        viClose(defaultRM_);
        defaultRM_ = VI_NULL;
    }

    address_.clear();
}

bool Keithley6500Driver::IsConnected() const {
    return session_ != VI_NULL;
}

string Keithley6500Driver::GetIdentity() {
    string response;

    if (!QuerySCPI("*IDN?", response)) {
        return "";
    }

    return response;
}

string Keithley6500Driver::GetAddress() const {
    return address_;
}

// Send a command to the instrument.
bool Keithley6500Driver::SendSCPI(const string& command) {
    if (!IsConnected()) {
        SetError("Instrument is not connected.");
        return false;
    }

    string message = command;

    if (message.empty() || message.back() != '\n') {
        message += '\n';
    }

    ViUInt32 written = 0;

    ViStatus status = viWrite(
        session_,
        reinterpret_cast<ViBuf>(message.data()),
        static_cast<ViUInt32>(message.size()),
        &written
    );

    if (status < VI_SUCCESS) {
        SetError("Failed to send SCPI command. VISA status: "
                 + to_string(status));
        return false;
    }

    if (written != message.size()) {
        SetError("Incomplete SCPI command was sent.");
        return false;
    }

    lastError_.clear();
    return true;
}

// Send a query and read the instrument's response.
bool Keithley6500Driver::QuerySCPI(
    const string& command,
    string& response
) {
    response.clear();

    if (!SendSCPI(command)) {
        return false;
    }

    char buffer[4096] = {};
    ViUInt32 received = 0;

    ViStatus status = viRead(
        session_,
        reinterpret_cast<ViBuf>(buffer),
        sizeof(buffer) - 1,
        &received
    );

    if (status < VI_SUCCESS) {
        SetError("Failed to read instrument response. VISA status: "
                 + to_string(status));
        return false;
    }

    response = string(buffer, received);
    response = Trim(response);

    if (response.empty()) {
        SetError("Instrument returned an empty response.");
        return false;
    }

    lastError_.clear();
    return true;
}

// Reset the instrument.
bool Keithley6500Driver::Reset() {
    return SendSCPI("*RST");
}

// Select the measurement function.
bool Keithley6500Driver::ConfigureMeasurement(
    MeasurementMode mode,
    double range
) {
    string command;

    switch (mode) {
        case MeasurementMode::DC_VOLTAGE:
            command = ":SENS:FUNC \"VOLT:DC\"";
            break;

        case MeasurementMode::AC_VOLTAGE:
            command = ":SENS:FUNC \"VOLT:AC\"";
            break;

        case MeasurementMode::DC_CURRENT:
            command = ":SENS:FUNC \"CURR:DC\"";
            break;

        case MeasurementMode::AC_CURRENT:
            command = ":SENS:FUNC \"CURR:AC\"";
            break;

        case MeasurementMode::TWO_WIRE_RES:
            command = ":SENS:FUNC \"RES\"";
            break;

        case MeasurementMode::FOUR_WIRE_RES:
            command = ":SENS:FUNC \"FRES\"";
            break;

        case MeasurementMode::TEMPERATURE:
            command = ":SENS:FUNC \"TEMP\"";
            break;

        default:
            SetError("Unsupported measurement function.");
            return false;
    }

    if (!SendSCPI(command)) {
        return false;
    }

    if (range >= 0.0) {
        SetError("Explicit measurement range is not implemented.");
        return false;
    }

    return true;
}

// Find out which measurement function is currently selected.
bool Keithley6500Driver::GetCurrentFunction(string& function) {
    return QuerySCPI(":SENS:FUNC?", function);
}

// Read and convert the measurement into a number.
bool Keithley6500Driver::ReadValue(double& value) {
    string response;

    if (!QuerySCPI(":READ?", response)) {
        return false;
    }

    try {
        size_t position = 0;
        double result = stod(response, &position);

        if (position != response.size()) {
            SetError("Invalid measurement response: " + response);
            return false;
        }

        if (!isfinite(result) || fabs(result) > 1.0e30) {
            SetError("Measurement is invalid or unavailable: " + response);
            return false;
        }

        value = result;
        lastError_.clear();

        return true;
    }
    catch (...) {
        SetError("Cannot convert measurement into a number: " + response);
        return false;
    }
}

// Execute a custom SCPI command.
string Keithley6500Driver::ExecuteCommand(const string& scpiCmd) {
    string command = Trim(scpiCmd);

    if (command.empty()) {
        SetError("SCPI command cannot be empty.");
        return "";
    }

    // Commands containing '?' are treated as queries.
    if (command.find('?') != string::npos) {
        string response;

        if (!QuerySCPI(command, response)) {
            return "";
        }

        return response;
    }

    if (!SendSCPI(command)) {
        return "";
    }

    return "Command sent successfully.";
}

string Keithley6500Driver::LastError() const {
    return lastError_;
}
