
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

namespace {

// ---------------- Configuration ----------------

constexpr char PLC_IP[] = "192.168.1.5";
constexpr unsigned short PLC_PORT = 502;
constexpr unsigned char UNIT_ID = 1; // Verify for your PLC

// Set to the VERIFIED Modbus coil address.
// -1 disables output writes until the address is confirmed.
constexpr int OUTPUT_COIL = -1;

constexpr char JSON_FILE[] = "latest.json";
constexpr int BLINK_INTERVAL_MS = 500;
constexpr int JSON_MAX_AGE_SECONDS = 5;

// ------------------------------------------------

bool readDmmConnected()
{
    std::ifstream file(JSON_FILE);
    if (!file) {
        std::cerr << "[JSON] Cannot open " << JSON_FILE << '\n';
        return false;
    }

    const std::string json(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    // Read a field from the current flat JSON schema.
    auto field = [&](const std::string& key) -> std::string {
        const std::string token = "\"" + key + "\"";
        auto p = json.find(token);

        if (p == std::string::npos)
            return {};

        p = json.find(':', p + token.size());
        if (p == std::string::npos)
            return {};

        p = json.find_first_not_of(" \t\r\n", p + 1);
        if (p == std::string::npos)
            return {};

        if (json[p] == '"') {
            const auto end = json.find('"', p + 1);
            if (end == std::string::npos)
                return {};
            return json.substr(p + 1, end - p - 1);
        }

        const auto end = json.find_first_of(",}\r\n \t", p);
        return json.substr(p, end - p);
    };

    if (field("connected") != "true")
        return false;

    const std::string status = field("status");
    if (status != "OK" && status != "CONNECTED")
        return false;

    // Expect UTC timestamp in the format YYYY-MM-DDTHH:MM:SSZ.
    const std::string timestamp = field("timestamp_utc");

    if (timestamp.size() != 20 ||
        timestamp[4] != '-' || timestamp[7] != '-' ||
        timestamp[10] != 'T' || timestamp[13] != ':' ||
        timestamp[16] != ':' || timestamp[19] != 'Z') {
        return false;
    }

    std::tm tm{};
    tm.tm_year = std::stoi(timestamp.substr(0, 4)) - 1900;
    tm.tm_mon  = std::stoi(timestamp.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(timestamp.substr(8, 2));
    tm.tm_hour = std::stoi(timestamp.substr(11, 2));
    tm.tm_min  = std::stoi(timestamp.substr(14, 2));
    tm.tm_sec  = std::stoi(timestamp.substr(17, 2));

    const std::time_t sampleTime = _mkgmtime(&tm);
    if (sampleTime == static_cast<std::time_t>(-1))
        return false;

    const std::time_t now = std::time(nullptr);
    const double age = std::difftime(now, sampleTime);

    if (!std::isfinite(age) ||
        age < -5 ||
        age > JSON_MAX_AGE_SECONDS) {
        std::cerr << "[JSON] Status is stale or timestamp is invalid.\n";
        return false;
    }

    return true;
}

// Send a complete buffer, even if send() writes only part of it.
bool sendAll(SOCKET s, const unsigned char* data, int length)
{
    int sent = 0;

    while (sent < length) {
        const int n = send(
            s,
            reinterpret_cast<const char*>(data) + sent,
            length - sent,
            0);

        if (n <= 0)
            return false;

        sent += n;
    }

    return true;
}

// Read a complete response.
bool recvAll(SOCKET s, unsigned char* data, int length)
{
    int received = 0;

    while (received < length) {
        const int n = recv(
            s,
            reinterpret_cast<char*>(data) + received,
            length - received,
            0);

        if (n <= 0)
            return false;

        received += n;
    }

    return true;
}

// Modbus TCP function 0x05: Write Single Coil.
bool writeOutput(bool on)
{
    if (OUTPUT_COIL < 0 || OUTPUT_COIL > 65535) {
        std::cerr
            << "[PLC] Output coil is not configured; no write sent.\n";
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "[PLC] Socket creation failed.\n";
        return false;
    }

    DWORD timeoutMs = 1500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(PLC_PORT);

    if (inet_pton(AF_INET, PLC_IP, &server.sin_addr) != 1 ||
        connect(sock, reinterpret_cast<sockaddr*>(&server),
                sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "[PLC] TCP connection failed.\n";
        closesocket(sock);
        return false;
    }

    static std::uint16_t transaction = 0;
    const std::uint16_t tid = ++transaction;
    const auto coil = static_cast<std::uint16_t>(OUTPUT_COIL);

    const unsigned char request[12] = {
        static_cast<unsigned char>(tid >> 8),
        static_cast<unsigned char>(tid),
        0x00, 0x00, // Protocol ID
        0x00, 0x06, // Length
        UNIT_ID,
        0x05,       // Write Single Coil
        static_cast<unsigned char>(coil >> 8),
        static_cast<unsigned char>(coil),
        static_cast<unsigned char>(on ? 0xFF : 0x00),
        0x00
    };

    unsigned char response[8]{};

    const bool sent = sendAll(sock, request, sizeof(request));
    // A normal Modbus Write Single Coil response is 12 bytes.
    unsigned char fullResponse[12]{};
    const bool received = sent &&
        recvAll(sock, fullResponse, sizeof(fullResponse));

    closesocket(sock);

    if (!received) {
        std::cerr << "[PLC] No complete Modbus response.\n";
        return false;
    }

    const bool valid =
        fullResponse[0] == request[0] &&
        fullResponse[1] == request[1] &&
        fullResponse[2] == 0 &&
        fullResponse[3] == 0 &&
        fullResponse[4] == 0 &&
        fullResponse[5] == 6 &&
        fullResponse[6] == UNIT_ID &&
        fullResponse[7] == 0x05 &&
        fullResponse[8] == request[8] &&
        fullResponse[9] == request[9] &&
        fullResponse[10] == request[10] &&
        fullResponse[11] == request[11];

    if (!valid) {
        std::cerr << "[PLC] Modbus exception or unexpected response.\n";
        return false;
    }

    std::cout << "[PLC] Output command acknowledged: "
              << (on ? "ON" : "OFF") << '\n';
    return true;
}

} // namespace

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock initialization failed.\n";
        return 1;
    }

    std::cout << "DMM JSON -> PLC output test\n"
              << "PLC: " << PLC_IP << ':' << PLC_PORT << '\n'
              << "JSON: " << JSON_FILE << '\n'
              << "Press Ctrl+C to stop.\n";

    bool outputState = false;
    bool previousDmmConnected = false;
    bool stateKnown = false;
    auto lastToggle = std::chrono::steady_clock::now();

    while (true) {
        const bool dmmConnected = readDmmConnected();

        if (!dmmConnected) {
            // Request OFF once after disconnection, or at startup.
            if (!stateKnown || outputState) {
                if (writeOutput(false)) {
                    outputState = false;
                    stateKnown = true;
                }
            }

            previousDmmConnected = false;
        } else {
            if (!previousDmmConnected) {
                outputState = false;
                lastToggle = std::chrono::steady_clock::now();
                std::cout << "[DMM] Connected and JSON is fresh.\n";
            }

            const auto now = std::chrono::steady_clock::now();

            if (now - lastToggle >=
                std::chrono::milliseconds(BLINK_INTERVAL_MS)) {
                const bool nextState = !outputState;

                if (writeOutput(nextState)) {
                    outputState = nextState;
                    stateKnown = true;
                    lastToggle = now;
                }
            }

            previousDmmConnected = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    WSACleanup();
    return 0;
}
