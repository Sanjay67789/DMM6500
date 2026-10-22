
#include <iostream>
#include <visa.h>

using namespace std;

int main() {
    ViSession rm = VI_NULL;
    ViSession dmm = VI_NULL;

    ViStatus status = viOpenDefaultRM(&rm);

    if (status < VI_SUCCESS) {
        cout << "Cannot open VISA resource manager\n";
        return 1;
    }

    status = viOpen(
        rm,
        const_cast<ViRsrc>(
            "USB0::0x05E6::0x6500::04622275::INSTR"
        ),
        VI_NULL,
        3000,
        &dmm
    );

    if (status < VI_SUCCESS) {
        cout << "Cannot connect to DMM\n";
        viClose(rm);
        return 1;
    }

    cout << "DMM connected successfully\n";

    viClose(dmm);
    viClose(rm);

    return 0;
}
