#pragma once

#include <string>
#include <vector>

enum class MeasurementMode {
    DC_VOLTAGE, AC_VOLTAGE, DC_CURRENT, AC_CURRENT,
    TWO_WIRE_RES, FOUR_WIRE_RES, TEMPERATURE,
    CONTINUITY, DIODE, FREQUENCY, PERIOD, CAPACITANCE
};

struct Measurement {
    bool ok = false;
    double value = 0.0;
    std::string raw;
    std::string function;
    std::string unit;
    std::string error;
};

class IDMMDriver {
public:
    virtual ~IDMMDriver() = default;
    virtual bool Connect(const std::string& visaAddress) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual std::string GetIdentity() = 0;
    virtual std::string GetAddress() const = 0;
    virtual bool Reset() = 0;
    virtual bool Supports(MeasurementMode mode) const = 0;
    virtual bool ConfigureMeasurement(MeasurementMode mode) = 0;
    virtual bool GetCurrentFunction(std::string& function) = 0;
    virtual bool ReadMeasurement(Measurement& measurement) = 0;
    virtual std::string ExecuteCommand(const std::string& scpiCmd) = 0;
    virtual std::string LastError() const = 0;
};
