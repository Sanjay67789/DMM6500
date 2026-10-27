# DMM6500 Console

Current DMM6500 console application, reorganized into responsibility-based folders. The application and driver behavior are retained; this change is primarily a source-tree organization.

## Structure

```text
dmm6.5k/
├── include/
│   ├── dmm/
│   │   ├── DMMFactory.h
│   │   ├── IDMMDriver.h
│   │   └── drivers/
│   │       ├── keithley/Keithley6500Driver.h
│   │       └── keysight/                 # reserved for a real driver
│   ├── features/
│   │   ├── formatting/EngineeringFormat.h
│   │   └── live_json/LiveJsonPublisher.h
│   ├── discovery/                        # reserved for discovery extraction
│   └── communication/                    # reserved for communication abstractions
├── src/
│   ├── main.cpp
│   ├── dmm/
│   │   ├── DMMFactory.cpp
│   │   └── drivers/
│   │       ├── keithley/Keithley6500Driver.cpp
│   │       └── keysight/                 # reserved for a real driver
│   ├── features/live_json/LiveJsonPublisher.cpp
│   ├── discovery/                        # reserved for discovery extraction
│   └── features/formatting/               # formatting is header-only
├── docs/
├── tests/
├── plc.cpp
├── latest.json
└── CMakeLists.txt
```

## Build notes

The supplied CMake configuration uses the NI-VISA MSVC import library at the configured path. This may not link with MSYS2/MinGW g++; use a compatible VISA import library or configure/build with a compatible MSVC toolchain.

`plc.cpp` is retained as a separate PLC utility and is not part of the `DMM6500_Console` target. `latest.json` is runtime output and is ignored by Git.

## Current scope

- Keithley DMM6500 driver is present.
- Keysight directories are placeholders only; no Keysight driver is implemented.
- VISA discovery remains in `src/main.cpp` in this organizational pass. The `discovery/` folders are prepared for extracting it in a later, behavior-preserving step.
