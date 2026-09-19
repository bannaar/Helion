#!/usr/bin/env python3
"""Run the bounded core cockpit fixtures and check stable render invariants."""
import argparse
import os
import pathlib
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    states = ("normal", "mining", "target", "combat", "docked", "destroyed", "galnet",
              "account", "station", "market", "mission", "outfit", "profile", "options", "graphics", "error")
    env = dict(os.environ, SDL_VIDEODRIVER="x11")
    with tempfile.TemporaryDirectory(prefix="helion-core-render-") as directory:
        root = pathlib.Path(directory)
        for state in states:
            frame = root / f"{state}.bmp"
            result = subprocess.run([
                args.client, "--renderer", "core", "--render-check", str(frame),
                "--render-state", state,
            ], env=env, capture_output=True, text=True, timeout=15)
            if result.returncode != 0:
                raise AssertionError(f"{state} failed:\n{result.stdout}\n{result.stderr}")
            if "actual=3.3-core" not in result.stdout or "renderer-class=hardware" not in result.stdout:
                raise AssertionError(f"{state} did not use the verified hardware core context: {result.stdout}")
            if "draw-calls=4" not in result.stdout or "text-draw-calls=1" not in result.stdout or "textures=1" not in result.stdout:
                raise AssertionError(f"{state} missing text render stats: {result.stdout}")
            if "text-components=" not in result.stdout or "text-bytes=" not in result.stdout or "cpu-build-ms=" not in result.stdout:
                raise AssertionError(f"{state} missing telemetry units: {result.stdout}")
            if frame.stat().st_size <= 1024:
                raise AssertionError(f"{state} produced an empty frame")
    print("core render states: flight, combat, station, account, market, mission, outfitting, profile, GalNet, options, graphics, and error passed")


if __name__ == "__main__":
    main()
