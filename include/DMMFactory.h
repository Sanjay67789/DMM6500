#pragma once
#include "IDMMDriver.h"
#include <memory>
#include <string>

enum class DMMType { KEITHLEY_6500, UNSUPPORTED };

class DMMFactory {
public:
    // Conservative: only create an adapter when identity matches a known model family.
    static std::unique_ptr<IDMMDriver> CreateForIdentity(const std::string& identity);
};
