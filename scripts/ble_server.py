import argparse
import asyncio
import json
import time

from bless import BlessServer
from bless import GATTAttributePermissions, GATTCharacteristicProperties

SERVICE_UUID = "59462f12-9543-9999-12c8-58b459a2712d"
CHAR_UUID = "33333333-2222-2222-1111-111100000000"
SIM_PERIOD_S = 1.0
HEATER_CYCLE_S = 60.0
MAX_SIM_STEPS = 10


class MeasurementSim:
    def __init__(self) -> None:
        self.reset()

    def reset(self) -> None:
        self.setpoint = 12.0
        self.temp_in = 15.0
        self.temp_out = 10.0
        self.heater = False
        self.cooler = False
        self.mode = 3
        self.status = 0
        self._setpoint_counter = 0
        self._last_update = time.monotonic()

    def _step(self) -> None:
        if self.heater:
            self.temp_in += 0.01
        else:
            self.temp_in += (self.temp_out - self.temp_in) * 0.005

        if self._setpoint_counter <= 0:
            self.setpoint = 12.0 if self.setpoint == 22.0 else 22.0
            self._setpoint_counter = int(HEATER_CYCLE_S / SIM_PERIOD_S) - 1
        else:
            self._setpoint_counter -= 1

        self.heater = self.setpoint > self.temp_in
        if (self.heater):
            self.status = 2

    def update(self) -> None:
        now = time.monotonic()
        elapsed = now - self._last_update
        if elapsed <= 0:
            return
        steps = int(elapsed / SIM_PERIOD_S)
        if steps <= 0:
            return
        steps = min(steps, MAX_SIM_STEPS)
        for _ in range(steps):
            self._step()
        self._last_update += steps * SIM_PERIOD_S


SIM = MeasurementSim()


def parse_args():
    parser = argparse.ArgumentParser(description="BLE peripheral emulator")
    parser.add_argument(
        "--name",
        default="POR-EMU",
        help="Advertised BLE name",
    )
    return parser.parse_args()


def dispatch_response(command: str) -> str:
    if not command:
        return ""
    first = command[0]
    if first == "w":
        print("WINK command received")
        return '{"w":"ok"}'
    if first == "i":
        SIM.update()
        print("INFO command received")
        payload = {
            "Measurement": {
                "Temp-Setpoint": SIM.setpoint,
                "ai1": SIM.temp_in,
                "ai2": SIM.temp_out,
                "bi0": SIM.heater
            }
        }
        return json.dumps(payload)
    if first == "r":
        print("RESET command received")
        SIM.reset()
        return '{"r":"ok"}'
    return "unknown command"


def decode_value(value) -> str:
    try:
        return bytes(value).decode("utf-8")
    except Exception:
        return ""


async def main():
    args = parse_args()

    server = BlessServer(name=args.name)
    await server.add_new_service(SERVICE_UUID)
    await server.add_new_characteristic(
        SERVICE_UUID,
        CHAR_UUID,
        GATTCharacteristicProperties.read
        | GATTCharacteristicProperties.write
        | GATTCharacteristicProperties.notify,
        None,
        GATTAttributePermissions.readable | GATTAttributePermissions.writeable,
    )

    def read_request(characteristic, **_kwargs):
        print("READ request")
        return characteristic.value

    def write_request(characteristic, value, **_kwargs):
        command = decode_value(value)
        print(f"WRITE request value={command!r}")
        response = dispatch_response(command)
        characteristic.value = response.encode("utf-8")
        print(f"WRITE response={response!r}")
        server.update_value(SERVICE_UUID, CHAR_UUID)

    server.read_request_func = read_request
    server.write_request_func = write_request

    await server.start()
    try:
        while True:
            await asyncio.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        await server.stop()


asyncio.run(main())
