"""Regression coverage for physical hotkeys under Russian and English layouts."""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'tools'))
from terminal import key_bytes, KeyboardInput


class KeyboardTests(unittest.TestCase):
    def test_hold_release_and_opposite_keys(self):
        keyboard = KeyboardInput()
        self.assertEqual(keyboard.feed(68, 'в'), b'\x1b[911~d')
        self.assertEqual(keyboard.feed(68, 'в', repeat=2), b'\x1b[911~dd')
        self.assertEqual(keyboard.feed(65, 'ф'), b'\x1b[910~a')
        # Repeating the older key must not steal the most recent direction.
        self.assertEqual(keyboard.feed(68, 'в'), b'\x1b[910~d')
        self.assertEqual(keyboard.feed(65, down=False), b'\x1b[911~')
        self.assertEqual(keyboard.feed(68, down=False), b'\x1b[912~')
        self.assertEqual(keyboard.feed(68, down=False), b'')

    def test_same_axis_focus_and_controls(self):
        keyboard = KeyboardInput()
        keyboard.feed(65, 'ф'); keyboard.feed(37)
        self.assertEqual(keyboard.feed(37, down=False), b'\x1b[910~')
        self.assertEqual(keyboard.release_all(), b'\x1b[912~')
        self.assertEqual(keyboard.release_all(), b'')
        self.assertEqual(keyboard.feed(65, '\x01', controls=0x8), b'\x01')
        self.assertEqual(keyboard.held, [])
        self.assertEqual(keyboard.feed(67, 'с'), b'c')
        self.assertEqual(keyboard.feed(67, down=False), b'')

    def test_russian_letters_use_the_same_commands(self):
        for latin, russian in zip('pqr cwasdf'.replace(' ', ''), 'зйк сцфыва'.replace(' ', '')):
            with self.subTest(key=latin):
                virtual = ord(latin.upper())
                self.assertEqual(key_bytes(virtual, russian), latin.encode())
                self.assertEqual(key_bytes(virtual, latin), latin.encode())
                self.assertEqual(key_bytes(virtual, russian.upper(), controls=0x10), latin.upper().encode())
        self.assertEqual(key_bytes(80, 'з', repeat=3), b'ppp')

    def test_native_controls_and_nonletters(self):
        self.assertEqual(key_bytes(67, '\x03', controls=0x8), b'\x03')
        self.assertEqual(key_bytes(221, '\x1d', controls=0x8), b'\x1d')
        self.assertEqual(key_bytes(80, 'З', controls=0x80), b'P')
        self.assertEqual(key_bytes(80, 'з', controls=0x90), b'p')
        self.assertEqual(key_bytes(37, repeat=2), b'\x1b[D\x1b[D')
        self.assertEqual(key_bytes(13, '\r'), b'\r')
        self.assertEqual(key_bytes(50, '2'), b'2')
        self.assertEqual(key_bytes(121), b'\x1d')
        self.assertEqual(key_bytes(0, '\0'), b'')


if __name__ == '__main__':
    unittest.main()
