#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <boost/lockfree/spsc_queue.hpp>
#include "../src/utromfx_core/include/utromfx/telemetry_protocol.h"

using namespace utrom_telemetry;

boost::lockfree::spsc_queue<TelemetryFrame, boost::lockfree::capacity<100000>> ring_buffer;
std::atomic<bool> keep_running(true);
std::atomic<uint32_t> frames_generated(0);
std::atomic<uint32_t> frames_written(0);

// --- SIGNAL HANDLER ---
// This intercepts Ctrl+C (SIGINT) or a generic kill command (SIGTERM)
void handle_signal(int signal) {
    if (keep_running) {
        std::cout << "\n[Emergency Override] Signal " << signal << " received." << std::endl;
        std::cout << "Initiating graceful shutdown... flushing memory to disk." << std::endl;
        keep_running = false; // This tells the loops to stop accepting new data
    }
}

// --- THE CONSUMER (Disk Writer) ---
void disk_writer_thread() {
    std::ofstream out_file("crash_record.utrom", std::ios::binary);
    TelemetryFrame frame;
    
    // Notice the condition: keep running OR queue is not empty.
    // This guarantees we drain the queue even after keep_running is false.
    while (keep_running || !ring_buffer.empty()) {
        if (ring_buffer.pop(frame)) {
            out_file.write(reinterpret_cast<const char*>(&frame), sizeof(TelemetryFrame));
            frames_written++;
        } else {
            std::this_thread::yield();
        }
    }
    
    out_file.flush();
    out_file.close();
    std::cout << "Disk Writer safely unmounted." << std::endl;
}

int main() {
    std::cout << "--- [UtromPulse v1.0] Secure Shutdown Verification ---" << std::endl;
    
    // 1. Register the Signal Handlers
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    // 2. Start the Disk Writer
    std::thread writer(disk_writer_thread);

    std::cout << "Simulating infinite 1kHz flight..." << std::endl;
    std::cout << "Press [Ctrl+C] at any time to simulate a crash or manual abort." << std::endl;

    // 3. The 1kHz Loop (Infinite until Ctrl+C)
    while (keep_running) {
        TelemetryFrame frame;
        frame.frame_index = frames_generated;
        frame.timestamp_ns = frames_generated * 1000000;
        
        // Add a recognizable marker to prove data wasn't corrupted
        frame.q[0] = 99.99f; 

        if (ring_buffer.push(frame)) {
            frames_generated++;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 4. Wait for the writer to finish flushing the queue
    writer.join();

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Total Frames Generated: " << frames_generated << std::endl;
    std::cout << "Total Frames Written:   " << frames_written << std::endl;

    if (frames_generated == frames_written) {
        std::cout << "VERDICT: Perfect shutdown. 100% of flight data recovered." << std::endl;
        return 0;
    } else {
        std::cout << "VERDICT: Data loss! " << (frames_generated - frames_written) << " frames abandoned." << std::endl;
        return 1;
    }
}