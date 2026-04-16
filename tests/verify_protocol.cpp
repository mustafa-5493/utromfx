#include <iostream>
#include "../src/utromfx_core/include/utromfx/telemetry_protocol.h"

int main() {
    std::cout << "--- [UtromPulse v1.0] Protocol Verification ---" << std::endl;
    
    // We expect 197 bytes based on our struct math
    size_t expected_size = 197;
    size_t actual_size = sizeof(utrom_telemetry::TelemetryFrame);

    std::cout << "Expected Frame Size: " << expected_size << " bytes" << std::endl;
    std::cout << "Actual Frame Size:   " << actual_size << " bytes" << std::endl;

    if (actual_size == expected_size) {
        std::cout << "SUCCESS: Protocol alignment is bit-perfect." << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURE: Byte alignment mismatch! Check #pragma pack." << std::endl;
        return 1;
    }
}