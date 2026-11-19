"""Regression tests for fragmented clipboard control sequences, without touching Windows."""
from pathlib import Path
import base64
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from clipboard import ClipboardBridge


class BridgeTests(unittest.TestCase):
    def test_fragmented(self):
        copied = []
        bridge = ClipboardBridge(lambda text: copied.append(text) or True)
        code = 'P2-4SW5-CD0J-0228E'
        raw = '\x1b[2;3HПривет'.encode()
        request = b'\x1b]52;c;' + base64.b64encode(code.encode()) + b'\x07'
        output = b''; replies = b''
        for byte in raw + request + b'\x1b[0m':
            a, b = bridge.feed(bytes([byte])); output += a; replies += b
        self.assertEqual(output, raw + b'\x1b[0m')
        self.assertEqual(copied, [code]); self.assertEqual(replies, b'\x1b[901~')

    def test_failure(self):
        bridge = ClipboardBridge(lambda _: False)
        request = b'\x1b]52;c;' + base64.b64encode(b'P2-4SW5-CD0J-0228E') + b'\x07'
        self.assertEqual(bridge.feed(request), (b'', b'\x1b[902~'))
        self.assertEqual(bridge.feed(b'\x1b]52;c;!!!\x07OK'), (b'OK', b'\x1b[902~'))
        self.assertEqual(bridge.feed(b'\x1b]52;c;' + b'A' * 1000), (b'', b'\x1b[902~'))
        self.assertLessEqual(len(bridge.pending), 128)
        self.assertEqual(bridge.feed(b'tail\x07OK'), (b'OK', b''))


if __name__ == '__main__':
    unittest.main()
