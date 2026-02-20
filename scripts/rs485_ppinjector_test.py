#!/usr/bin/env python3
"""
RS485 test tool for PPInjectorUI protocol.

Usage examples:
  python3 scripts/rs485_ppinjector_test.py --port /dev/ttyUSB0 --mode machine
  python3 scripts/rs485_ppinjector_test.py --port COM5 --baud 115200 --mode monitor
"""

from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from dataclasses import dataclass

try:
    import serial
except ImportError as exc:  # pragma: no cover
    print("Missing dependency: pyserial")
    print("Install with: pip install pyserial")
    raise SystemExit(1) from exc


SAFE_STATES = [
    "INIT_HEATING",
    "INIT_HOT_WAIT",
    "REFILL",
    "READY_TO_INJECT",
    "PURGE_ZERO",
    "CONFIRM_REMOVAL",
]


@dataclass
class MachineModel:
    encoder_turns: float = 12.345
    temp_c: float = 185.0
    state: str = "READY_TO_INJECT"
    error_code_hex: str = "0000"
    error_msg: str = ""
    mould_ok: str = (
        "Default|10.0|20.0|30.0|4.0|5.0|6.0|7.0|8.0|9.0|10.0|11.0|12.0|2D|13.0"
    )
    common_ok: str = (
        "1.0|2.0|300|50|4.0|5.0|6.0|7.0|8.0|9.0|10.0|11.0|12|13"
    )


class RS485Tester:
    def __init__(self, port: str, baud: int, timeout: float, mode: str, telemetry_period: float) -> None:
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.mode = mode
        self.telemetry_period = telemetry_period

        self.ser = serial.Serial(port=self.port, baudrate=self.baud, timeout=self.timeout)
        self.model = MachineModel()
        self._stop = threading.Event()
        self._rx_q: queue.Queue[str] = queue.Queue()

    def close(self) -> None:
        self._stop.set()
        if self.ser.is_open:
            self.ser.close()

    def send_line(self, line: str) -> None:
        payload = (line.rstrip("\r\n") + "\n").encode("utf-8")
        self.ser.write(payload)
        self.ser.flush()
        print(f"[TX] {line}")

    def reader_loop(self) -> None:
        buf = bytearray()
        while not self._stop.is_set():
            chunk = self.ser.read(1)
            if not chunk:
                continue
            ch = chunk[0]
            if ch in (10, 13):
                if buf:
                    line = buf.decode("utf-8", errors="replace").strip()
                    buf.clear()
                    if line:
                        print(f"[RX] {line}")
                        self._rx_q.put(line)
            else:
                buf.append(ch)

    def telemetry_loop(self) -> None:
        idx = 0
        while not self._stop.is_set():
            self.model.encoder_turns += 0.01
            self.model.temp_c += 0.05 if (idx % 20) < 10 else -0.05
            self.model.state = SAFE_STATES[(idx // 10) % len(SAFE_STATES)]
            self.send_line(f"ENC|{self.model.encoder_turns:.3f}")
            self.send_line(f"TEMP|{self.model.temp_c:.1f}")
            self.send_line(f"STATE|{self.model.state}")
            idx += 1
            time.sleep(self.telemetry_period)

    def handle_machine_request(self, line: str) -> None:
        cmd = line.split("|", 1)[0].strip().upper()

        if cmd == "QUERY_STATE":
            self.send_line(f"STATE|{self.model.state}")
            return

        if cmd == "QUERY_ERROR":
            self.send_line(f"ERROR|{self.model.error_code_hex}|{self.model.error_msg}")
            return

        if cmd == "QUERY_MOULD":
            self.send_line(f"MOULD_OK|{self.model.mould_ok}")
            return

        if cmd == "QUERY_COMMON":
            self.send_line(f"COMMON_OK|{self.model.common_ok}")
            return

        if cmd == "MOULD":
            payload = line.split("|", 1)[1] if "|" in line else ""
            if payload:
                self.model.mould_ok = payload
                self.send_line(f"MOULD_OK|{payload}")
            return

        if cmd == "COMMON":
            payload = line.split("|", 1)[1] if "|" in line else ""
            if payload:
                self.model.common_ok = payload
                self.send_line(f"COMMON_OK|{payload}")
            return

    def command_help(self) -> None:
        print("Commands:")
        print("  /help                      Show this help")
        print("  /quit                      Exit")
        print("  /send <RAW_LINE>           Send raw protocol line")
        print("  /state <NAME>              Change simulated state")
        print("  /error <HEX> <MSG...>      Set simulated error")
        print("  /clearerror                Clear simulated error")

    def interactive_loop(self) -> None:
        self.command_help()
        while not self._stop.is_set():
            try:
                raw = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not raw:
                continue

            if raw == "/quit":
                break
            if raw == "/help":
                self.command_help()
                continue
            if raw.startswith("/send "):
                self.send_line(raw[6:])
                continue
            if raw.startswith("/state "):
                self.model.state = raw[7:].strip()
                print(f"State set to: {self.model.state}")
                continue
            if raw.startswith("/error "):
                parts = raw.split(" ", 2)
                if len(parts) < 2:
                    print("Usage: /error <HEX> <MSG>")
                    continue
                self.model.error_code_hex = parts[1]
                self.model.error_msg = parts[2] if len(parts) > 2 else ""
                print(f"Error set: {self.model.error_code_hex} {self.model.error_msg}")
                continue
            if raw == "/clearerror":
                self.model.error_code_hex = "0000"
                self.model.error_msg = ""
                print("Error cleared")
                continue

            print("Unknown command. Type /help")

    def run(self) -> None:
        print(f"Opened {self.port} @ {self.baud} bps (mode={self.mode})")

        reader = threading.Thread(target=self.reader_loop, daemon=True)
        reader.start()

        telemetry = None
        if self.mode == "machine":
            telemetry = threading.Thread(target=self.telemetry_loop, daemon=True)
            telemetry.start()

            def responder() -> None:
                while not self._stop.is_set():
                    try:
                        line = self._rx_q.get(timeout=0.2)
                    except queue.Empty:
                        continue
                    self.handle_machine_request(line)

            responder_thread = threading.Thread(target=responder, daemon=True)
            responder_thread.start()

        self.interactive_loop()


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="RS485 protocol tester for PPInjectorUI")
    p.add_argument("--port", required=True, help="Serial port, e.g. /dev/ttyUSB0 or COM5")
    p.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")
    p.add_argument("--timeout", type=float, default=0.05, help="Read timeout in seconds")
    p.add_argument(
        "--mode",
        choices=["machine", "monitor"],
        default="machine",
        help="machine: auto-reply QUERY/MOULD/COMMON, monitor: RX/TX manual",
    )
    p.add_argument(
        "--telemetry-period",
        type=float,
        default=0.5,
        help="Seconds between periodic ENC/TEMP/STATE updates in machine mode",
    )
    return p.parse_args()


def main() -> int:
    args = parse_args()
    tester = RS485Tester(
        port=args.port,
        baud=args.baud,
        timeout=args.timeout,
        mode=args.mode,
        telemetry_period=args.telemetry_period,
    )
    try:
        tester.run()
    finally:
        tester.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
