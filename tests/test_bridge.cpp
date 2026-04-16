#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <boost/lockfree/spsc_queue.hpp>
#include "../src/utromfx_core/include/utromfx/telemetry_protocol.h"

using namespace utrom_telemetry;

// Capacity for 10,000 frames (10 seconds of 1kHz data)
// SPSC = Single Producer Single Consumer
boost::lockfree::spsc_queue<TelemetryFrame, boost::lockfree::capacity<10000>> ring_buffer;

std::atomic<bool> keep_running(true);
std::atomic<int> frames_received(0);

// --- THE CONSUMER (Logger Thread) ---
// This thread simulates writing to a disk or sending over a network.
void logger_thread() {
    TelemetryFrame frame;
    while (keep_running || !ring_buffer.empty()) {
        if (ring_buffer.pop(frame)) {
            frames_received++;
        } else {
            // If the queue is empty, give the CPU a tiny break
            std::this_thread::yield();
        }
    }
}

int main() {
    std::cout << "--- [UtromPulse v1.0] SPSC Bridge Verification ---" << std::endl;

    // 1. Start the logger thread
    std::thread consumer(logger_thread);

    // 2. THE PRODUCER (G1 Control Loop Simulation)
    std::cout << "Simulating 1 second of 1kHz robot telemetry..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; ++i) {
        TelemetryFrame frame;
        frame.frame_index = i;
        frame.timestamp_ns = i * 1000000; // Simulated 1ms intervals
        
        // Attempt to push to the queue. 
        // .push() is non-blocking and lock-free.
        if (!ring_buffer.push(frame)) {
            std::cerr << "Buffer Full at index " << i << std::endl;
        }

        // Simulate 1ms control loop frequency
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 3. Cleanup
    keep_running = false;
    if (consumer.joinable()) {
        consumer.join();
    }

    // 4. Verification
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total Frames Sent:     1000" << std::endl;
    std::cout << "Total Frames Received: " << frames_received << std::endl;

    if (frames_received == 1000) {
        std::cout << "SUCCESS: Zero-drop lock-free bridge verified." << std::endl;
        return 0;
    } else {
        std::cerr << "FAILURE: Data loss detected!" << std::endl;
        return 1;
    }
}