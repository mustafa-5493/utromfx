import torch
import torch.fx
import numpy as np
from typing import List, Dict, Any

class WeightExporter:
    @staticmethod
    def to_cpp_array(arr: np.ndarray) -> str:
        # Using :.15e ensures we capture the full precision of a 32-bit float
        if arr.ndim == 1:
            content = ", ".join([f"{x:.15e}f" for x in arr])
            return f"{{ {content} }}"
        elif arr.ndim == 2:
            rows = [f"      {{ {', '.join([f'{x:.15e}f' for x in row])} }}" for row in arr]
            return "{\n" + ",\n".join(rows) + "\n    }"
        raise ValueError("Unsupported tensor dimensions.")

class AxonCodegen:
    def __init__(self, model: torch.nn.Module, history_len: int = 1):
        self.model = model
        self.history_len = history_len
        self.layers: List[Dict[str, Any]] = []
        self._trace_model()

    def _trace_model(self):
        self.model.eval()
        # Use named_children to ensure we get the execution order of nn.Sequential
        for name, module in self.model.named_children():
            if isinstance(module, torch.nn.Linear):
                self.layers.append({
                    'type': 'linear', 'in_features': module.in_features,
                    'out_features': module.out_features,
                    'weights': module.weight.data.cpu().numpy(),
                    'bias': module.bias.data.cpu().numpy()
                })
            elif isinstance(module, torch.nn.ReLU): self.layers.append({'type': 'relu'})
            elif isinstance(module, torch.nn.Tanh): self.layers.append({'type': 'tanh'})

    def generate_header(self, class_name: str = "G1Controller") -> str:
        input_dim = self.layers[0]['in_features']
        output_dim = self.layers[-1]['out_features']
        max_hidden = max([l.get('out_features', 0) for l in self.layers])
        feature_dim = input_dim // self.history_len

        code = [
            '#pragma once', '#include "utromfx/kernels.h"', '#include "utromfx/state_buffer.h"',
            '#include <algorithm>', '', 'namespace utromfx_gen {', '', f'class {class_name} {{', 'private:',
            f'    utromfx::ReflexBuffer<{feature_dim}, {self.history_len}> history;', ''
        ]

        for i, l in enumerate(self.layers):
            if l['type'] == 'linear':
                code.append(f'    const utromfx::LinearLayer<{l["in_features"]}, {l["out_features"]}> layer_{i} = {{')
                code.append(f'        {WeightExporter.to_cpp_array(l["weights"])},')
                code.append(f'        {WeightExporter.to_cpp_array(l["bias"])}')
                code.append(f'    }};')

        code.append('\npublic:')
        code.append('    void update(const float* raw_obs, float* action_out) {')
        code.append(f'        float processed_obs[{input_dim}];')
        code.append('        history.push(raw_obs);')
        code.append('        history.flatten(processed_obs);')
        code.append(f'        float buf_a[{max_hidden}], buf_b[{max_hidden}];')
        code.append('        _execute_graph(processed_obs, action_out, buf_a, buf_b);')
        code.append('    }\n')

        code.append('    void forward_raw(const float* input, float* output) {')
        code.append(f'        float buf_a[{max_hidden}], buf_b[{max_hidden}];')
        code.append('        _execute_graph(input, output, buf_a, buf_b);')
        code.append('    }\n')

        code.append('private:')
        code.append('    void _execute_graph(const float* input, float* output, float* buf_a, float* buf_b) {')
        current_in = "input"
        for i, l in enumerate(self.layers):
            if l['type'] == 'linear':
                current_out = "buf_a" if current_in != "buf_a" else "buf_b"
                code.append(f'        layer_{i}.forward({current_in}, {current_out});')
                current_in = current_out
            elif l['type'] == 'relu':
                code.append(f'        utromfx::relu<{self.layers[i-1]["out_features"]}>({current_in});')
            elif l['type'] == 'tanh':
                code.append(f'        utromfx::tanh<{self.layers[i-1]["out_features"]}>({current_in});')
        
        code.append(f'        std::copy({current_in}, {current_in} + {output_dim}, output);')
        code.append('    }\n}; }')
        return "\n".join(code)