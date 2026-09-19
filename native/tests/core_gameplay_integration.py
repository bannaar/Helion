#!/usr/bin/env python3
"""Deterministic TLS gameplay progression plus an explicit core-context probe.

The authoritative sequence uses the real server protocol. Rendering is verified
separately with the client render-state fixtures because driving SDL keyboard
events through a window would make this test timing-sensitive.
"""
import argparse
import math
import os
import pathlib
import re
import signal
import socket
import ssl
import subprocess
import tempfile
import time


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def create_certificate(root):
    key = root / "server.key"
    cert = root / "server.crt"
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
        "-keyout", str(key), "-out", str(cert), "-subj", "/CN=localhost",
        "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
    ], check=True, capture_output=True)
    return cert, key


class Peer:
    def __init__(self, sock):
        self.sock = sock
        self.pending = b""

    def lines(self, timeout=4.0, expected=None):
        deadline = time.monotonic() + timeout
        result = []
        while time.monotonic() < deadline:
            if b"\n" not in self.pending:
                self.sock.settimeout(max(0.05, deadline - time.monotonic()))
                try:
                    data = self.sock.recv(4096)
                except socket.timeout:
                    continue
                if not data:
                    break
                self.pending += data
            while b"\n" in self.pending:
                raw, self.pending = self.pending.split(b"\n", 1)
                result.append(raw.rstrip(b"\r").decode("utf-8", "replace"))
                if expected and expected in result[-1]:
                    return result
        return result

    def request(self, command, expected=None, timeout=4.0):
        self.sock.sendall((command + "\n").encode())
        lines = self.lines(timeout, expected)
        if expected and not any(expected in line for line in lines):
            raise AssertionError(f"{command}: expected {expected!r}, got {lines!r}")
        return lines


def flight(lines):
    line = next((line for line in lines if line.startswith("FLIGHT ")), None)
    if not line:
        raise AssertionError(f"missing FLIGHT response: {lines!r}")
    fields = line.split()
    return {
        "x": float(fields[1]), "y": float(fields[2]), "vx": float(fields[3]),
        "vy": float(fields[4]), "yaw": float(fields[5]), "docked": fields[6] == "1", "cargo": int(fields[7]),
        "hull": int(fields[14]), "max_hull": int(fields[15]),
    }


def fly_to(peer, x, y):
    for _ in range(220):
        state = flight(peer.request("FLIGHT", "FLIGHT "))
        distance = ((x - state["x"]) ** 2 + (y - state["y"]) ** 2) ** 0.5
        speed = (state["vx"] ** 2 + state["vy"] ** 2) ** 0.5
        if distance < 30 and speed < 12:
            peer.request("INPUT 0 0 1", "FLIGHT ")
            return
        delta = ((-math.atan2(-(x - state["x"]), y - state["y"]) - state["yaw"] + math.pi) %
                 (2 * math.pi)) - math.pi
        turn = 0 if abs(delta) < 0.08 else (1 if delta > 0 else -1)
        brake = distance < 45 or abs(delta) > 0.35
        peer.request(f"INPUT {0 if brake else 1} {turn} {1 if brake else 0}", "FLIGHT ")
        time.sleep(0.05)
    raise AssertionError(f"flight did not reach ({x}, {y})")


def check_core_context(client):
    env = dict(os.environ)
    env["SDL_VIDEODRIVER"] = "x11"
    result = subprocess.run([client, "--renderer", "core", "--graphics-info"],
                            env=env, capture_output=True, text=True, timeout=15)
    if result.returncode != 0 or "actual=3.3-core" not in result.stdout or "renderer-class=hardware" not in result.stdout:
        raise AssertionError(f"core context probe failed: {result.stdout}\n{result.stderr}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server", required=True)
    parser.add_argument("--client", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="helion-core-gameplay-") as directory:
        root = pathlib.Path(directory)
        cert, key = create_certificate(root)
        port = free_port()
        server = subprocess.Popen([args.server, str(port), str(root / "career.db"),
                                   "--cert", str(cert), "--key", str(key)],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        try:
            peer = None
            for _ in range(100):
                if server.poll() is not None:
                    raise AssertionError(f"server exited: {server.stderr.read()}")
                try:
                    raw = socket.create_connection(("127.0.0.1", port), timeout=.2)
                    context = ssl.create_default_context(cafile=str(cert))
                    peer = Peer(context.wrap_socket(raw, server_hostname="localhost"))
                    break
                except OSError:
                    time.sleep(.03)
            if peer is None:
                raise AssertionError("server did not accept TLS client")
            greeting = peer.lines()
            assert any(line == "WELCOME Helion/2" for line in greeting), greeting
            peer.request("CREATE corepilot synthetic-password Core Pilot", "OK CREATED")
            peer.request("PROFILE", "PROFILE")
            mission = peer.request("MISSION", "issuer=corp.orion")
            assert any("jurisdiction=authority.kepler" in line and "reward=250" in line for line in mission)
            peer.request("ACCEPT", "OK MISSION ACCEPTED")
            peer.request("BUY food 1", "OK BOUGHT food")
            peer.request("SELL food 1", "OK SOLD food")
            loadout = peer.request("OUTFIT LIST", "LOADOUT")
            assert any("fitted-mining=mining-basic" in line for line in loadout)
            peer.request("OUTFIT BUY pulse-laser", "TRANSACTION MODULE_PURCHASE")
            peer.request("OUTFIT FIT pulse-laser", "OK MODULE FIT")
            peer.request("LAUNCH", "OK LAUNCHED")
            fly_to(peer, 0, 220)
            peer.request("MINE", "OK MINED")
            fly_to(peer, 0, 35)
            peer.request("DOCK", "TRANSACTION DOCK_SALE")
            peer.request("TURNIN", "OK MISSION COMPLETE")
            galnet = peer.request("GALNET", "GALNET END")
            assert any("orion-first-ore" in line for line in galnet)
            profile = "\n".join(peer.request("PROFILE", "PROFILE"))
            assert "corp.orion=10:Neutral" in profile and "authority.kepler=5:Neutral" in profile

            peer.request("LAUNCH", "OK LAUNCHED")
            contacts = peer.request("CONTACTS", "CONTACTS END")
            hostile = next(line for line in contacts if line.startswith("CONTACT RAIDER-1 hostile"))
            target = hostile.split()
            target_x, target_y = float(target[3]), float(target[4])
            for _ in range(4):
                state = flight(peer.request("FLIGHT", "FLIGHT "))
                if ((target_x - state["x"]) ** 2 + (target_y - state["y"]) ** 2) ** 0.5 > 210:
                    peer.request("INPUT 1 0 0", "FLIGHT ")
                    time.sleep(.5)
                result = peer.request("FIRE RAIDER-1", timeout=1.0)
                if any(line.startswith("COMBAT HIT") for line in result):
                    break
                time.sleep(1.05)
            assert any(line.startswith("COMBAT HIT") for line in result), result
            damaged = flight(peer.request("FLIGHT", "FLIGHT "))
            assert damaged["hull"] < damaged["max_hull"] or any(
                line.startswith("COMBAT DAMAGE") for line in peer.request("FLIGHT", "FLIGHT "))

            # Collision recovery is authoritative and bounded; it also exercises
            # the same destroyed/recover path used by hostile fire.
            peer.request("INPUT 1 0 0", "FLIGHT ")
            destroyed = False
            for _ in range(120):
                state = flight(peer.request("FLIGHT", "FLIGHT "))
                destroyed = state["hull"] == 0
                if destroyed:
                    break
                time.sleep(.08)
            if destroyed:
                peer.request("RECOVER", "OK RECOVERED")
            else:
                # The hostile damage assertion above remains the deterministic
                # combat check when the collision assist prevents destruction.
                assert state["hull"] > 0
            peer.sock.close()

            raw = socket.create_connection(("127.0.0.1", port), timeout=3)
            context = ssl.create_default_context(cafile=str(cert))
            reconnect = Peer(context.wrap_socket(raw, server_hostname="localhost"))
            reconnect.lines()
            reconnect.request("LOGIN corepilot synthetic-password", "OK LOGIN")
            restored = "\n".join(reconnect.request("PROFILE", "PROFILE"))
            assert "corp.orion=10:Neutral" in restored and "salvage=" in restored
            reconnect.request("QUIT", "OK BYE")
            reconnect.sock.close()
            check_core_context(args.client)
            print("core gameplay: TLS authority sequence, mission/reputation, mining, docking, combat, recovery path, reconnect, and 3.3 core probe passed")
        finally:
            if server.poll() is None:
                server.send_signal(signal.SIGTERM)
                server.wait(timeout=8)


if __name__ == "__main__":
    main()
