#!/usr/bin/env python3
"""Run the bounded core cockpit fixtures and check stable render invariants."""
import argparse
import os
import pathlib
import re
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    states = ("normal", "mining", "target", "combat", "docked", "destroyed", "galnet",
              "account", "station", "market", "mission", "outfit", "shipyard", "profile", "galnet-ui", "options", "graphics",
              "error", "help", "completion")
    env = dict(os.environ, SDL_VIDEODRIVER="x11")
    expected_renderer = "software" if os.environ.get("HELION_HEADLESS_SOFTWARE_GL") == "1" else "hardware"
    with tempfile.TemporaryDirectory(prefix="helion-core-render-") as directory:
        root = pathlib.Path(directory)
        for state in states:
            for width, height in ((640, 400), (960, 600), (1280, 720)):
                frame = root / f"{state}-{width}x{height}.bmp"
                result = subprocess.run([
                    args.client, "--renderer", "core", "--render-check", str(frame),
                    "--render-state", state, "--render-size", str(width), str(height),
                ], env=env, capture_output=True, text=True, timeout=15)
                context = f"{state} {width}x{height}"
                if result.returncode != 0:
                    raise AssertionError(f"{context} failed:\n{result.stdout}\n{result.stderr}")
                if ("actual=3.3-core" not in result.stdout or
                        f"renderer-class={expected_renderer}" not in result.stdout):
                    raise AssertionError(f"{context} did not use the verified core context: {result.stdout}")
                if "draw-calls=4" not in result.stdout or "text-draw-calls=1" not in result.stdout or "textures=1" not in result.stdout:
                    raise AssertionError(f"{context} missing text render stats: {result.stdout}")
                if "text-components=" not in result.stdout or "text-bytes=" not in result.stdout or "cpu-build-ms=" not in result.stdout:
                    raise AssertionError(f"{context} missing telemetry units: {result.stdout}")
                metrics = dict(re.findall(r"(text-vertices|text-indices|glyphs|atlas|atlas-bytes)=([^ ]+)", result.stdout))
                if int(metrics["text-vertices"]) != int(metrics["glyphs"]) * 6:
                    raise AssertionError(f"{context} violates one-quad-per-glyph telemetry: {result.stdout}")
                if metrics["text-indices"] != "0" or metrics["atlas"] != "96x48":
                    raise AssertionError(f"{context} has unexpected atlas/index metrics: {result.stdout}")
                data = frame.read_bytes()
                if len(data) <= 1024 or data[:2] != b"BM" or struct.unpack_from("<ii", data, 18) != (width, height):
                    raise AssertionError(f"{context} produced an empty or wrongly sized frame")
    print(f"core render states: {len(states)} states at 640x400, 960x600, and 1280x720 passed on {expected_renderer}")


if __name__ == "__main__":
    main()
