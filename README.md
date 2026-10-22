# Keithley DMM6500 simplified console

## Features
- Automatically discovers VISA resources and keeps only instruments whose `*IDN?` response identifies a DMM6500.
- If one DMM6500 is found, selects it automatically. If multiple are found, displays indexed devices and asks which one to use.
- User-selectable DC/AC voltage, DC/AC current, 2-wire/4-wire resistance, and temperature functions.
- Read operation displays the active instrument index, its current function, and the reading with a best-effort unit label.
- Custom SCPI commands: commands containing `?` are treated as queries; other commands are sent without expecting a response.
- Reset option with explicit confirmation.

## Build (Windows PowerShell, from project root)
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
.\build\Release\DMM6500_Console.exe
```

If NI-VISA is installed in a different directory, pass `-DVISA_INCLUDE_DIR=...` and `-DVISA_LIBRARY=...` to CMake.

## Important notes
- The console performs discovery at startup. It opens candidate VISA resources and sends `*IDN?` to identify DMM6500 instruments.
- Discovery may time out or produce a VISA error on devices that do not answer `*IDN?`.
- `*RST` resets the active instrument's configuration and requires typing `YES`.
- Select a measurement function before reading. Ensure leads are connected to the correct terminals and within the DMM's ratings.
- Temperature support and exact function/range capabilities depend on the instrument setup and accessories.
- Range parameter is intentionally not implemented; do not assume an arbitrary range is applied.
- This version does not include JSONL logging yet; logging should be added as a separate increment after discovery, selection, measurement and SCPI flows are tested.
