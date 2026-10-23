# DMM6500 CMake + SCPI quoting fix

This package is a complete replacement for the current CMake source tree. It includes the range-free measurement selection and engineering-prefix display overlay, plus the fixes below.

## Fixes
- CMake compiles all four required source files: `main.cpp`, `DMMFactory.cpp`, `Keithley6500Driver.cpp`, and `LiveJsonPublisher.cpp`.
- The `:SENS:FUNC` command is now built with literal SCPI single quotes, e.g. `:SENS:FUNC 'VOLT:DC'` and `:SENS:FUNC 'CONT'`. This avoids the malformed C++ escaping that caused the compiler errors and the instrument's error 1135.
- The measurement range prompt remains removed.

## Apply
Back up your current `include`, `src`, and `CMakeLists.txt`, then copy this package's `include`, `src`, and `CMakeLists.txt` into the project root, preserving the folder structure.

Build from PowerShell in the project root:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Run:

```powershell
.\build\Debug\DMM6500_Console.exe
```

## Notes
- This CMake configuration targets Windows with MSVC and the NI-VISA `visa64.lib` shown in the project setup. It is not a MinGW link configuration.
- The SCPI function syntax follows Keithley/Tektronix DMM6500 command examples. Hardware-test continuity and then DC voltage on the physical instrument before enabling live logging.
- Do not assume all listed measurement functions are supported by every manufacturer's 6.5-digit DMM; each model needs a validated adapter.
