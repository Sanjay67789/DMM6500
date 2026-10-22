#pragma once

#include "IDMMDriver.h"
#include <visa.h>

#include <string>

class Keithley6500Driver final : public IDMMDriver {
public:
    Keithley6500Driver() = default;
    ~Keithley6500Driver() override;

    Keithley6500Driver(const Keithley6500Driver&) = delete;
    Keithley6500Driver& operator=(const Keithley6500Driver&) = delete;

    bool Connect(const std::string& visaAddress) override;
    void Disconnect() override;
    bool IsConnected() const override;
    std::string GetIdentity() override;
    std::string GetAddress() const override;
    bool Reset() override;
    bool ConfigureMeasurement(MeasurementMode mode, double range = -1.0) override;
    bool GetCurrentFunction(std::string& function) override;
    bool ReadValue(double& value) override;
    std::string ExecuteCommand(const std::string& scpiCmd) override;
    std::string LastError() const override;

private:
    bool SendSCPI(const std::string& command);
    bool QuerySCPI(const std::string& command, std::string& response);
    void SetError(const std::string& message);

    ViSession defaultRM_ = VI_NULL;
    ViSession session_ = VI_NULL;
    std::string address_;
    std::string lastError_;
};
