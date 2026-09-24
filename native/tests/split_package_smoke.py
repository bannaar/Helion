#!/usr/bin/env python3
"""Audit split installs and exercise a headless server with the client-only build."""
import argparse
import os
import pathlib
import shutil
import signal
import socket
import subprocess
import tarfile
import tempfile
import time


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def certificate(root, name):
    target = root / name
    target.mkdir()
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
        "-keyout", str(target / "server.key"), "-out", str(target / "server.crt"),
        "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost",
    ], check=True, capture_output=True)
    return target / "server.crt", target / "server.key"


def wait_listening(process, port):
    for _ in range(100):
        if process.poll() is not None:
            out, err = process.communicate()
            raise AssertionError(f"server exited\n{out}\n{err}")
        with socket.socket() as probe:
            if probe.connect_ex(("127.0.0.1", port)) == 0:
                return
        time.sleep(.05)
    raise AssertionError("headless server did not listen")


def start(server, port, database, cert, key):
    process = subprocess.Popen([
        str(server), str(port), str(database), "--environment", "development", "--bind", "127.0.0.1",
        "--cert", str(cert), "--key", str(key),
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    wait_listening(process, port)
    return process


def stop(process):
    process.send_signal(signal.SIGTERM)
    process.wait(timeout=10)
    out, err = process.communicate()
    assert process.returncode == 0, (process.returncode, out, err)


def terminal(client, host, port, ca, commands):
    return subprocess.run([
        str(client), host, str(port), "--ca", str(ca), "--terminal",
    ], input=commands, capture_output=True, text=True, timeout=20)


def assert_no_private_material(root):
    forbidden = {"server.key", "commander.db"}
    for path in root.rglob("*"):
        assert path.name not in forbidden and path.suffix not in {".key", ".db"}, path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--server-build", required=True)
    parser.add_argument("--client-build", required=True)
    parser.add_argument("--server-deb")
    parser.add_argument("--client-deb")
    parser.add_argument("--server-tgz")
    parser.add_argument("--client-tgz")
    args = parser.parse_args()
    if bool(args.server_deb) != bool(args.client_deb):
        parser.error("supply both split Debian packages or neither")
    if bool(args.server_tgz) != bool(args.client_tgz):
        parser.error("supply both split tarballs or neither")
    if args.server_tgz:
        with tarfile.open(args.server_tgz, "r:gz") as archive:
            server_members = {member.name for member in archive.getmembers()}
        with tarfile.open(args.client_tgz, "r:gz") as archive:
            client_members = {member.name for member in archive.getmembers()}
        assert "usr/bin/helion_server" in server_members
        assert "usr/bin/helion_client" not in server_members
        assert "usr/bin/helion_client" in client_members
        assert "usr/bin/helion_server" not in client_members
        assert not any(name.endswith((".key", ".db")) for name in server_members | client_members)

    with tempfile.TemporaryDirectory(prefix="helion-split-package-") as directory:
        root = pathlib.Path(directory)
        server_prefix, client_prefix = root / "server-install", root / "client-install"
        if args.server_deb:
            subprocess.run(["dpkg-deb", "-x", args.server_deb, str(server_prefix)],
                           check=True, capture_output=True, timeout=30)
            subprocess.run(["dpkg-deb", "-x", args.client_deb, str(client_prefix)],
                           check=True, capture_output=True, timeout=30)
            server_control = subprocess.run(["dpkg-deb", "-f", args.server_deb, "Depends"],
                                            check=True, capture_output=True, text=True).stdout.lower()
            client_control = subprocess.run(["dpkg-deb", "-f", args.client_deb, "Depends"],
                                            check=True, capture_output=True, text=True).stdout.lower()
            assert "libsdl" not in server_control and "libgl" not in server_control
            assert "libsdl" in client_control and "libgl" in client_control
        else:
            subprocess.run(["cmake", "--install", args.server_build, "--prefix", str(server_prefix)],
                           check=True, capture_output=True, timeout=30)
            subprocess.run(["cmake", "--install", args.client_build, "--prefix", str(client_prefix)],
                           check=True, capture_output=True, timeout=30)

        server = server_prefix / ("usr/bin/helion_server" if args.server_deb else "bin/helion_server")
        client = client_prefix / ("usr/bin/helion_client" if args.client_deb else "bin/helion_client")
        server_root = server.parent.parent
        client_root = client.parent.parent
        assert server.is_file() and os.access(server, os.X_OK)
        assert (server.parent / "helion-dev-cert").is_file()
        assert not (server.parent / "helion_client").exists()
        assert not (server_root / "share/applications/helion.desktop").exists()
        dependencies = subprocess.run(["ldd", str(server)], check=True, capture_output=True,
                                      text=True).stdout.lower()
        assert "sdl" not in dependencies and "libgl" not in dependencies and "x11" not in dependencies

        assert client.is_file() and os.access(client, os.X_OK)
        assert (client.parent / "helion-client-launch").is_file()
        assert (client_root / "share/applications/helion.desktop").is_file()
        assert (client_root / "share/icons/hicolor/scalable/apps/helion.svg").is_file()
        assert not (client.parent / "helion_server").exists()
        assert not (client.parent / "helion-dev-cert").exists()
        assert_no_private_material(client_prefix)

        service = next(server_prefix.rglob("helion-server.service"))
        service_text = service.read_text()
        for directive in ("NoNewPrivileges=true", "UMask=0077", "ProtectSystem=strict",
                          "ReadWritePaths=/var/lib/helion", "Restart=on-failure",
                          "--environment development"):
            assert directive in service_text

        cert, key = certificate(root, "tls")
        wrong_cert, _ = certificate(root, "wrong-tls")
        database = root / "durable/commander.db"
        database.parent.mkdir(mode=0o700)
        preference = root / "home/.config/helion/client.conf"
        preference.parent.mkdir(parents=True)
        port = free_port()
        process = start(server, port, database, cert, key)
        try:
            created = terminal(client, "localhost", port, cert,
                               "/create split_pilot strong-test-password Split Pilot\n/profile\n/quit\n")
            assert created.returncode == 0 and "OK CREATED user=split_pilot" in created.stdout, created.stderr
            launcher_env = dict(os.environ, HELION_HOST="localhost", HELION_PORT=str(port),
                                HELION_CA=str(cert), HELION_RENDERER="legacy")
            launcher = subprocess.run([str(client.parent / "helion-client-launch"), "--terminal"],
                                      env=launcher_env, input="/state\n/quit\n",
                                      capture_output=True, text=True, timeout=20)
            assert launcher.returncode == 0 and "STATE profiles=1" in launcher.stdout, launcher.stderr
            preference.write_text(f"host=localhost\nport={port}\nca={cert}\nrenderer=legacy\n")
            saved_env = dict(os.environ, HELION_CLIENT_CONFIG=str(preference))
            for setting in ("HELION_HOST", "HELION_PORT", "HELION_CA", "HELION_RENDERER"):
                saved_env.pop(setting, None)
            saved = subprocess.run([str(client.parent / "helion-client-launch"), "--terminal"],
                                   env=saved_env, input="/state\n/quit\n",
                                   capture_output=True, text=True, timeout=20)
            assert saved.returncode == 0 and "STATE profiles=1" in saved.stdout, saved.stderr
            wrong_ca = terminal(client, "localhost", port, wrong_cert, "/quit\n")
            assert wrong_ca.returncode != 0 and "strong-test-password" not in wrong_ca.stdout + wrong_ca.stderr
            wrong_host = terminal(client, "127.0.0.1", port, cert, "/quit\n")
            assert wrong_host.returncode != 0 and "strong-test-password" not in wrong_host.stdout + wrong_host.stderr
        finally:
            stop(process)

        process = start(server, port, database, cert, key)
        try:
            restored = terminal(client, "localhost", port, cert,
                                "/login split_pilot strong-test-password\n/profile\n/quit\n")
            assert restored.returncode == 0 and "OK LOGIN user=split_pilot" in restored.stdout
        finally:
            stop(process)
        assert database.exists() and (database.stat().st_mode & 0o777) == 0o600

        # Reinstall/upgrade changes package-owned files only; preferences and
        # server data live outside those manifests.
        before_database = database.read_bytes()
        if args.server_deb:
            subprocess.run(["dpkg-deb", "-x", args.server_deb, str(server_prefix)],
                           check=True, capture_output=True, timeout=30)
            subprocess.run(["dpkg-deb", "-x", args.client_deb, str(client_prefix)],
                           check=True, capture_output=True, timeout=30)
        else:
            subprocess.run(["cmake", "--install", args.server_build, "--prefix", str(server_prefix)],
                           check=True, capture_output=True, timeout=30)
            subprocess.run(["cmake", "--install", args.client_build, "--prefix", str(client_prefix)],
                           check=True, capture_output=True, timeout=30)
        assert database.read_bytes() == before_database and preference.exists()

        shutil.rmtree(server_prefix)
        shutil.rmtree(client_prefix)
        assert database.exists() and preference.exists(), "package removal deleted user/server data"
        if args.server_deb:
            # Exercise dpkg's package database and removal in an isolated root.
            # Dependencies are audited above and provided by the host for the
            # executable smoke; they are intentionally not installed in this
            # temporary root, so dpkg's dependency check is bypassed here.
            install_root = root / "dpkg-root"
            install_root.mkdir()
            dpkg_base = ["dpkg", f"--root={install_root}",
                         f"--log={root / 'dpkg.log'}", "--force-not-root"]
            for package in (args.server_deb, args.client_deb):
                subprocess.run(dpkg_base + ["--force-depends", "-i", package],
                               check=True, capture_output=True, text=True, timeout=30)
            assert (install_root / "usr/bin/helion_server").is_file()
            assert (install_root / "usr/bin/helion_client").is_file()
            subprocess.run(dpkg_base + ["-r", "helion-server", "helion-client"],
                           check=True, capture_output=True, text=True, timeout=30)
            assert not (install_root / "usr/bin/helion_server").exists()
            assert not (install_root / "usr/bin/helion_client").exists()
            assert database.exists() and preference.exists(), "dpkg removal deleted external data"
        print("split packages: headless server, client-only desktop, TLS identity rejection, "
              "restart persistence, isolated dpkg install/remove, upgrade preservation and data-safe removal passed")


if __name__ == "__main__":
    main()
