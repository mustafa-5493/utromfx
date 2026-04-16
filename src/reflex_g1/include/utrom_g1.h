#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include "G1Controller.h"

namespace utrom_g1 {

/**
 * @brief Verified Unitree G1 Motor Layout
 * Following the standard SDK2 order: Yaw -> Roll -> Pitch -> Knee -> Ankle P -> Ankle R
 */
enum JointIndex {
    L_HIP_YAW = 0, L_HIP_ROLL = 1, L_HIP_PITCH = 2, L_KNEE = 3, L_ANKLE_PITCH = 4, L_ANKLE_ROLL = 5,
    R_HIP_YAW = 6, R_HIP_ROLL = 7, R_HIP_PITCH = 8, R_KNEE = 9, R_ANKLE_PITCH = 10, R_ANKLE_ROLL = 11,
    JOINT_COUNT = 12
};

struct ReflexConfig {
    float pos_scale = 1.0f;
    float vel_scale = 0.05f;
    float imu_gyro_scale = 0.1f;
    float action_scale = 15.0f;     // Normalized (-1..1) to Torque (Nm)
    float torque_limit = 25.0f;    // Hard cap for motor safety
    float tilt_limit = 0.7f;       // Radians (~40 deg) before emergency cut
};

class ReflexWrapper {
private:
    utromfx_gen::G1Controller brain;
    ReflexConfig config;
    
    // Internal Buffers
    std::array<float, 45> obs_frame;  // The 45-dim input for the "Alien" Brain
    std::array<float, 12> last_tau;   // Memory of the previous motor commands

    // Neutral Standing Pose (Radians)
    // IMPORTANT: Verify these offsets on your gantry before full-power tests.
    const float stand_pose[12] = {
        0.0f, 0.0f, -0.2f, 0.5f, -0.3f, 0.0f, // Left Leg
        0.0f, 0.0f, -0.2f, 0.5f, -0.3f, 0.0f  // Right Leg
    };

public:
    ReflexWrapper() {
        obs_frame.fill(0.0f);
        last_tau.fill(0.0f);
    }

    /**
     * @brief High-speed control tick.
     * @return true if successful, false if emergency stop triggered.
     */
    bool step(const float* q, const float* dq, 
              const float* gyro, const float* gravity,
              float vx, float vy, float vyaw,
              float* tau_out) {

        // 1. WATCHDOG: Check for catastrophic tilt (IMU Z-axis check)
        if (std::abs(gravity[0]) > config.tilt_limit || std::abs(gravity[1]) > config.tilt_limit) {
            std::cerr << "⚠️ EMERGENCY: Tilt limit exceeded! Cutting power." << std::endl;
            std::fill(tau_out, tau_out + 12, 0.0f);
            return false;
        }

        // 2. PRE-PROCESS: Pack the 45-dimension vector
        // [0-11] Joint Positions (Delta from stand)
        for (int i = 0; i < 12; ++i) obs_frame[i] = (q[i] - stand_pose[i]) * config.pos_scale;
        
        // [12-23] Joint Velocities
        for (int i = 0; i < 12; ++i) obs_frame[12 + i] = dq[i] * config.vel_scale;
        
        // [24-26] Project Gravity Vector
        obs_frame[24] = gravity[0]; 
        obs_frame[25] = gravity[1]; 
        obs_frame[26] = gravity[2];
        
        // [27-29] Gyroscope
        obs_frame[27] = gyro[0] * config.imu_gyro_scale;
        obs_frame[28] = gyro[1] * config.imu_gyro_scale;
        obs_frame[29] = gyro[2] * config.imu_gyro_scale;
        
        // [30-41] Previous Actions (Memory/Smoothing)
        for (int i = 0; i < 12; ++i) obs_frame[30 + i] = last_tau[i];
        
        // [42-44] Joystick Commands
        obs_frame[42] = vx;   // Forward velocity
        obs_frame[43] = vy;   // Lateral velocity
        obs_frame[44] = vyaw; // Turning rate

        // 3. INFERENCE: The "Alien Tech" 12us Loop
        float raw_actions[12];
        brain.update(obs_frame.data(), raw_actions);

        // 4. POST-PROCESS: Torque Mapping & Safety Clipping
        for (int i = 0; i < 12; ++i) {
            float target_tau = raw_actions[i] * config.action_scale;
            
            // Hard torque clamp
            tau_out[i] = std::clamp(target_tau, -config.torque_limit, config.torque_limit);
            
            // Save for the next frame's observation
            last_tau[i] = raw_actions[i];
        }

        return true;
    }
};

} // namespace utrom_g1