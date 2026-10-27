# Current architecture

`src/main.cpp` owns the console menu and current VISA discovery workflow. It obtains instrument identity, asks `DMMFactory` for a compatible driver, then calls the `IDMMDriver` interface.

`DMMFactory` currently recognizes the Keithley DMM6500. `Keithley6500Driver` implements the interface using NI-VISA and SCPI. `LiveJsonPublisher` writes the current reading/status to the live JSON file and uses the header-only engineering formatter for display fields.

The folder layout separates responsibilities but does not yet mean that discovery or transport is implemented as an independent subsystem. Those are future extractions, not existing features.
