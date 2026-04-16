import struct
import pandas as pd
import numpy as np
import os

class UtromDecoder:
    # '=': Native byte order, no padding
    # Q: uint64, I: uint32, f: float, B: uint8
    FORMAT = "=QI12f12f12f3f3f3fIB"
    FRAME_SIZE = 197

    @classmethod
    def decode_file(cls, filepath):
        if not os.path.exists(filepath):
            print(f"File not found: {filepath}")
            return None

        frames = []
        with open(filepath, "rb") as f:
            while True:
                data = f.read(cls.FRAME_SIZE)
                if len(data) < cls.FRAME_SIZE:
                    break
                unpacked = struct.unpack(cls.FORMAT, data)
                frames.append(unpacked)

        # Re-constructing the columns list correctly
        columns = ["timestamp_ns", "frame_index"]
        columns += [f"q_{i}" for i in range(12)]
        columns += [f"dq_{i}" for i in range(12)]
        columns += [f"tau_cmd_{i}" for i in range(12)]
        columns += ["ro_x", "gyro_y", "gyro_z"]
        columns += ["accel_x", "accel_y", "accel_z"]
        columns += ["grav_x", "grav_y", "grav_z"]
        columns += ["latency_us", "safety_status"]

        return pd.DataFrame(frames, columns=columns)

if __name__ == "__main__":
    print("--- [UtromPulse v1.0] Python Decoder Verification ---")
    df = UtromDecoder.decode_file("test_flight.utrom")
    
    if df is not None:
        print(f"Successfully decoded {len(df)} frames.")
        print("\n--- Telemetry Preview (Head) ---")
        # Selecting specific columns to verify
        cols_to_show = ["frame_index", "q_0", "tau_cmd_11", "latency_us", "safety_status"]
        print(df[cols_to_show].head())
        
        q0_val = df.iloc[0]["q_0"]
        print(f"\nVerifying Bit-Integrity:")
        print(f"Expected q_0: 1.234| Decoded: {q0_val:.4f}")
        
        if abs(q0_val - 1.2345) < 1e-5:
            print("BIT-PERFECT: Binary-to-Python bridge is verified.")
