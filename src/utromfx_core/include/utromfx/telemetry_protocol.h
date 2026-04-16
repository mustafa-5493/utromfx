#ifndef UTROM_TELEMETRY_PROTOCOL_H
#define UTROM_TELEMETRY_PROTOCOL_H

#include <cstdint>

namespace utrom_telemetry {

#pragma pack(push, 1)
struct TelemetryFrame {
    // --- Header (12 Bytes) ---
    uint64_t timestamp_ns;    
    uint32_t frame_index;     

    // --- Joint State (96 Bytes) ---
    float q[12];              
    float dq[12];             

    // --- Commands (48 Bytes) ---
    float tau_cmd[12];        

    // --- Inertial Data (36 Bytes) ---
    float imu_gyro[3];        
    float imu_accel[3];       
    float gravity_proj[3];    

    // --- Diagnostics (5 Bytes) ---
    uint32_t loop_latency_us; 
    uint8_t  safety_status;   
};
#pragma pack(pop)

} // namespace utrom_telemetry

#endif