#!/usr/bin/env python3
"""Exercise guarded core-init fault points and clean legacy recreation."""
import argparse
import os
import subprocess


STAGES = ("context", "version", "profile", "missing-function", "shader",
          "program", "atlas", "buffer", "renderer", "software")


def invoke(client, mode, stage=None):
    env = dict(os.environ, SDL_VIDEODRIVER="x11", HELION_GRAPHICS_TEST_FAULTS="1")
    command = [client, "--renderer", mode, "--graphics-info"]
    if stage:
        command += ["--test-core-failure", stage]
    return subprocess.run(command, env=env, capture_output=True, text=True, timeout=20)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--client", required=True)
    args = parser.parse_args()

    normal = invoke(args.client, "auto")
    assert normal.returncode == 0, normal.stderr
    assert "GRAPHICS SELECT requested=auto selected=core fallback=no" in normal.stdout, normal.stdout
    assert "actual=3.3-core" in normal.stdout and "renderer-class=hardware" in normal.stdout

    for stage in STAGES:
        automatic = invoke(args.client, "auto", stage)
        assert automatic.returncode == 0, (stage, automatic.stdout, automatic.stderr)
        assert f"core-failed={stage}" in automatic.stderr or (
            stage == "software" and "core-failed=renderer-policy" in automatic.stderr), (
                stage, automatic.stderr)
        assert "GRAPHICS SELECT requested=auto selected=legacy fallback=yes" in automatic.stdout
        assert "actual=" in automatic.stdout and "-compatibility" in automatic.stdout

        explicit = invoke(args.client, "core", stage)
        assert explicit.returncode != 0, (stage, explicit.stdout, explicit.stderr)
        assert "falling back to legacy" not in explicit.stderr
        assert "initialization failed at" in explicit.stderr

    legacy = invoke(args.client, "legacy")
    assert legacy.returncode == 0, legacy.stderr
    assert "GRAPHICS SELECT requested=legacy selected=legacy fallback=no" in legacy.stdout
    print(f"renderer fallback: auto core plus {len(STAGES)} clean legacy fallbacks, "
          "explicit-core failures, and explicit legacy passed")


if __name__ == "__main__":
    main()
