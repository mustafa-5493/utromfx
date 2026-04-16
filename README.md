# utromfx

**High-Performance Sim2Real Neural Deployment for Humanoid Locomotion**

utromfx is a specialized deployment infrastructure designed to bridge the gap between high-level Reinforcement Learning (RL) training and low-level hardware execution. It provides a standalone compiler and a real-time reflex wrapper to deploy neural policies onto the Unitree G1 humanoid at a deterministic 1kHz control frequency.

---

## Key Features

* **utx Compiler:** A zero-dependency neural-to-C++ tracer that bakes PyTorch models directly into header-only C++ classes.
* **Bit-Exact Parity Engine:** Automated JIT-validation suite ensuring numerical consistency between Python (PyTorch) and C++ (SIMD) with a precision threshold of epsilon = 1e-6.
* **NEON-Optimized Kernels:** Math kernels hand-crafted for the ARM Cortex-A78AE (NVIDIA Orin NX), utilizing 128-bit SIMD intrinsics for sub-20 microsecond inference.

* **Industrial Telemetry Bridge (v1.0):** A zero-blocking observability stack designed for production robotics:
    * **Lock-Free SPSC Bridge:** Uses boost::lockfree to move data from the control loop to the disk writer with 53ns average latency and zero mutex contention.
    * **Real-Time Threading:** Automated SCHED_FIFO thread pinning on Orin NX to eliminate OS jitter.
    * **Crash-Proof Logging:** Integrated SIGINT/SIGTERM handlers ensure 100% data recovery of the 197-byte packed binary frames during emergency shutdowns.
    * **Foxglove Studio Integration:** Native Python converter translates raw flight binaries into industry-standard MCAP files for 3D playback.

* **Safety Watchdog:** Sub-millisecond IMU-based tilt-protection and joint velocity damping.

---

## Project Structure

```text
utromfx/
├── utromfx/                # Python Core
│   ├── compiler/           # Codegen and Parity engines
│   ├── decoder.py          # Binary-to-Pandas Telemetry Decoder
│   └── converter.py        # MCAP Converter for Foxglove Studio
├── src/                    # C++ Hardware Source
│   ├── utromfx_core/       
│   │   └── include/        # Telemetry Protocol & Math Kernels
│   └── reflex_g1/          # Unitree G1 Bridge & 1kHz Loop
├── tests/                  # Hardware-in-the-loop (HIL) Verifications
│   ├── benchmark_bridge.cpp     # Sub-microsecond latency stress test
│   └── test_secure_shutdown.cpp # SIGINT disk flush verification
├── CMakeLists.txt          # Orin NX-optimized Build System
└── pyproject.toml          # Python Environment (uv)
```

---

## Installation

### Prerequisites

- Python 3.14+
- NVIDIA Jetson Orin NX (or compatible ARMv8.2-A environment)
- Boost C++ Libraries (e.g., `libboost-all-dev`)
- Unitree SDK2

### Setup

```bash
# Clone the repository
git clone https://github.com/mustafa-5493/utromfx.git
cd utromfx

# Install dependencies using uv
uv sync
```

---

## Usage

### 1. Compile and Validate

The utx tool converts a saved PyTorch model into a high-performance C++ header.

```bash
utx compile policy.pt --history 5 --output G1Controller.h
```

---

### 2. Deploy to G1 Hardware

The C++ bridge utilizes SCHED_FIFO real-time priority to ensure deterministic execution.

```bash
mkdir build && cd build
cmake .. && make -j$(nproc)
sudo ./utrom_g1_node
```

---

### 3. Decode & Visualize Telemetry

Post-flight, convert the raw crash-proof binary data into an MCAP file for 3D analysis in Foxglove Studio, or decode it for Pandas data science:

```bash
# Generate Foxglove MCAP log
uv run python utromfx/converter.py

# Decode directly to Pandas DataFrame
uv run python utromfx/decoder.py
```

---

## Technical Specifications

| Metric              | Specification                          |
|-------------------|----------------------------------------|
| Control Loop       | 1000 Hz (Deterministic)               |
| Inference Latency  | ~12–18 µs (on Orin NX)                |
| Telemetry Format   | Packed Binary (197 Bytes/Frame)       |
| Data Bridge        | Lock-Free SPSC (53ns avg latency)     |
| Safety Protocol    | IMU-based Torque Cut-off + Velocity Damping |
| SIMD Instruction Set | NEON (Vectorized FP32)             |

---

## Safety Disclaimer

This software is designed for high-torque humanoid hardware. Always test policies on a gantry or hoist before attempting untethered locomotion. The authors are not responsible for hardware damage resulting from non-validated neural policies.

---

## License

This project is licensed under the MIT License — see the LICENSE file for details.
