#include "dmm/DMMFactory.h"
#include "dmm/drivers/keithley/Keithley6500Driver.h"
#include <algorithm>
#include <cctype>

using namespace std;

unique_ptr<IDMMDriver> DMMFactory::CreateForIdentity(const string& identity) {
    string lower = identity;
    transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    if (lower.find("keithley") != string::npos && lower.find("dmm6500") != string::npos)
        return make_unique<Keithley6500Driver>();
    // Add explicit model/vendor adapters here. Do not assume SCPI dialects are identical.
    return nullptr;
}
