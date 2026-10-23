<div align="center">

# DMM6500 Console Driver

**A C++17, VISA-based framework for discovering digital multimeters, configuring measurements, and publishing live readings to JSON.**

<p>
  <img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" alt="C++17">
  <img src="https://img.shields.io/badge/Instrument-VISA-6A5ACD?style=for-the-badge" alt="VISA">
  <img src="https://img.shields.io/badge/Output-Live%20JSON-2E8B57?style=for-the-badge" alt="Live JSON">
  <img src="https://img.shields.io/badge/Platform-Windows-0078D4?style=for-the-badge&logo=windows&logoColor=white" alt="Windows">
</p>

*A modular starting point for instrument automation. Validate hardware behavior before relying on it in production.*

</div>

---

## 📑 Contents

- [DMM6500 Console Driver](#dmm6500-console-driver)
  - [📑 Contents](#-contents)
  - [🔍 Overview](#-overview)
  - [✨ Features](#-features)
  - [🏗️ Architecture](#️-architecture)
    - [📁 Project Structure](#-project-structure)
    - [🧩 Component Responsibilities](#-component-responsibilities)
  - [⚙️ Requirements](#️-requirements)
  - [🚀 Build and Run](#-build-and-run)
    - [1. Configure the Project](#1-configure-the-project)
    - [2. Build](#2-build)
    - [3. Run](#3-run)
    - [4. Optional Command-Line Arguments](#4-optional-command-line-arguments)
  - [🖥️ Using the Console](#️-using-the-console)
    - [Console Menu](#console-menu)
  - [📡 Live JSON Output](#-live-json-output)
    - [Example JSON Snapshot](#example-json-snapshot)
    - [Reading JSON from Another Program](#reading-json-from-another-program)
    - [Snapshot vs. History](#snapshot-vs-history)
  - [📐 Engineering-Unit Formatting](#-engineering-unit-formatting)
  - [🔌 Adding Another DMM Model](#-adding-another-dmm-model)
  - [🛠️ Troubleshooting](#️-troubleshooting)
  - [⚠️ Safety and Limitations](#️-safety-and-limitations)
  - [🗺️ Roadmap](#️-roadmap)

---

## 🔍 Overview

DMM6500 Console Driver is a C++17 console application that communicates with supported digital multimeters through VISA and SCPI. It provides a common driver interface, a model-selection factory, a console workflow, and a JSON snapshot that another application can read.

> **Current scope:** The factory currently includes a Keithley DMM6500 adapter. The common interface is designed for extension, but other manufacturers and models require their own implemented and tested adapters.

## ✨ Features

| Feature | Description |
|:---|:---|
| **VISA Discovery** | Enumerates VISA `INSTR` resources and queries `*IDN?`. |
| **Driver Interface** | Provides common connection, configuration, reading, and error-handling methods. |
| **Measurement Menu** | Represents DC/AC voltage, DC/AC current, 2-wire/4-wire resistance, temperature, continuity, diode, frequency, period, and capacitance in the current menu. |
| **Error Reporting** | Reports VISA failures, parsing failures, and instrument SCPI errors where implemented. |
| **Live JSON Snapshot** | Replaces a JSON file with the latest measurement or error state. |
| **Engineering Display** | Can display values using engineering prefixes, such as `1 µV`, `1 kΩ`, or `1 MHz`. |
| **Custom SCPI** | Provides a console option for sending a command or query to the active instrument. |

> **Important:** A menu entry or enum does not, by itself, prove that a function works on every instrument. Validate each SCPI mapping against the specific model, firmware, terminal configuration, and instrument manual.

---

## 🏗️ Architecture

```text
                 Console Application
                     src/main.cpp
                          |
                          v
                      IDMMDriver
                          |
             +------------+------------+
             |                         |
             v                         v
         DMMFactory             LiveJsonPublisher
             |
             v
      Keithley6500Driver
             |
             v
      VISA / SCPI Interface
             |
             v
      Physical DMM6500
```

### 📁 Project Structure

```text
dmm6.5k/
├── include/
│   ├── DMMFactory.h
│   ├── EngineeringFormat.h
│   ├── IDMMDriver.h
│   ├── Keithley6500Driver.h
│   └── LiveJsonPublisher.h
│
├── src/
│   ├── DMMFactory.cpp
│   ├── Keithley6500Driver.cpp
│   ├── LiveJsonPublisher.cpp
│   └── main.cpp
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```

### 🧩 Component Responsibilities

| Component | Responsibility |
|:---|:---|
| `IDMMDriver` | Defines the common driver contract and measurement data types. |
| `DMMFactory` | Selects a driver based on instrument identity. |
| `Keithley6500Driver` | Handles VISA sessions and model-specific SCPI commands. |
| `LiveJsonPublisher` | Writes timestamped JSON snapshots. |
| `EngineeringFormat.h` | Converts numeric values into readable engineering-prefix strings. |
| `main.cpp` | Handles discovery, the console menu, the reading loop, and user interaction. |

---

## ⚙️ Requirements

- Windows 10 or Windows 11
- CMake
- Visual Studio Build Tools / MSVC with a Windows SDK
- NI-VISA or a compatible VISA implementation
- Keithley DMM6500 connected through a VISA-supported interface
- Git (optional, for version control)

The CMake project must point to the VISA header and library installed on your machine. Do not assume another computer uses the same installation paths.

---

## 🚀 Build and Run

Open PowerShell in the repository root.

### 1. Configure the Project

```powershell
cmake -S . -B build
```

### 2. Build

```powershell
cmake --build build --config Debug
```

### 3. Run

```powershell
.\build\Debug\DMM6500_Console.exe
```

### 4. Optional Command-Line Arguments

Some project versions may support an output path and update interval, for example:

```powershell
.\build\Debug\DMM6500_Console.exe .\latest.json 1000
```

This example requests `latest.json` and a 1000 ms update interval. Confirm the argument handling in your checked-out `src/main.cpp`; command-line behavior depends on the version you built.

---

## 🖥️ Using the Console

Typical workflow:

1. Connect and power on the instrument.
2. Launch the application and select a discovered, supported DMM.
3. Choose **Configure Measurement** and select a function.
4. Choose **Read Once** to take one reading.
5. Choose **Live Update Loop** to repeatedly publish readings.
6. Press `q` to stop live mode, where supported.
7. Open the JSON file to inspect the latest state.

### Console Menu

| Option | Action |
|:---:|:---|
| `1` | Configure measurement |
| `2` | Read once |
| `3` | Live update loop |
| `4` | Send a custom SCPI command or query |
| `5` | Show the last driver error |
| `0` | Exit |

The exact menu and supported options depend on the source version currently built.

---

## 📡 Live JSON Output

By default, the application writes `latest.json` relative to its **current working directory**. If a path is supplied on the command line and that build supports it, that path is used instead.

### Example JSON Snapshot

```json
{
  "schema_version": 1,
  "timestamp_utc": "2026-10-23T06:45:12Z",
  "connected": true,
  "manufacturer": "Keithley Instruments",
  "model": "DMM6500",
  "identity": "KEITHLEY INSTRUMENTS,MODEL DMM6500,...",
  "visa_address": "USB0::...::INSTR",
  "function": "DC_VOLTAGE",
  "value": 1.4998,
  "raw": "+1.49980000E+00",
  "unit": "V",
  "status": "OK",
  "error": ""
}
```

> This is an illustrative schema. Actual fields and values depend on the current implementation and instrument response.

### Reading JSON from Another Program

Example using Python:

```python
import json
from pathlib import Path

path = Path("latest.json")

with path.open("r", encoding="utf-8") as file:
    data = json.load(file)

if data.get("status") == "OK" and data.get("value") is not None:
    print(data["value"], data.get("unit", ""))
else:
    print("Measurement unavailable:", data.get("error", "unknown error"))
```

Use the numeric `value` field for calculations. A display string such as `1 kΩ` is intended for people; the machine-readable value should remain numeric in base units, for example `1000` with unit `ohm`.

### Snapshot vs. History

`latest.json` is a **latest-value snapshot**, not a measurement history. Each new reading replaces the previous snapshot.

To preserve historical measurements, add a separate CSV or JSON Lines logger rather than trying to store every reading in this file.

---

## 📐 Engineering-Unit Formatting

Engineering prefixes make console output easier to read.

| Numeric Value | Unit | Example Display |
|---:|:---:|:---|
| `0.000001` | V | `1 µV` |
| `0.001` | V | `1 mV` |
| `1000` | ohm | `1 kΩ` |
| `1000000` | Hz | `1 MHz` |

The exact output depends on the formatter implementation and supported unit mappings. Keep raw numeric values and units separate from formatted display text.

---

## 🔌 Adding Another DMM Model

Do not assume all manufacturers use identical SCPI commands.

1. Create a new adapter implementing `IDMMDriver`.
2. Implement connection/disconnection, identity, supported functions, configuration, reading, and error handling.
3. Verify commands and response parsing against the specific instrument manual.
4. Add a model/vendor match to `DMMFactory`.
5. Test discovery, normal readings, timeouts, unsupported functions, instrument error queues, and disconnect/reconnect behavior.
6. Document the model and firmware versions tested.

The factory currently recognizes the Keithley DMM6500 adapter. Other vendors are not supported merely because they appear in the roadmap.

---

## 🛠️ Troubleshooting

<details>
<summary><strong>CMake cannot find VISA</strong></summary>

Check the VISA include and library paths in `CMakeLists.txt`. Confirm that the SDK and architecture match the compiler, for example, 64-bit MSVC and 64-bit VISA libraries.

Run:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

</details>

<details>
<summary><strong>No supported instrument is discovered</strong></summary>

- Confirm the instrument is powered on and connected.
- Confirm it appears in the VISA vendor utility.
- Verify the VISA resource address.
- Check that `*IDN?` returns an identity string.
- Confirm the factory recognizes that manufacturer and model.

</details>

<details>
<summary><strong>SCPI error 1135: expected closing quotes</strong></summary>

This indicates a malformed quoted SCPI parameter. Inspect the exact command sent by the driver and compare it with the syntax in the DMM6500 reference manual.

C++ string escaping and SCPI quoting are separate layers. Log the final command string to diagnose it.

</details>

<details>
<summary><strong><code>latest.json</code> is missing or not changing</strong></summary>

- Check the output path printed by the console.
- Remember that relative paths are resolved from the current working directory.
- Configure a measurement before reading.
- Check whether the reading succeeded or the JSON publisher reported a file-write error.
- Ensure the application has permission to write to the target directory.

</details>

<details>
<summary><strong>Git reports that LF will be replaced by CRLF</strong></summary>

These are Git line-ending conversion warnings on Windows, not compiler errors. If consistent line endings matter for the team, configure a `.gitattributes` file.

</details>

---

## ⚠️ Safety and Limitations

- Verify the correct terminals, measurement function, input limits, leads, fuses, and category ratings before connecting a source.
- Never connect a current input directly across a voltage source.
- Disconnect power and discharge capacitors before resistance, continuity, diode, or capacitance checks where appropriate.
- Treat SCPI commands, auto-ranging, and terminal routing as instrument-specific.
- Do not use this project as a safety interlock.
- A successful VISA connection does not guarantee a valid measurement.
- Validate every supported function on actual hardware before relying on results in production.

---

## 🗺️ Roadmap

- [ ] Validate all advertised measurement functions on a physical DMM6500.
- [ ] Add automated unit tests for parsing and engineering formatting.
- [ ] Improve timeout, reconnect, and partial-response recovery.
- [ ] Add optional CSV / JSON Lines history logging.
- [ ] Add a documented, versioned JSON schema.
- [ ] Add tested adapters for additional DMM models.
- [ ] Add CI build checks and release packaging.

---

<div align="center">

**Built for instrument automation and measurement workflows.**

<sub>Contributions and tested hardware reports are welcome.</sub>

</div>