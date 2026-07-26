#!/usr/bin/env python3
"""Stream a drive_model cycle to the cluster's USB frame-injector.

The P4 firmware always runs a reader (usb_inject.c) that turns "#F <hex>"
lines on the USB-Serial-JTAG port into J1850 frames and feeds them to the
real decoder. This tool drives that reader from a Mac so a bench cluster with
no live bus shows a realistic gauge.

The frame stream comes from the host generator (frame_inject_gen), which
reuses the exact tested C encoder, so nothing diverges from the firmware.

    pip install pyserial
    # build the generator once:
    (cd firmware/test_apps/host && cmake -B build -S . && \
        cmake --build build --target frame_inject_gen)
    ./inject_stream.py [-p PORT] [--gen PATH] [--once]

Ctrl-C to stop. The '@<ms>' markers in the generator output set the pacing;
'#F' lines are written verbatim to the port.
"""

import argparse
import pathlib
import subprocess
import sys
import time

import serial

DEFAULT_PORT = "/dev/cu.usbmodem5B5F0299541"
REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_GEN = REPO_ROOT / "firmware/test_apps/host/build/frame_inject_gen"


def gen_lines(gen_path, duration_ms, tick_ms):
    """Run the generator once and return its output lines."""
    out = subprocess.run(
        [str(gen_path), str(duration_ms), str(tick_ms)],
        check=True, capture_output=True, text=True,
    ).stdout
    return out.splitlines()


def stream_once(port, lines):
    """Play one pass of the line list, pacing on '@<ms>' markers."""
    t0 = time.monotonic()
    for line in lines:
        if line.startswith("@"):
            target_s = int(line[1:]) / 1000.0
            dt = t0 + target_s - time.monotonic()
            if dt > 0:
                time.sleep(dt)
        elif line.startswith("#F"):
            port.write((line + "\n").encode("ascii"))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-p", "--port", default=DEFAULT_PORT)
    ap.add_argument("--gen", default=str(DEFAULT_GEN), help="path to frame_inject_gen")
    ap.add_argument("--duration", type=int, default=60000, help="cycle length in ms")
    ap.add_argument("--tick", type=int, default=100, help="snapshot interval in ms")
    ap.add_argument("--once", action="store_true", help="play one cycle then exit")
    args = ap.parse_args()

    gen_path = pathlib.Path(args.gen)
    if not gen_path.exists():
        sys.exit(f"generator not found: {gen_path}\n"
                 "build it: (cd firmware/test_apps/host && cmake -B build -S . && "
                 "cmake --build build --target frame_inject_gen)")

    lines = gen_lines(gen_path, args.duration, args.tick)
    n_frames = sum(1 for line in lines if line.startswith("#F"))
    print(f"{n_frames} frames over {args.duration/1000:.0f}s -> {args.port}"
          f"{' (once)' if args.once else ' (looping, Ctrl-C to stop)'}")

    with serial.Serial(args.port, 115200, timeout=1) as port:
        try:
            while True:
                stream_once(port, lines)
                if args.once:
                    break
        except KeyboardInterrupt:
            print("\nstopped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
