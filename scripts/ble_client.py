import argparse
import asyncio
from bleak import BleakClient, BleakScanner

CHAR_UUID = "33333333-2222-2222-1111-111100000000"

def parse_args():
    parser = argparse.ArgumentParser(description="Simple BLE client")
    parser.add_argument("name_or_address", help="BLE device name (or MAC address)")
    parser.add_argument("payload", help="String to send to the characteristic")
    parser.add_argument("--timeout", type=float, default=10.0, help="Scan timeout in seconds")
    parser.add_argument("--wait", type=float, default=2.0, help="Seconds to wait for notify")
    parser.add_argument(
        "--repeat",
        type=int,
        default=0,
        help="Repeat interval in milliseconds (0 disables)",
    )
    return parser.parse_args()

def looks_like_mac(value: str) -> bool:
    return len(value.split(":")) == 6

async def find_device(name_or_address: str, timeout: float):
    if looks_like_mac(name_or_address):
        return await BleakScanner.find_device_by_address(name_or_address, timeout=timeout)
    return await BleakScanner.find_device_by_filter(
        lambda d, _: d.name and d.name.startswith(name_or_address),
        timeout=timeout,
    )

async def main():
    args = parse_args()
    device = await find_device(args.name_or_address, args.timeout)
    if not device:
        raise RuntimeError("No se encuentra el dispositivo BLE")

    def on_notify(_, data: bytearray):
        try:
            print(data.decode("utf-8"))
        except UnicodeDecodeError:
            print(data)

    async with BleakClient(device) as client:
        await client.start_notify(CHAR_UUID, on_notify)
        payload = args.payload.encode("utf-8")
        if args.repeat > 0:
            interval = args.repeat / 1000.0
            try:
                while True:
                    await client.write_gatt_char(CHAR_UUID, payload, response=True)
                    await asyncio.sleep(interval)
            except asyncio.CancelledError:
                raise
            except KeyboardInterrupt:
                pass
        else:
            await client.write_gatt_char(CHAR_UUID, payload, response=True)
            await asyncio.sleep(args.wait)  # espera a la notificación
        await client.stop_notify(CHAR_UUID)

asyncio.run(main())
