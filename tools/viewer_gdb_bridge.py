"""Local MCUViewer GDB remote bridge to the existing C28x wireless UART driver.

Addresses are C28x word addresses; transfer lengths are octets. No CPU halt,
reset, flash, register writes or arbitrary monitor commands are implemented.
Firmware remains responsible for control and protection. Bind localhost only.
"""
import argparse
import json
import logging
import re
import socket
import struct
from pathlib import Path

import serial
from test_viewer_link import crc16, read_values

ROOT = Path(__file__).resolve().parents[1]


def symbols():
    text = (ROOT / 'ccs_project/CPU1_FLASH/BUCK_F28P55X.map').read_text()
    return {n: int(a, 16) for a, n in re.findall(r'^0\s+([0-9a-fA-F]+)\s+(\S+)\s*$', text, re.M)}


class Memory:
    def __init__(self, uart, addresses):
        self.uart = uart
        self.writable = {addresses[n] for n in [
            'power_mode', 'control_mode', 'current_reference', 'output_voltage_target',
            'openloop_command', 'current_limit', 'pwm_limit']}
        for group in ['buck_current_settings', 'buck_dual_settings',
                      'boost_current_settings', 'boost_dual_settings', 'buck_voltage_settings']:
            self.writable.update(addresses[group] + 2 * i for i in range(4))

    def read(self, address, size):
        if size < 2 or size > 256 or size % 2:
            raise ValueError('Only even 2..256 octet reads are supported')
        end = address + size // 2
        if not (0x128 <= address < end <= 0x7f8 or 0x8000 <= address < end <= 0x18000):
            raise ValueError('Read outside firmware RAM whitelist')
        entries = []
        remaining = size
        while remaining:
            count = 4 if remaining >= 4 and address % 2 == 0 else 2
            entries.append((address, count))
            address += count // 2
            remaining -= count
        data = bytearray()
        for start in range(0, len(entries), 32):
            batch = entries[start:start + 32]
            for value, (_, count) in zip(read_values(self.uart, batch), batch):
                data.extend(value.to_bytes(count, 'little'))
        return data.hex()

    def write(self, address, data):
        if len(data) != 4 or address not in self.writable:
            raise ValueError('Write outside parameter whitelist')
        frame = b'\xaa\x55\x02\x04' + struct.pack('<I', address) + data
        frame += struct.pack('<H', crc16(frame))
        self.uart.reset_input_buffer()
        self.uart.write(frame)
        self.uart.flush()
        if self.uart.read(1) != b'\0':
            raise RuntimeError('Firmware did not acknowledge parameter write')


def respond(command, memory):
    if command.startswith('m'):
        address, size = command[1:].split(',')
        return memory.read(int(address, 16), int(size, 16))
    if command.startswith('M'):
        fields, data = command[1:].split(':')
        address, size = fields.split(',')
        raw = bytes.fromhex(data)
        if len(raw) != int(size, 16):
            raise ValueError('Write length mismatch')
        memory.write(int(address, 16), raw)
        return 'OK'
    if command.startswith('qSupported'):
        return 'PacketSize=1000'
    if command in ['qAttached', 'qC']:
        return '1' if command == 'qAttached' else 'QC1'
    if command == 'qfThreadInfo':
        return 'm1'
    if command == 'qsThreadInfo':
        return 'l'
    if command == '?':
        return 'S05'
    if command.startswith(('H', 'T', '!')) or command == 'D':
        return 'OK'
    return ''


def serve_client(connection, memory):
    buffer = bytearray()
    while True:
        data = connection.recv(4096)
        if not data:
            return
        buffer.extend(data)
        while b'$' in buffer:
            start = buffer.index(b'$')
            del buffer[:start]
            end = buffer.find(b'#', 1)
            if end < 0 or len(buffer) < end + 3:
                if len(buffer) > 8192:
                    raise ValueError('Oversized RSP packet')
                break
            payload = bytes(buffer[1:end])
            checksum = bytes(buffer[end + 1:end + 3])
            del buffer[:end + 3]
            if checksum.lower() != f'{sum(payload) & 255:02x}'.encode():
                connection.sendall(b'-')
                continue
            connection.sendall(b'+')
            try:
                result = respond(payload.decode('ascii'), memory).encode('ascii')
            except Exception as error:
                logging.warning('Request failed: %s', error)
                result = b'E03'
            connection.sendall(b'$' + result + b'#' + f'{sum(result) & 255:02x}'.encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM14')
    parser.add_argument('--listen', type=int, default=3334)
    args = parser.parse_args()
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(message)s')
    addresses = symbols()
    with serial.Serial(args.port, 115200, timeout=0.3, write_timeout=1) as uart:
        driver = read_values(uart, [(addresses['____serialDriverSettings'] + 2 * i, 4) for i in range(3)])
        if driver != [1, 2, 32]:
            raise RuntimeError('Unexpected DSP firmware/serial driver')
        memory = Memory(uart, addresses)
        with socket.socket() as server:
            server.bind(('127.0.0.1', args.listen))
            server.listen(1)
            logging.info('READY 127.0.0.1:%s -> %s; C28x word addresses; driver %s', args.listen, args.port, driver)
            while True:
                client, peer = server.accept()
                logging.info('Client connected %s', peer)
                with client:
                    try:
                        serve_client(client, memory)
                    except (ConnectionError, OSError, ValueError) as error:
                        logging.info('Client disconnected: %s', error)


if __name__ == '__main__':
    main()
