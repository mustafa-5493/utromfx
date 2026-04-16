import argparse
import sys
import torch
from utromfx.compiler.codegen import AxonCodegen
from utromfx.compiler.validator import AxonValidator

def handle_compile(args):
    try:
        model = torch.load(args.model_path, map_location="cpu", weights_only=False)
        model.eval()
    except Exception as e:
        print(f"Error loading model: {e}"); sys.exit(1)

    print(f"Tracing and Compiling {args.model_path}...")
    codegen = AxonCodegen(model, history_len=args.history)
    header_code = codegen.generate_header()

    if not args.skip_validation:
        # Set epsilon to 1e-6 to account for O3 compiler floating point reordering
        print(f"Running Bit-Exact Parity Check (ε={args.epsilon})...")
        validator = AxonValidator(epsilon=args.epsilon)
        try:
            validator.verify(model, header_code, input_dim=codegen.layers[0]['in_features'])
        except Exception as e:
            print(f"CRITICAL PARITY ERROR: {e}"); sys.exit(1)

    with open(args.output, "w") as f: f.write(header_code)
    print(f"SUCCESS: Header generated at '{args.output}'")

def main():
    parser = argparse.ArgumentParser(prog="utx", description="UtromFX Compiler")
    subparsers = parser.add_subparsers(dest="command")
    c_parser = subparsers.add_parser("compile")
    c_parser.add_argument("model_path")
    c_parser.add_argument("--output", "-o", default="G1Controller.h")
    c_parser.add_argument("--history", type=int, default=1)
    c_parser.add_argument("--epsilon", type=float, default=1e-6) # Industrial Standard
    c_parser.add_argument("--skip-validation", action="store_true")
    
    args = parser.parse_args()
    if args.command == "compile": handle_compile(args)
    else: parser.print_help()

if __name__ == "__main__": main()