#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the Hexagon virt QUP DMA UART with a BusyBox ext2 rootfs."""

import argparse
import hashlib
import os
import select
import socket
import subprocess
import sys
import tempfile
import time


def read_until(sock, marker, timeout, *, log=False):
    data = bytearray()
    deadline = time.monotonic() + timeout
    while marker not in data:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([sock], [], [], remaining)[0]:
            raise TimeoutError(f"waiting for {marker!r}: {bytes(data[-1000:])!r}")
        chunk = sock.recv(65536)
        if not chunk:
            raise RuntimeError("QEMU closed the serial socket")
        data.extend(chunk)
        if log:
            sys.stdout.buffer.write(chunk)
            sys.stdout.buffer.flush()
    return bytes(data)


def run(args):
    with tempfile.TemporaryDirectory(prefix="qup-dma-") as directory:
        servers = []
        for name in ("console", "qup"):
            path = os.path.join(directory, name)
            server = socket.socket(socket.AF_UNIX)
            server.bind(path)
            server.listen(1)
            server.settimeout(30)
            servers.append((server, path))

        command = [
            args.qemu, "-M", "virt", "-smp", "1", "-bios", args.bios,
            "-kernel", args.kernel, "-m", "4G", "-accel", "tcg,thread=multi",
            "-drive", f"if=none,id=hd0,file={args.rootfs},format=raw,snapshot=on",
            "-device", "virtio-blk-device,drive=hd0", "-display", "none",
            "-monitor", "none", "-chardev", f"socket,id=con,path={servers[0][1]}",
            "-serial", "chardev:con", "-chardev", f"socket,id=qup,path={servers[1][1]}",
            "-serial", "chardev:qup", "-append",
            "console=ttyAMA0 mem=892M lpj=89124080 root=/dev/vda rw init=/bin/sh",
        ]
        proc = subprocess.Popen(command)
        try:
            console, _ = servers[0][0].accept()
            qup, _ = servers[1][0].accept()
            with console, qup:
                read_until(console, b"~ # ", 120, log=True)
                console.sendall(b"stty -F /dev/ttyHS1 raw -echo; exec 3<>/dev/ttyHS1; echo PORT_''READY\n")
                read_until(console, b"\r\nPORT_READY\r\n", 30, log=True)

                console.sendall(b"head -c 16384 /dev/zero | tr '\\000' Z >&3; echo TX_''DONE\n")
                tx = read_until(qup, b"Z" * 16384, 30)
                if tx != b"Z" * 16384:
                    raise AssertionError("TX payload mismatch")
                read_until(console, b"\r\nTX_DONE\r\n", 30, log=True)
                print("Verified 16384 transmitted bytes", flush=True)

                payload = bytes(range(256)) * 16
                console.sendall(b"dd if=/dev/ttyHS1 of=/tmp/qup-rx bs=1 count=4096 2>/dev/null & echo RX_''READY\n")
                read_until(console, b"\r\nRX_READY\r\n", 30, log=True)
                qup.sendall(payload)
                console.sendall(b"wait; sha256sum /tmp/qup-rx; echo RX_''DONE\n")
                result = read_until(console, b"\r\nRX_DONE\r\n", 90, log=True)
                digest = hashlib.sha256(payload).hexdigest().encode()
                if digest not in result:
                    raise AssertionError("RX digest mismatch")
                print("Verified 4096 received bytes", flush=True)

                console.sendall(b"exec 3>&-; exec 3<&-; stty -F /dev/ttyHS1 raw -echo; exec 3<>/dev/ttyHS1; printf REOPENED >&3; echo REOPEN_''DONE\n")
                read_until(qup, b"REOPENED", 30)
                read_until(console, b"\r\nREOPEN_DONE\r\n", 30, log=True)
                print("Verified close and reopen", flush=True)
        finally:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
            for server, _ in servers:
                server.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", required=True, help="qemu-system-hexagon binary")
    parser.add_argument("--kernel", required=True, help="Hexagon Linux vmlinux")
    parser.add_argument("--bios", required=True, help="Hexagon v68 H2 loadlinux ELF")
    parser.add_argument("--rootfs", required=True, help="BusyBox ext2 image; writes use a snapshot")
    run(parser.parse_args())
