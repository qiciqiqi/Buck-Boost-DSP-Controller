"""Read-only MCUViewer UART check. Run after flashing this build and wiring UART.

Usage: python tools/test_viewer_link.py --port COMxx
Does not change targets, modes, PI parameters or any MCU memory.
Close MCUViewer/serial terminals first so the port is available.
"""
from pathlib import Path
import argparse
import json
import struct
import time

import serial

ROOT = Path(__file__).resolve().parents[1]


def crc16(data):
    # Match MCUViewer SerialDriver v1.2 exactly; its nibble table is not the
    # standard Modbus nibble table. Do not substitute a generic CRC library.
    table = (0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
             0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440)
    crc = 0xffff
    for byte in data:
        crc = (crc >> 4) ^ table[(crc & 15) ^ (byte & 15)]
        crc = (crc >> 4) ^ table[(crc & 15) ^ (byte >> 4)]
    return crc


def read_values(port, entries):
    frame = bytearray((0xaa, 0x55, 1, len(entries)))
    for address, size in entries:
        frame.extend(struct.pack('<IB', address, size))
    frame.extend(struct.pack('<H', crc16(frame)))
    port.reset_input_buffer()
    port.write(frame)
    port.flush()
    expected = sum(size for _, size in entries) + 2
    response = bytearray()
    deadline = time.monotonic() + 2
    while len(response) < expected and time.monotonic() < deadline:
        response.extend(port.read(expected - len(response)))
    if len(response) != expected:
        raise RuntimeError(f'Timeout: expected {expected} bytes, got {len(response)}. '
                           'Check flashed image, S2=0/0, UART TX/RX/GND and wireless serial mode.')
    if crc16(response[:-2]) != struct.unpack('<H', response[-2:])[0]:
        raise RuntimeError('Response CRC mismatch: UART data must be binary-transparent.')
    values, offset = [], 0
    for _, size in entries:
        values.append(int.from_bytes(response[offset:offset + size], 'little'))
        offset += size
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='Host wireless DAP serial COM port')
    args = parser.parse_args()
    addresses = json.loads((ROOT / 'docs/validation/20261116_OLED_MCUViewer/变量地址.json').read_text())
    report = {'port': args.port, 'baudrate': 115200, 'read_only': True, 'passed': False}
    log = ROOT / 'docs/validation/20261116_OLED_MCUViewer/无线串口实测.json'
    try:
        with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=1) as port:
            time.sleep(0.25)
            address = addresses['____serialDriverSettings']
            version, revision, maximum = read_values(port, [(address + 2 * i, 4) for i in range(3)])
            report['driver'] = {'version': version, 'revision': revision, 'max_variables': maximum}
            if (version, revision, maximum) != (1, 2, 32):
                raise RuntimeError('Unexpected driver settings: check image and map file match.')
            names = ['viewer_heartbeat', 'viewer_rx_bytes', 'viewer_tx_bytes', 'viewer_errors',
                     'power_output_enabled', 'oled_online']
            entries = [(addresses[name], 2 if name in ['power_output_enabled', 'oled_online'] else 4)
                       for name in names]
            samples = []
            for _ in range(5):
                samples.append(dict(zip(names, read_values(port, entries))))
                time.sleep(0.3)
            report['samples'] = samples
            elapsed_ticks = (samples[-1]['viewer_heartbeat'] - samples[0]['viewer_heartbeat']) & 0xffffffff
            if not 1 <= elapsed_ticks < 10000:
                raise RuntimeError('Heartbeat did not advance normally; check target is running.')
            if any(sample['power_output_enabled'] != 0 for sample in samples):
                raise RuntimeError('Unexpected power enable state during zero-target communication check.')
            if samples[-1]['viewer_errors'] != samples[0]['viewer_errors']:
                raise RuntimeError('UART errors increased during communication check.')
            report['passed'] = True
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        log.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(report, indent=2, ensure_ascii=False))
    print('WIRELESS_SERIAL_CHECK_PASSED; no MCU memory was written.')


if __name__ == '__main__':
    main()
