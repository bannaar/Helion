#!/usr/bin/env python3
"""Drive one real SDL/core/TLS career, restart, and verify persistence.

The production client injects ordinary SDL events only when guarded by
HELION_ACCEPTANCE=1 on loopback. No authoritative state is injected here.
"""
import argparse
import os
import pathlib
import re
import signal
import socket
import struct
import subprocess
import tempfile
import time


CHECKPOINTS = (
    "onboarding", "first-ore-accepted", "mining", "first-ore-complete",
    "supply-contract", "market", "outfitting", "red-wake-target", "combat",
    "career-completion", "galnet-complete", "reconnected-profile",
    "post-completion-free-play",
)
PASSWORD_SENTINEL = "Kepler-Accept-2026"


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def start_server(server_path, port, database, cert, key):
    process = subprocess.Popen([
        server_path, str(port), str(database), "--bind", "127.0.0.1",
        "--cert", str(cert), "--key", str(key),
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if process.poll() is not None:
            out, err = process.communicate()
            raise AssertionError(f"server exited during startup\n{out}\n{err}")
        with socket.socket() as probe:
            if probe.connect_ex(("127.0.0.1", port)) == 0:
                return process
        time.sleep(.05)
    process.terminate()
    raise AssertionError("server did not listen within 10 seconds")


def stop_server(process):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        process.wait(timeout=10)
    out, err = process.communicate()
    if process.returncode != 0:
        raise AssertionError(f"server did not stop cleanly: {process.returncode}\n{out}\n{err}")


def run_client(client, port, cert, captures, phase):
    env = dict(os.environ)
    env.update({"HELION_ACCEPTANCE": "1", "SDL_VIDEODRIVER": "x11"})
    result = subprocess.run([
        client, "localhost", str(port), "--ca", str(cert), "--renderer", "core",
        "--acceptance-run", str(captures), "--acceptance-phase", phase,
    ], env=env, capture_output=True, text=True, timeout=240)
    combined = result.stdout + result.stderr
    if PASSWORD_SENTINEL in combined:
        raise AssertionError("acceptance credential leaked to client output")
    if result.returncode != 0 or f"ACCEPTANCE PASS phase={phase}" not in result.stdout:
        raise AssertionError(
            f"graphical acceptance {phase} failed ({result.returncode})\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}")
    if "actual=3.3-core" not in result.stdout or "renderer-class=hardware" not in result.stdout:
        raise AssertionError(f"acceptance did not use hardware OpenGL 3.3 core\n{result.stdout}")
    return result.stdout


def run_soak(client, port, cert, captures, seconds):
    env = dict(os.environ)
    env.update({"HELION_ACCEPTANCE": "1", "SDL_VIDEODRIVER": "x11"})
    command = [client, "localhost", str(port), "--ca", str(cert), "--renderer", "core",
               "--acceptance-run", str(captures), "--acceptance-phase", "soak",
               "--acceptance-seconds", str(seconds)]
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
    samples = []
    def rss():
        try:
            status = pathlib.Path(f"/proc/{process.pid}/status").read_text()
            match = re.search(r"^VmRSS:\s+(\d+)\s+kB$", status, re.MULTILINE)
            return int(match.group(1)) if match else None
        except OSError:
            return None
    deadline = time.monotonic() + seconds + 90
    next_sample = time.monotonic() + 5  # sample the initialized client, not a just-forked process
    last_rss = None
    while process.poll() is None and time.monotonic() < deadline:
        last_rss = rss() or last_rss
        if time.monotonic() >= next_sample:
            if last_rss is not None:
                samples.append(last_rss)
            next_sample += 60
        time.sleep(.25)
    if process.poll() is None:
        process.terminate()
        raise AssertionError("graphical soak hung beyond bounded deadline")
    stdout, stderr = process.communicate(timeout=10)
    if PASSWORD_SENTINEL in stdout + stderr:
        raise AssertionError("soak credential leaked to output")
    if process.returncode != 0 or "ACCEPTANCE PASS phase=soak" not in stdout:
        raise AssertionError(f"soak failed ({process.returncode})\n{stdout}\n{stderr}")
    report = next((line for line in stdout.splitlines() if line.startswith("SOAK ")), "")
    values = dict(token.split("=", 1) for token in report.split()[1:] if "=" in token)
    cycle_match = re.search(r"ACCEPTANCE PASS phase=soak cycles=(\d+)", stdout)
    cycles = int(cycle_match.group(1)) if cycle_match else 0
    if float(values.get("duration-sec", 0)) < seconds or int(values.get("frames", 0)) < seconds * 10:
        raise AssertionError(f"short or stalled soak: {report}")
    if cycles < max(1, seconds // 30):
        raise AssertionError(f"soak did not cycle through its screens and flight: cycles={cycles}")
    if any(values.get(key) != "0" for key in
           ("gl-errors", "render-errors", "sdl-errors", "disconnects")):
        raise AssertionError(f"soak reported errors: {report}")
    if samples and last_rss is not None and last_rss > samples[0] + 200000:
        raise AssertionError(f"soak RSS growth exceeded 200 MiB: start={samples[0]} end={last_rss}")
    print(f"graphical soak: cycles={cycles} {report}; rss-start-kb={samples[0] if samples else 'unavailable'} "
          f"rss-interval-kb={samples} rss-end-kb={last_rss if last_rss is not None else 'unavailable'}")


def validate_bmp(path):
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise AssertionError(f"invalid screenshot: {path}")
    width, height = struct.unpack_from("<ii", data, 18)
    offset = struct.unpack_from("<I", data, 10)[0]
    if width < 640 or height < 400 or offset >= len(data):
        raise AssertionError(f"bad screenshot dimensions: {path} {width}x{height}")
    pixels = data[offset:]
    sample = pixels[::max(1, len(pixels) // 4096)]
    if len(set(sample)) < 8 or not any(sample):
        raise AssertionError(f"blank or low-information screenshot: {path}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    parser.add_argument("--cert-helper", required=True)
    parser.add_argument("--output")
    parser.add_argument("--soak-seconds", type=int, default=0)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="helion-graphical-acceptance-") as directory:
        root = pathlib.Path(directory)
        captures = pathlib.Path(args.output) if args.output else root / "captures"
        captures.mkdir(parents=True, exist_ok=True)
        tls = root / "tls"
        subprocess.run([args.cert_helper, str(tls)], check=True, capture_output=True,
                       text=True, timeout=30)
        cert, key = tls / "server.crt", tls / "server.key"
        database = root / "career.db"
        port = free_port()

        server = start_server(args.server, port, database, cert, key)
        try:
            journey_output = run_client(args.client, port, cert, captures, "journey")
        finally:
            stop_server(server)

        server = start_server(args.server, port, database, cert, key)
        try:
            reconnect_output = run_client(args.client, port, cert, captures, "reconnect")
            if args.soak_seconds:
                run_soak(args.client, port, cert, captures, args.soak_seconds)
        finally:
            stop_server(server)

        for name in CHECKPOINTS:
            validate_bmp(captures / f"{name}.bmp")
        mode = "preserved" if args.output else "temporary"
        print(f"graphical acceptance: real SDL/core UI, TLS career, {len(CHECKPOINTS)} "
              f"screenshots, restart/reconnect, reward idempotence and free play passed; captures={mode}")
        # Keep output variables live so a future accidental credential leak in
        # either phase remains covered by run_client's assertion.
        assert journey_output and reconnect_output


if __name__ == "__main__":
    main()
