import torch
import numpy as np
import subprocess
import os
import sys
import importlib.util
import tempfile
import pybind11
import sysconfig
from pathlib import Path

class AxonValidator:
    def __init__(self, epsilon: float = 1e-7):
        self.epsilon = epsilon

    def verify(self, model: torch.nn.Module, header_code: str, input_dim: int):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            test_input = torch.randn(1, input_dim)
            with torch.no_grad():
                py_output = model(test_input).numpy().flatten()

            header_file, glue_file = tmp_path / "shadow_header.h", tmp_path / "glue.cpp"
            lib_path = tmp_path / f"utromfx_shadow.so"
            
            with open(header_file, "w") as f: f.write(header_code)
            with open(glue_file, "w") as f: f.write(self._generate_glue_code())

            self._compile(glue_file, lib_path, tmp_path)
            cpp_output = self._run_cpp_inference(lib_path, test_input.numpy().flatten(), py_output.size)

            max_diff = np.abs(py_output - cpp_output).max()
            if max_diff < self.epsilon:
                print(f"PARITY PASSED: Max Delta = {max_diff:.2e}")
                return True
            raise ValueError(f"PARITY FAILED: Delta {max_diff:.2e} > {self.epsilon}!")

    def _generate_glue_code(self) -> str:
        return """
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include "shadow_header.h"
namespace py = pybind11;
utromfx_gen::G1Controller controller;
void forward(py::array_t<float> input, py::array_t<float> output) {
    auto r_in = input.unchecked<1>();
    auto r_out = output.mutable_unchecked<1>();
    controller.forward_raw(r_in.data(0), r_out.mutable_data(0));
}
PYBIND11_MODULE(utromfx_shadow, m) { m.def("forward", &forward); }
"""

    def _compile(self, src: Path, out: Path, include_dir: Path):
        core_include = Path(__file__).parent.parent.parent / "src/utromfx_core/include"
        py_inc = sysconfig.get_path('include')
        cmd = [
            "c++", "-O3", "-shared", "-std=c++17", "-fPIC", f"-I{core_include}", f"-I{include_dir}",
            f"-I{pybind11.get_include()}", f"-I{pybind11.get_include(user=True)}", f"-I{py_inc}",
            str(src), "-o", str(out)
        ]
        if sys.platform == "darwin": cmd += ["-undefined", "dynamic_lookup"]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(res.stderr)
            raise RuntimeError("Compilation failed.")

    def _run_cpp_inference(self, lib_path: Path, input_data: np.ndarray, output_size: int) -> np.ndarray:
        spec = importlib.util.spec_from_file_location("utromfx_shadow", str(lib_path))
        module = importlib.util.module_from_spec(spec)
        sys.modules["utromfx_shadow"] = module
        spec.loader.exec_module(module)
        cpp_out = np.zeros(output_size, dtype=np.float32)
        module.forward(input_data.astype(np.float32), cpp_out)
        return cpp_out