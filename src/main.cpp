
#include "DMMFactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace std;

struct DMMDevice {
    string address;
    string identity;
    unique_ptr<IDMMDriver> driver;
};

string Lower(string text) {
    transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) {
            return static_cast<char>(tolower(c));
        });

    return text;
}

string Upper(string text) {
    transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) {
            return static_cast<char>(toupper(c));
        });

    return text;
}

void ClearBadInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

bool IsDMM6500(const string& identity) {
    return Lower(identity).find("dmm6500") != string::npos;
}

// Discover all connected DMM6500 instruments.
bool DiscoverDMM6500s(vector<DMMDevice>& devices) {
    devices.clear();

    ViSession resourceManager = VI_NULL;

    ViStatus status = viOpenDefaultRM(&resourceManager);

    if (status < VI_SUCCESS) {
        cout << "Cannot open VISA resource manager.\n";
        return false;
    }

    ViFindList findList = VI_NULL;
    ViUInt32 count = 0;
    char descriptor[VI_FIND_BUFLEN] = {};

    status = viFindRsrc(
        resourceManager,
        const_cast<ViString>("?*INSTR"),
        &findList,
        &count,
        descriptor
    );

    if (status < VI_SUCCESS) {
        viClose(resourceManager);
        cout << "No VISA instruments discovered.\n";
        return true;
    }

    vector<string> addresses;
    addresses.push_back(descriptor);

    for (ViUInt32 i = 1; i < count; i++) {
        status = viFindNext(findList, descriptor);

        if (status < VI_SUCCESS) {
            break;
        }

        addresses.push_back(descriptor);
    }

    viClose(findList);
    viClose(resourceManager);

    for (const string& address : addresses) {
        unique_ptr<IDMMDriver> driver =
            DMMFactory::CreateDriver(DMMType::KEITHLEY_6500);

        if (!driver) {
            continue;
        }

        if (!driver->Connect(address)) {
            continue;
        }

        string identity = driver->GetIdentity();

        if (IsDMM6500(identity)) {
            DMMDevice device;
            device.address = address;
            device.identity = identity;
            device.driver = move(driver);

            devices.push_back(move(device));
        }
    }

    return true;
}

void PrintDevices(const vector<DMMDevice>& devices, int activeIndex) {
    cout << "\n--- Discovered Keithley DMM6500 Instruments ---\n";

    if (devices.empty()) {
        cout << "No Keithley DMM6500 instruments found.\n";
        return;
    }

    for (size_t i = 0; i < devices.size(); i++) {
        cout << "[" << i << "] " << devices[i].identity << "\n";
        cout << "    VISA: " << devices[i].address;

        if (static_cast<int>(i) == activeIndex) {
            cout << "  <-- ACTIVE";
        }

        cout << "\n";
    }
}

void PrintMenu(size_t count, int activeIndex) {
    cout << "\n=============================================\n";
    cout << "       KEITHLEY DMM6500 CONSOLE\n";
    cout << "=============================================\n";
    cout << "Discovered DMM6500s: " << count << "\n";

    if (activeIndex >= 0 &&
        static_cast<size_t>(activeIndex) < count) {
        cout << "Active DMM index: [" << activeIndex << "]\n";
    } else {
        cout << "Active DMM: none\n";
    }

    cout << "---------------------------------------------\n";
    cout << "1. List discovered DMM6500s / select device\n";
    cout << "2. Choose measurement function\n";
    cout << "3. Read value\n";
    cout << "4. Send custom SCPI command\n";
    cout << "5. Reset active DMM (*RST)\n";
    cout << "6. Rediscover instruments\n";
    cout << "0. Exit\n";
    cout << "=============================================\n";
    cout << "Enter choice: ";
}

bool SelectActiveDevice(
    const vector<DMMDevice>& devices,
    int& activeIndex
) {
    if (devices.empty()) {
        cout << "No DMM6500 is available.\n";
        return false;
    }

    if (devices.size() == 1) {
        activeIndex = 0;
        cout << "Only one DMM6500 found. Selected automatically.\n";
        return true;
    }

    PrintDevices(devices, activeIndex);

    cout << "Select DMM index: ";

    int selected;

    if (!(cin >> selected)) {
        ClearBadInput();
        cout << "Invalid input.\n";
        return false;
    }

    if (selected < 0 ||
        static_cast<size_t>(selected) >= devices.size()) {
        cout << "Index out of range.\n";
        return false;
    }

    activeIndex = selected;

    cout << "Active DMM is now [" << activeIndex << "].\n";
    return true;
}

bool HasActive(const vector<DMMDevice>& devices, int activeIndex) {
    if (activeIndex >= 0 &&
        static_cast<size_t>(activeIndex) < devices.size() &&
        devices[activeIndex].driver &&
        devices[activeIndex].driver->IsConnected()) {
        return true;
    }

    cout << "No active DMM6500. Run discovery first.\n";
    return false;
}

const char* ModeName(MeasurementMode mode) {
    switch (mode) {
        case MeasurementMode::DC_VOLTAGE:
            return "DC Voltage";

        case MeasurementMode::AC_VOLTAGE:
            return "AC Voltage";

        case MeasurementMode::DC_CURRENT:
            return "DC Current";

        case MeasurementMode::AC_CURRENT:
            return "AC Current";

        case MeasurementMode::TWO_WIRE_RES:
            return "2-Wire Resistance";

        case MeasurementMode::FOUR_WIRE_RES:
            return "4-Wire Resistance";

        case MeasurementMode::TEMPERATURE:
            return "Temperature";
    }

    return "Unknown";
}

const char* UnitForFunction(const string& function) {
    string f = Lower(function);

    if (f.find("volt") != string::npos) {
        return "V";
    }

    if (f.find("curr") != string::npos) {
        return "A";
    }

    if (f.find("res") != string::npos ||
        f.find("fres") != string::npos) {
        return "ohm";
    }

    if (f.find("temp") != string::npos) {
        return "temperature units configured on instrument";
    }

    return "instrument units";
}

bool ChooseFunction(IDMMDriver& driver) {
    cout << "\nChoose measurement function:\n";
    cout << "1. DC Voltage\n";
    cout << "2. AC Voltage\n";
    cout << "3. DC Current\n";
    cout << "4. AC Current\n";
    cout << "5. 2-Wire Resistance\n";
    cout << "6. 4-Wire Resistance\n";
    cout << "7. Temperature\n";
    cout << "Choice: ";

    int choice;

    if (!(cin >> choice)) {
        ClearBadInput();
        cout << "Invalid input.\n";
        return false;
    }

    MeasurementMode mode;

    switch (choice) {
        case 1:
            mode = MeasurementMode::DC_VOLTAGE;
            break;

        case 2:
            mode = MeasurementMode::AC_VOLTAGE;
            break;

        case 3:
            mode = MeasurementMode::DC_CURRENT;
            break;

        case 4:
            mode = MeasurementMode::AC_CURRENT;
            break;

        case 5:
            mode = MeasurementMode::TWO_WIRE_RES;
            break;

        case 6:
            mode = MeasurementMode::FOUR_WIRE_RES;
            break;

        case 7:
            mode = MeasurementMode::TEMPERATURE;
            break;

        default:
            cout << "Invalid function choice.\n";
            return false;
    }

    if (!driver.ConfigureMeasurement(mode)) {
        cout << "Configuration failed: "
             << driver.LastError() << "\n";
        return false;
    }

    string actualFunction;

    if (driver.GetCurrentFunction(actualFunction)) {
        cout << "Configured " << ModeName(mode)
             << "; instrument reports: "
             << actualFunction << "\n";
    } else {
        cout << "Function selected, but verification failed: "
             << driver.LastError() << "\n";
    }

    return true;
}

int main() {
    vector<DMMDevice> devices;
    int activeIndex = -1;

    cout << "Discovering connected Keithley DMM6500 instruments...\n";

    if (!DiscoverDMM6500s(devices)) {
        cout << "Initial discovery failed.\n";
    }

    if (devices.size() == 1) {
        activeIndex = 0;
    } else if (devices.size() > 1) {
        SelectActiveDevice(devices, activeIndex);
    }

    PrintDevices(devices, activeIndex);

    int choice = -1;

    while (choice != 0) {
        PrintMenu(devices.size(), activeIndex);

        if (!(cin >> choice)) {
            ClearBadInput();
            cout << "Please enter a numeric menu choice.\n";
            continue;
        }

        switch (choice) {
            case 1:
                PrintDevices(devices, activeIndex);
                SelectActiveDevice(devices, activeIndex);
                break;

            case 2:
                if (HasActive(devices, activeIndex)) {
                    ChooseFunction(*devices[activeIndex].driver);
                }
                break;

            case 3: {
                if (!HasActive(devices, activeIndex)) {
                    break;
                }

                string function;

                if (!devices[activeIndex].driver->
                    GetCurrentFunction(function)) {
                    cout << "Cannot query function: "
                         << devices[activeIndex].driver->LastError()
                         << "\n";
                    break;
                }

                double value = 0.0;

                if (!devices[activeIndex].driver->ReadValue(value)) {
                    cout << "Measurement failed: "
                         << devices[activeIndex].driver->LastError()
                         << "\n";
                    break;
                }

                // Avoid displaying extreme sentinel values as measurements.
                if (!isfinite(value) || fabs(value) > 1.0e30) {
                    cout << "Reading unavailable or invalid: "
                         << value << "\n";
                    cout << "Check the measurement setup and sensor.\n";
                    break;
                }

                cout << "DMM [" << activeIndex << "]"
                     << " | Function: " << function
                     << " | Reading: " << value << " "
                     << UnitForFunction(function) << "\n";

                break;
            }

            case 4: {
                if (!HasActive(devices, activeIndex)) {
                    break;
                }

                ClearBadInput();

                cout << "SCPI command (e.g. *IDN? or :SENS:FUNC?): ";

                string command;
                getline(cin, command);

                string response =
                    devices[activeIndex].driver->ExecuteCommand(command);

                if (response.empty()) {
                    cout << "SCPI operation failed: "
                         << devices[activeIndex].driver->LastError()
                         << "\n";
                } else {
                    cout << "Response: " << response << "\n";
                }

                break;
            }

            case 5: {
                if (!HasActive(devices, activeIndex)) {
                    break;
                }

                cout << "WARNING: *RST resets the DMM configuration.\n";
                cout << "Type YES to confirm: ";

                string confirmation;
                cin >> confirmation;

                if (Upper(confirmation) == "YES") {
                    if (devices[activeIndex].driver->Reset()) {
                        cout << "Reset command sent.\n";
                        cout << "Select the measurement function again.\n";
                    } else {
                        cout << "Reset failed: "
                             << devices[activeIndex].driver->LastError()
                             << "\n";
                    }
                } else {
                    cout << "Reset cancelled.\n";
                }

                break;
            }

            case 6: {
                // Disconnect old sessions before rediscovery.
                for (auto& device : devices) {
                    if (device.driver) {
                        device.driver->Disconnect();
                    }
                }

                devices.clear();
                activeIndex = -1;

                cout << "Rediscovering instruments...\n";

                if (!DiscoverDMM6500s(devices)) {
                    cout << "Rediscovery failed.\n";
                    break;
                }

                if (devices.size() == 1) {
                    activeIndex = 0;
                } else if (devices.size() > 1) {
                    SelectActiveDevice(devices, activeIndex);
                }

                PrintDevices(devices, activeIndex);
                break;
            }

            case 0:
                cout << "Disconnecting instruments and exiting.\n";
                break;

            default:
                cout << "Invalid choice.\n";
                break;
        }
    }

    return 0;
}
