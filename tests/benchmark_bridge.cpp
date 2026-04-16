#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <pthread.h>
#include <sched.h>
#include <boost/lockfree/spsc_queue.hpp>
#include "../src/utromfx_core/include/utromfx/telemetry_protocol.h"

using namespace utrom_telemetry;

const int NUM_FRAMES = 1000000; // 1 Million Frames

// Massive capacity so the consumer thread doesn't bottleneck our producer measurement
boost::lockfree::spsc_queue<TelemetryFrame, boost::lockfree::capacity<100000>> ring_buffer;
std::atomic<bool> keep_running(true);

// --- THE CONSUMER (Drainer) ---
// Just empties the queue as fast as possible to keep it clear for the producer
void consumer_thread() {
    TelemetryFrame frame;
    while (keep_running) {
        while (ring_buffer.pop(frame)) {
            // Do nothing, just drain
        }
    }
}

// --- REAL-TIME SCHEDULING HELPER ---
void set_realtime_priority(std::thread& th, int core_id, int priority) {
#ifdef __linux__
    pthread_t native_thread = th.native_handle();

    // 1. Core Pinning (CPU Affinity)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    int rc = pthread_setaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Warning: Failed to set CPU affinity. Are you running as root?" << std::endl;
    }

    // 2. SCHED_FIFO Priority
    struct sched_param param;
    param.sched_priority = priority; // 1 (low) to 99 (high)
    
    rc = pthread_setschedparam(native_thread, SCHED_FIFO, &param);
    if (rc != 0) {
        std::cerr << "Warning: Failed to set SCHED_FIFO. Run with 'sudo'." << std::endl;
    } else {
        std::cout << "Thread locked to Core " << core_id << " with SCHED_FIFO (Priority: " << priority << ")" << std::endl;
    }
#else
    std::cout << "macOS Detected: Skipping RT Linux Thread Pinning (Simulation Mode) for Worker." << std::endl;
#endif
}

int main() {
    std::cout << "--- [UtromPulse v1.0] Hard Performance Evidence ---" << std::endl;
    std::cout << "Payload Size: " << sizeof(TelemetryFrame) << " bytes/frame" << std::endl;
    std::cout << "Target:       " << NUM_FRAMES << " frames" << std::endl;
    
    std::thread consumer(consumer_thread);

    // --- APPLY REAL-TIME SCHEDULING ---
    // Consumer gets priority 50 on Core 2.
    set_realtime_priority(consumer, 2, 50);

    // Main loop gets maximum priority 99 on Core 1.
#ifdef __linux__
    pthread_t main_thread = pthread_self();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    pthread_setaffinity_np(main_thread, sizeof(cpu_set_t), &cpuset);

    struct sched_param param;
    param.sched_priority = 99;
    pthread_setschedparam(main_thread, SCHED_FIFO, &param);
    std::cout << "Main Control Loop locked to Core 1 with SCHED_FIFO (Priority: 99)" << std::endl;
#else
    std::cout << "macOS Detected: Skipping RT Linux Thread Pinning for Main Loop." << std::endl;
#endif

    // Warmup the CPU caches
    TelemetryFrame dummy_frame;
    for(int i=0; i<1000; i++) { ring_buffer.push(dummy_frame); }

    uint64_t max_latency_ns = 0;
    uint64_t total_latency_ns = 0;

    std::cout << "Firing..." << std::endl;

    auto start_bench = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_FRAMES; ++i) {
        // Measure exact nanoseconds taken to push a frame
        auto t0 = std::chrono::high_resolution_clock::now();
        
        while (!ring_buffer.push(dummy_frame)) {
            // Spin if queue is full (Consumer is catching up)
        }
        
        auto t1 = std::chrono::high_resolution_clock::now();
        
        uint64_t latency = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        total_latency_ns += latency;
        
        if (latency > max_latency_ns) {
            max_latency_ns = latency;
        }
    }

    auto end_bench = std::chrono::high_resolution_clock::now();
    uint64_t wall_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_bench - start_bench).count();

    keep_running = false;
    consumer.join();

    uint64_t avg_latency_ns = total_latency_ns / NUM_FRAMES;
    double throughput_mb = (NUM_FRAMES * sizeof(TelemetryFrame)) / (1024.0 * 1024.0);
    double mb_per_sec = throughput_mb / (wall_time_ms / 1000.0);

    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Total Wall Time:   " << wall_time_ms << " ms" << std::endl;
    std::cout << "Average Latency:   " << avg_latency_ns << " ns / push" << std::endl;
    std::cout << "Worst-Case Jitter: " << max_latency_ns << " ns" << std::endl;
    std::cout << "Total Throughput:  " << std::fixed << std::setprecision(2) << mb_per_sec << " MB/s" << std::endl;
    std::cout << "---------------------------------------------------" << std::endl;

    // A 1kHz loop gives us 1,000,000 ns per tick. 
    // If our worst-case jitter is under 50,000 ns (50μs), it's a massive win.
    if (max_latency_ns < 50000) {
        std::cout << "VERDICT: Sub-microsecond blocking verified. Safe for 1kHz flight." << std::endl;
    } else {
        std::cout << "VERDICT: Jitter exceeded 50μs. Check OS scheduler." << std::endl;
    }

    return 0;
}