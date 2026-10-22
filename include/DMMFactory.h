#pragma once

#include "IDMMDriver.h"
#include "Keithley6500Driver.h"
#include <memory>

enum class DMMType {
    KEITHLEY_6500
};

class DMMFactory {
public:
    static std::unique_ptr<IDMMDriver> CreateDriver(DMMType type) {
        switch (type) {
        case DMMType::KEITHLEY_6500:
            return std::make_unique<Keithley6500Driver>();
        }
        return nullptr;
    }
};
