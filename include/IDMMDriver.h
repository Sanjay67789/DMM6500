#pragma once

#include <string>

enum class MeasurementMode {
    DC_VOLTAGE,
    AC_VOLTAGE,
    DC_CURRENT,
    AC_CURRENT,
    TWO_WIRE_RES,
    FOUR_WIRE_RES,
    TEMPERATURE
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

    virtual bool ConfigureMeasurement(MeasurementMode mode, double range = -1.0) = 0;
    virtual bool GetCurrentFunction(std::string& function) = 0;
    virtual bool ReadValue(double& value) = 0;
    virtual std::string ExecuteCommand(const std::string& scpiCmd) = 0;
    virtual std::string LastError() const = 0;
};
