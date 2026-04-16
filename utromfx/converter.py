import struct
import json
import time
import os
from mcap.writer import Writer

class UtromToMcapConverter:
    #  C++ struct layout: uint64, uint32, 12f(q), 12f(dq), 12f(tau), 3f(gyro), 3f(accel), 3f(grav), uint32(lat), uint8(safe)
    FORMAT = "=QI12f12f12f3f3f3fIB"
    FRAME_SIZE = 197
    
    # Unitree G1 Joint Names (in order of the C++ array)
    JOINT_NAMES = [
        "left_hip_pitch", "left_hip_roll", "left_hip_yaw", "left_knee", "left_ankle_pitch", "left_ankle_roll",
        "right_hip_pitch", "right_hip_roll", "right_hip_yaw", "right_knee", "right_ankle_pitch", "right_ankle_roll"
    ]

    @classmethod
    def convert(cls, input_bin, output_mcap):
        if not os.path.exists(input_bin):
            print(f"Input file not found: {input_bin}")
            return

        with open(output_mcap, "wb") as f_out:
            writer = Writer(f_out)
            writer.start()

            # Register Foxglove Schemas
            joint_schema_id = writer.register_schema(
                name="foxglove.JointState",
                encoding="jsonschema",
                data=b'{"type": "object"}' # Simplified, Foxglove Studio auto-infers from the name
            )
            imu_schema_id = writer.register_schema(
                name="foxglove.Imu",
                encoding="jsonschema",
                data=b'{"type": "object"}'
            )

            # Register Channels
            joint_channel_id = writer.register_channel(
                schema_id=joint_schema_id, topic="/g1/joint_states", message_encoding="json"
            )
            imu_channel_id = writer.register_channel(
                schema_id=imu_schema_id, topic="/g1/imu", message_encoding="json"
            )

            print(f"Converting {input_bin} to {output_mcap}...")
            
            frame_count = 0
            with open(input_bin, "rb") as f_in:
                while True:
                    data = f_in.read(cls.FRAME_SIZE)
                    if len(data) < cls.FRAME_SIZE:
                        break
                    
                    # Unpack the binary
                    unpacked = struct.unpack(cls.FORMAT, data)
                    
                    timestamp_ns = unpacked[0]
                    # Map slices from the unpacked tuple
                    q = unpacked[2:14]
                    dq = unpacked[14:26]
                    tau = unpacked[26:38]
                    gyro = unpacked[38:41]
                    accel = unpacked[41:44]
                    
                    # 1. Create Foxglove JointState JSON
                    joint_msg = {
                        "name": cls.JOINT_NAMES,
                        "position": list(q),
                        "velocity": list(dq),
                        "effort": list(tau)
                    }
                    
                    # 2. Create Foxglove IMU JSON
                    imu_msg = {
                        "angular_velocity": {"x": gyro[0], "y": gyro[1], "z": gyro[2]},
                        "linear_acceleration": {"x": accel[0], "y": accel[1], "z": accel[2]}
                    }

                    # Write to MCAP (Timestamp must be in nanoseconds)
                    writer.add_message(
                        channel_id=joint_channel_id,
                        log_time=timestamp_ns,
                        data=json.dumps(joint_msg).encode("utf-8"),
                        publish_time=timestamp_ns
                    )
                    
                    writer.add_message(
                        channel_id=imu_channel_id,
                        log_time=timestamp_ns,
                        data=json.dumps(imu_msg).encode("utf-8"),
                        publish_time=timestamp_ns
                    )
                    
                    frame_count += 1

            writer.finish()
            print(f"Success! Packed {frame_count} frames into {output_mcap}")

if __name__ == "__main__":
    
    UtromToMcapConverter.convert("crash_record.utrom", "flight_log.mcap")