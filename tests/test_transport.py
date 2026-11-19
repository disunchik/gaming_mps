"""Verify UART partial writes, idle reads and optional pyserial loopback."""
from pathlib import Path
import socket
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from run import UartStream, SerialPort

class Port:
    in_waiting = 4
    def __init__(self): self.written = bytearray(); self.data = b'abcd'; self.stalled = False
    def read(self, count):
        data, self.data = self.data[:count], self.data[count:]
        return data
    def write(self, data):
        if self.stalled: return 0
        self.written.extend(data[:2]); return min(2, len(data))

class TransportTests(unittest.TestCase):
    def test_partial_write_and_idle_read(self):
        port = Port(); stream = UartStream(port)
        stream.sendall(b'abcdef'); self.assertEqual(port.written, b'abcdef')
        self.assertEqual(stream.recv(2), b'ab'); self.assertEqual(stream.recv(5), b'cd')
        with self.assertRaises(socket.timeout): stream.recv(5)
        port.stalled = True
        with self.assertRaises(OSError): stream.sendall(b'x')

    def test_serial_loopback(self):
        try: import serial
        except ImportError: self.skipTest('pyserial is supplied by ESP-IDF')
        with SerialPort('loop://') as link:
            link.serial.sendall(b'P2-4SW5-CD0J-0228E')
            self.assertEqual(link.serial.recv(128), b'P2-4SW5-CD0J-0228E')
        self.assertFalse(link.port.is_open)

if __name__ == '__main__': unittest.main()
