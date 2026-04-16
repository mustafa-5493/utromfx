# utromfx

**High-Performance Sim2Real Neural Deployment for Humanoid Locomotion**

[![Python 3.14+](https://img.shields.io/badge/python-3.14+-blue.svg)](https://www.python.org/downloads/release/python-3140a1/)
[![C++ 17](https://img.shields.io/badge/std-c%2B%2B17-orange.svg)](#)
[![Unitree G1](https://img.shields.io/badge/Hardware-Unitree%20G1-green.svg)](#)

`utromfx` is a specialized deployment infrastructure designed to bridge the gap between high-level Reinforcement Learning (RL) training and low-level hardware execution. It provides a standalone compiler and a real-time reflex wrapper to deploy neural policies onto the Unitree G1 humanoid at a deterministic 1kHz control frequency.

---

## Key Features

- **`utx` Compiler:** A zero-dependency neural-to-C++ tracer that bakes PyTorch models directly into header-only C++ classes.
- **Bit-Exact Parity Engine:** Automated JIT-validation suite ensuring numerical consistency between Python (PyTorch) and C++ (SIMD) with a precision threshold of $\epsilon = 10^{-6}$.
- **NEON-Optimized Kernels:** Math kernels hand-crafted for the ARM Cortex-A78AE (NVIDIA Orin NX), utilizing 128-bit SIMD intrinsics for sub-20μs inference.
- **G1 Reflex Wrapper:** A production-ready hardware bridge featuring:
  - **45-Dim Observation Vector:** Integrated support for joint kinematics, IMU projected gravity, temporal history, and joystick commands.
  - **Real-time Logger:** RAM-preallocated telemetry system for high-frequency "Black Box" data collection without I/O jitter.
  - **Safety Watchdog:** Sub-millisecond IMU-based tilt-protection and joint velocity damping.

---

## Project Structure

```text
utromfx/
├── utromfx/                # Python Compiler & Validation Logic
│   ├── compiler/           # Core Codegen and Parity engines
│   └── main.py             # CLI Entry point (utx)
├── src/                    # C++ Hardware Source
│   ├── utromfx_core/       # Architecture-agnostic SIMD math kernels
│   └── reflex_g1/          # Unitree G1 Hardware Bridge & 1kHz Loop
├── CMakeLists.txt          # Orin NX-optimized Build System
├── pyproject.toml          # Python Environment (uv)
└── G1Controller.h          # Generated "Baked" Brain
```

---

## Installation

### Prerequisites

- Python 3.14+
- NVIDIA Jetson Orin NX (or compatible ARMv8.2-A environment)
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

The `utx` tool converts a saved PyTorch model (`.pt`) into a high-performance C++ header while verifying bit-exact parity.

```bash
utx compile policy.pt --history 5 --output G1Controller.h
```

### 2. Deploy to G1 Hardware

The C++ bridge utilizes `SCHED_FIFO` real-time priority to ensure deterministic execution on the robot's Orin NX.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo ./utrom_g1_node
```

---

## Technical Specifications

| Metric                 | Specification                                  |
|----------------------|-----------------------------------------------|
| Control Loop         | 1000 Hz (Deterministic)                        |
| Inference Latency    | ~12–18 μs (on Orin NX)                         |
| Observation Space    | 45-Dimensions (7-frame temporal stack)         |
| Safety Protocol      | IMU-based Torque Cut-off + Velocity Damping    |
| SIMD Instruction Set | NEON (Vectorized FP32)                         |

---

## Safety Disclaimer

This software is designed for high-torque humanoid hardware. Always test policies on a gantry or hoist before attempting untethered locomotion. The authors are not responsible for hardware damage resulting from non-validated neural policies.

---

## License

This project is licensed under the MIT License — see the `LICENSE` file for details.