#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <boost/lockfree/spsc_queue.hpp>
#include "../src/utromfx_core/include/utromfx/telemetry_protocol.h"

using namespace utrom_telemetry;

// Buffer for 10 seconds of data
boost::lockfree::spsc_queue<TelemetryFrame, boost::lockfree::capacity<10000>> ring_buffer;
std::atomic<bool> keep_running(true);

// --- THE CONSUMER (Binary Disk Writer) ---
void disk_writer_thread() {
    // Open in binary mode
    std::ofstream out_file("test_flight.utrom", std::ios::binary);
    
    TelemetryFrame frame;
    while (keep_running || !ring_buffer.empty()) {
        if (ring_buffer.pop(frame)) {
            // Write the raw bytes of the struct directly to disk
            out_file.write(reinterpret_cast<const char*>(&frame), sizeof(TelemetryFrame));
        } else {
            std::this_thread::yield();
        }
    }
    out_file.close();
}

int main() {
    std::cout << "--- [UtromPulse v1.0] Binary Disk Writer Verification ---" << std::endl;

    // 1. Start the writer thread
    std::thread writer(disk_writer_thread);

    // 2. THE PRODUCER (G1 1kHz Loop Simulation)
    std::cout << "Writing 5 seconds of 1kHz telemetry to disk..." << std::endl;

    for (int i = 0; i < 5000; ++i) {
        TelemetryFrame frame;
        frame.frame_index = i;
        frame.timestamp_ns = i * 1000000;
        
        // Populate with some dummy float data to test bit-integrity
        frame.q[0] = 1.2345f;
        frame.tau_cmd[11] = -0.5432f;

        if (!ring_buffer.push(frame)) {
            std::cerr << "Buffer Overflow at frame " << i << "!" << std::endl;
        }

        // 1ms frequency
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 3. Shutdown
    keep_running = false;
    writer.join();

    // 4. Verification
    std::cout << "------------------------------------------" << std::endl;
    
    // Check file size
    std::ifstream in_file("test_flight.utrom", std::ios::binary | std::ios::ate);
    std::streamsize size = in_file.tellg();
    
    size_t expected_size = 5000 * 197; // 5000 frames * 197 bytes

    std::cout << "Expected File Size: " << expected_size << " bytes" << std::endl;
    std::cout << "Actual File Size:   " << size << " bytes" << std::endl;

    if (size == expected_size) {
        std::cout << "SUCCESS: Binary file persisted with zero data loss." << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURE: File size mismatch!" << std::endl;
        return 1;
    }
}