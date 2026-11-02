"""Shows what is inside a glTF model and what the engine will do with it.

    python main.py model.glb
    python main.py model.gltf --strict
    python main.py model.glb --json
"""
import argparse
import json
import sys

import GLTF


def main():
    ap = argparse.ArgumentParser(prog="main.py", description="inspect a glTF model for the blit engine")
    ap.add_argument("model", help=".gltf or .glb file")
    ap.add_argument("--json", action="store_true", help="print the report as JSON")
    ap.add_argument("--strict", action="store_true", help="exit with 1 on warnings too")
    args = ap.parse_args()

    try:
        report = GLTF.Inspect(args.model)
    except (GLTF.GltfError, OSError) as e:
        sys.exit(f"error: {e}")

    if args.json:
        print(json.dumps(report, indent=2))
    else:
        GLTF.PrintReport(report)

    levels = {issue["level"] for issue in report["issues"]}

    if "error" in levels or (args.strict and "warning" in levels):
        sys.exit(1)


if __name__ == "__main__":
    main()
