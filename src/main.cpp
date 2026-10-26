#include "DMMFactory.h"
#include "LiveJsonPublisher.h"
#include "EngineeringFormat.h"
#include <visa.h>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#ifdef _WIN32
#include <conio.h>
#else
#include <sys/select.h>
#include <unistd.h>
#endif
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace std;

struct FoundDevice { 
    string address; 
    unique_ptr<IDMMDriver> driver; 
    string identity; 
};

bool StopKeyPressed() {
#ifdef _WIN32
    if (_kbhit()) { 
        int c = _getch(); 
        return c == 'q' || c == 'Q';
     }
    return false;
#else
    fd_set set; FD_ZERO(&set); FD_SET(STDIN_FILENO, &set);
    timeval timeout{0, 0};
    if (select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout) > 0) {
        char c = 0;
        if (read(STDIN_FILENO, &c, 1) == 1) return c == 'q' || c == 'Q';
    }
    return false;
#endif
}

bool Discover(vector<FoundDevice>& devices) {
    devices.clear(); ViSession rm = VI_NULL;
    ViStatus s = viOpenDefaultRM(&rm);
    if (s < VI_SUCCESS) { cerr << "viOpenDefaultRM failed: " << s << "\n"; return false; }
    ViFindList list = VI_NULL; ViUInt32 count = 0; char desc[VI_FIND_BUFLEN] = {};
    s = viFindRsrc(rm, const_cast<ViString>("?*INSTR"), &list, &count, desc);
    if (s < VI_SUCCESS) { viClose(rm); cout << "No VISA INSTR resources found.\n"; return true; }
    vector<string> addresses{desc};
    for (ViUInt32 i = 1; i < count; ++i) { s = viFindNext(list, desc); if (s < VI_SUCCESS) break; addresses.emplace_back(desc); }
    if (list != VI_NULL) viClose(list);
    viClose(rm);
    for (const auto& address : addresses) {
        // Probe with a temporary VISA session to learn identity before choosing an adapter.
        ViSession probeRm = VI_NULL, probe = VI_NULL; string identity;
        if (viOpenDefaultRM(&probeRm) < VI_SUCCESS) continue;
        s = viOpen(probeRm, const_cast<ViRsrc>(address.c_str()), VI_NULL, 3000, &probe);
        if (s >= VI_SUCCESS) {
            viSetAttribute(probe, VI_ATTR_TMO_VALUE, 2000);
            viSetAttribute(probe, VI_ATTR_TERMCHAR, '\n');
            viSetAttribute(probe, VI_ATTR_TERMCHAR_EN, VI_TRUE);
            const string q = "*IDN?\n"; ViUInt32 wrote = 0;
            s = viWrite(probe, reinterpret_cast<ViBuf>(const_cast<char*>(q.data())), static_cast<ViUInt32>(q.size()), &wrote);
            if (s >= VI_SUCCESS) { char b[512] = {}; ViUInt32 got = 0; s = viRead(probe, reinterpret_cast<ViBuf>(b), sizeof(b)-1, &got); if (s >= VI_SUCCESS) identity.assign(b, got); }
            viClose(probe);
        }
        viClose(probeRm);
        auto driver = DMMFactory::CreateForIdentity(identity);
        if (!driver) { cout << "Skipped unsupported instrument: " << address << " | IDN: " << identity << "\n"; continue; }
        if (!driver->Connect(address)) { cerr << "Adapter connect failed for " << address << ": " << driver->LastError() << "\n"; continue; }
        devices.push_back({address, move(driver), identity});
    }
    return true;
}

bool ChooseMode(IDMMDriver& d, MeasurementMode& mode) {
    cout << "\nSelect function:\n1 DC voltage\n2 AC voltage\n3 DC current\n4 AC current\n5 2-wire resistance\n6 4-wire resistance\n7 temperature\n8 continuity\n9 diode\n10 frequency\n11 period\n12 capacitance\nChoice: ";
    int n; if (!(cin >> n)) { cin.clear(); cin.ignore(10000, '\n'); cout << "Invalid input.\n"; return false; }
    switch (n) {
        case 1: mode=MeasurementMode::DC_VOLTAGE; break; case 2: mode=MeasurementMode::AC_VOLTAGE; break;
        case 3: mode=MeasurementMode::DC_CURRENT; break; case 4: mode=MeasurementMode::AC_CURRENT; break;
        case 5: mode=MeasurementMode::TWO_WIRE_RES; break; case 6: mode=MeasurementMode::FOUR_WIRE_RES; break;
        case 7: mode=MeasurementMode::TEMPERATURE; break; case 8: mode=MeasurementMode::CONTINUITY; break;
        case 9: mode=MeasurementMode::DIODE; break; case 10: mode=MeasurementMode::FREQUENCY; break;
        case 11: mode=MeasurementMode::PERIOD; break; case 12: mode=MeasurementMode::CAPACITANCE; break;
        default: cout << "Unknown choice.\n"; return false;
    }
    if (!d.Supports(mode)) { cout << "This adapter does not support that mode.\n"; return false; }
    if (!d.ConfigureMeasurement(mode)) { cout << "Configuration failed: " << d.LastError() << "\n"; return false; }
    return true;
}

int main(int argc, char** argv) {
    string output = argc > 1 ? argv[1] : "latest.json";
    int intervalMs = argc > 2 ? max(100, atoi(argv[2])) : 1000;
    LiveJsonPublisher publisher(output);
    vector<FoundDevice> devices;
    if (!Discover(devices)) return 1;
    if (devices.empty()) { publisher.PublishDisconnected("", "No supported DMM adapter connected."); cerr << "No supported DMM connected. Output: " << output << "\n"; return 2; }
    cout << "Supported DMMs:\n";
    for (size_t i=0;i<devices.size();++i) cout << "[" << i << "] " << devices[i].identity << " @ " << devices[i].address << "\n";
    size_t active=0;
    if (devices.size()>1) { cout << "Select device index: "; if (!(cin>>active) || active>=devices.size()) { cerr << "Invalid device index.\n"; return 3; } }
    IDMMDriver& d = *devices[active].driver;
    MeasurementMode mode = MeasurementMode::DC_VOLTAGE;
    cout << "Live JSON path: " << output << "\nUpdate interval: " << intervalMs << " ms\n";
    bool configured=false, running=true;
    while (running) {
        cout << "\n1 Configure measurement\n2 Read once\n3 Live update loop\n4 Send SCPI query/command\n5 Show last error\n0 Exit\nChoice: ";
        int choice; if (!(cin>>choice)) { cin.clear(); cin.ignore(10000,'\n'); cout << "Invalid input.\n"; continue; }
        if (choice==0) running=false;
        else if (choice==1) configured=ChooseMode(d,mode);
        else if (choice==2 || choice==3) {
            if (!configured) { cout << "Configure a measurement first.\n"; continue; }
            bool live = choice==3;
            do {
                Measurement m;
                if (d.ReadMeasurement(m)) {
                    if (!publisher.Publish(d, devices[active].identity, m, "OK", "")) cerr << "Could not write JSON file: " << output << "\n";
                    const auto display = FormatMeasurement(m.value, m.unit);
                    cout << m.function << " = " << display.value << " " << display.unit
                         << " (raw: " << m.value << " " << m.unit << ") | JSON updated\n";
                } else {
                    m.error=d.LastError();
                    if (!publisher.Publish(d, devices[active].identity, m, "ERROR", m.error)) cerr << "Could not write JSON file: " << output << "\n";
                    cerr << "Measurement failed: " << m.error << "\n";
                }
                if (!live) break;
                if (live) {
                    cout << "Live mode running; press q to stop.\n";
                    for (int waited = 0; waited < intervalMs / 50; ++waited) {
                        if (StopKeyPressed()) { live = false; break; }
                        this_thread::sleep_for(chrono::milliseconds(50));
                    }
                }
            } while (live);
        }
        else if (choice==4) {
            cin.ignore(10000,'\n'); cout << "SCPI command: "; string cmd; getline(cin,cmd);
            string response=d.ExecuteCommand(cmd);
            if (response.empty()) cerr << "SCPI failed: " << d.LastError() << "\n"; else cout << response << "\n";
        }
        else if (choice==5) cout << (d.LastError().empty()?"No current driver error.":d.LastError()) << "\n";
        else cout << "Unknown choice.\n";
    }
    d.Disconnect(); publisher.PublishDisconnected(devices[active].identity, "Application exited.");
    cout << "Disconnected. Final JSON state written to " << output << "\n";
    return 0;
}
