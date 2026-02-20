#!/usr/bin/env python3
"""
Interactive UART command console for PPInjectorUI protocol.

Usage:
  python3 scripts/uart_command_console.py
  python3 scripts/uart_command_console.py --port /dev/ttyUSB0 --baud 115200
"""

from __future__ import annotations

import argparse
import sys
import threading

try:
    import serial
except ImportError as exc:  # pragma: no cover
    print("Missing dependency: pyserial")
    print("Install with: pip install pyserial")
    raise SystemExit(1) from exc


COMMANDS = [
    ("QUERY_STATE", []),
    ("QUERY_ERROR", []),
    ("QUERY_MOULD", []),
    ("QUERY_COMMON", []),
    ("ENC|{turns}", ["turns"]),
    ("TEMP|{temp_c}", ["temp_c"]),
    ("STATE|{name}", ["name"]),
    ("ERROR|{hex_code}|{message}", ["hex_code", "message"]),
    ("MOULD_OK|{payload}", ["payload"]),
    ("COMMON_OK|{payload}", ["payload"]),
    ("MOCK|STATE|{name}", ["name"]),
    ("MOCK|POS|{turns}", ["turns"]),
    ("MOCK|TEMP|{temp_c}", ["temp_c"]),
    ("MOCK|OFF", []),
    ("RAW", ["line"]),
]


class UartConsole:
    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self.ser = serial.Serial(port=port, baudrate=baud, timeout=timeout)
        self.stop = threading.Event()
        self._rx_thread = threading.Thread(target=self._reader_loop, daemon=True)

    def start(self) -> None:
        self._rx_thread.start()

    def close(self) -> None:
        self.stop.set()
        if self.ser.is_open:
            self.ser.close()

    def _reader_loop(self) -> None:
        buf = bytearray()
        while not self.stop.is_set():
            chunk = self.ser.read(1)
            if not chunk:
                continue
            c = chunk[0]
            if c in (10, 13):
                if buf:
                    line = buf.decode("utf-8", errors="replace").strip()
                    buf.clear()
                    if line:
                        print(f"\n[RX] {line}")
                        print("> ", end="", flush=True)
            else:
                buf.append(c)

    def send_line(self, line: str) -> None:
        payload = (line.rstrip("\r\n") + "\n").encode("utf-8")
        self.ser.write(payload)
        self.ser.flush()
        print(f"[TX] {line}")


def build_line_from_choice(choice: int) -> str | None:
    if choice < 1 or choice > len(COMMANDS):
        print("Invalid choice.")
        return None

    template, args = COMMANDS[choice - 1]

    if template == "RAW":
        raw = input("Raw line: ").strip()
        return raw or None

    values = {}
    for arg in args:
        value = input(f"{arg}: ").strip()
        values[arg] = value

    line = template.format(**values)
    edited = input(f"Edit line (Enter to keep): [{line}] ").strip()
    return edited if edited else line


def print_menu() -> None:
    print("\nAvailable commands:")
    for idx, (template, _) in enumerate(COMMANDS, start=1):
        print(f"  {idx:2d}. {template}")
    print("   q. Quit")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Interactive UART command console")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate (default: 115200)")
    parser.add_argument("--timeout", type=float, default=0.05, help="Read timeout in seconds")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        console = UartConsole(port=args.port, baud=args.baud, timeout=args.timeout)
    except Exception as exc:
        print(f"Failed to open serial port {args.port}: {exc}")
        return 1

    print(f"Opened {args.port} @ {args.baud} bps")
    console.start()

    try:
        while True:
            print_menu()
            raw = input("> ").strip().lower()
            if raw in {"q", "quit", "exit"}:
                break
            if not raw:
                continue
            if not raw.isdigit():
                print("Please enter a number from the list or 'q'.")
                continue

            line = build_line_from_choice(int(raw))
            if line:
                console.send_line(line)
    except (KeyboardInterrupt, EOFError):
        print()
    finally:
        console.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
