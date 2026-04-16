#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <iomanip>
#include <signal.h>
#include <sched.h>

// Unitree SDK Headers
#include "unitree/robot/channel/channel_publisher.hpp"
#include "unitree/robot/channel/channel_subscriber.hpp"
#include "unitree/idl/go2/LowState_.hpp"
#include "unitree/idl/go2/LowCmd_.hpp"

#include "utrom_g1.h"

// --- LOGGING STRUCTURE ---
struct TelemetryEntry {
    double time_ms;
    float q[12];
    float dq[12];
    float tau[12];
    float gravity[3];
};

// Global for signal handling
bool running = true;
void SigHandler(int sig) { running = false; }

int main(int argc, char** argv) {
    std::cout << "--- [UtromPulse] Initializing G1 Hardware Bridge with Telemetry ---" << std::endl;
    signal(SIGINT, SigHandler);

    // 1. PRE-ALLOCATE TELEMETRY RAM
    // Reserve 60 seconds of 1kHz data (60,000 entries) to avoid mid-loop allocations
    std::vector<TelemetryEntry> black_box;
    black_box.reserve(60000); 

    utrom_g1::ReflexWrapper reflex;

    // 2. SETUP SDK CHANNELS
    unitree::robot::ChannelSubscriber<unitree_go2::msg::dds_::LowState_> state_sub("rt/lowstate");
    unitree::robot::ChannelPublisher<unitree_go2::msg::dds_::LowCmd_> cmd_pub("rt/lowcmd");
    state_sub.Init();
    cmd_pub.Init();

    // 3. REAL-TIME PRIORITY
    struct sched_param param;
    param.sched_priority = 99;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    auto start_session = std::chrono::steady_clock::now();
    auto interval = std::chrono::microseconds(1000); // 1ms
    
    unitree_go2::msg::dds_::LowState_ state;
    unitree_go2::msg::dds_::LowCmd_ cmd;

    std::cout << "UTROM_G1: Loop started. Logging to RAM..." << std::endl;

    while (running) {
        auto loop_start = std::chrono::steady_clock::now();

        if (state_sub.Read(state, 1000)) {
            float q[12], dq[12], gyro[3], gravity[3], tau_out[12];

            // Extract Data
            for (int i = 0; i < 12; ++i) {
                q[i] = state.motor_state()[i].q();
                dq[i] = state.motor_state()[i].dq();
            }
            for (int i = 0; i < 3; ++i) {
                gyro[i] = state.imu_state().gyroscope()[i];
                gravity[i] = state.imu_state().accelerometer()[i];
            }

            // --- EXECUTE BRAIN ---
            // Using dummy joystick commands (0,0,0) for the initial stand test
            bool safe = reflex.step(q, dq, gyro, gravity, 0.0f, 0.0f, 0.0f, tau_out);

            // --- RECORD TELEMETRY (RAM Only - O(1) Speed) ---
            if (black_box.size() < black_box.capacity()) {
                TelemetryEntry entry;
                entry.time_ms = std::chrono::duration<double, std::milli>(loop_start - start_session).count();
                std::copy(q, q + 12, entry.q);
                std::copy(dq, dq + 12, entry.dq);
                std::copy(tau_out, tau_out + 12, entry.tau);
                std::copy(gravity, gravity + 3, entry.gravity);
                black_box.push_back(entry);
            }

            // Pack and Publish
            if (safe) {
                for (int i = 0; i < 12; ++i) {
                    cmd.motor_cmd()[i].q(0);
                    cmd.motor_cmd()[i].dq(0);
                    cmd.motor_cmd()[i].kp(0);
                    cmd.motor_cmd()[i].kd(0);
                    cmd.motor_cmd()[i].tau(tau_out[i]);
                }
                cmd_pub.Write(cmd);
            }
        }

        // Timing Guard
        auto loop_end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(loop_end - loop_start);
        if (elapsed < interval) {
            std::this_thread::sleep_for(interval - elapsed);
        }
    }

    // --- POST-RUN: DUMP RAM TO DISK ---
    std::cout << "\n Loop stopped. Writing " << black_box.size() << " entries to g1_telemetry.csv..." << std::endl;
    std::ofstream file("g1_telemetry.csv");
    file << "ms,q0,q1,q2,q3,q4,q5,q6,q7,q8,q9,q10,q11,tau0,tau1,tau2,tau3,tau4,tau5,tau6,tau7,tau8,tau9,tau10,tau11,gx,gy,gz\n";
    
    for (const auto& log : black_box) {
        file << log.time_ms << ",";
        for (int i = 0; i < 12; ++i) file << log.q[i] << ",";
        for (int i = 0; i < 12; ++i) file << log.tau[i] << ",";
        file << log.gravity[0] << "," << log.gravity[1] << "," << log.gravity[2] << "\n";
    }

    std::cout << "Done. Telemetry saved." << std::endl;
    return 0;
}