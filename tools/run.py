"""Build and run the ESP32 firmware with a native terminal/UART transport."""
from pathlib import Path
import argparse
import os
import shutil
import socket
import subprocess
import sys
import threading
import time
from terminal import Terminal
from clipboard import ClipboardBridge

ROOT = Path(__file__).resolve().parent.parent  # Project root, independent of cwd.


class Qemu:
    """Own one QEMU process and its UART socket, including deterministic cleanup."""
    def __init__(self, debug=False, port=5555):
        """Configure the optional GDB server and local UART port."""
        self.debug = debug
        self.port = port
        self.process = None
        self.serial = None
        self.log = None

    def __enter__(self):
        """Merge firmware, configure eFuses, start QEMU, then connect UART."""
        for port in ([self.port, 3333] if self.debug else [self.port]):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
                if hasattr(socket, 'SO_EXCLUSIVEADDRUSE'):
                    probe.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
                try:
                    probe.bind(('127.0.0.1', port))
                except OSError as error:
                    raise RuntimeError(f'Порт {port} занят. Закрой прежний QEMU перед запуском.') from error
        build = ROOT / 'build'
        if not (build / 'gaming_mps.elf').exists():
            raise RuntimeError('Сначала собери проект: idf.py build')
        idf = Path(os.environ['IDF_PATH'])
        sys.path.insert(0, str(idf / 'tools'))
        from idf_py_actions.qemu_ext import QEMU_TARGETS
        flash = build / 'qemu_flash.bin'
        subprocess.run([sys.executable, '-m', 'esptool', '--chip', 'esp32', 'merge_bin',
                        '--output', str(flash), '--fill-flash-size', '2MB', '@flash_args'],
                        cwd=build, check=True, stdout=subprocess.DEVNULL)
        efuse = build / 'qemu_efuse.bin'
        if not efuse.exists(): efuse.write_bytes(QEMU_TARGETS['esp32'].default_efuse)
        executable = shutil.which('qemu-system-xtensa')
        if not executable: raise RuntimeError('Не найден qemu-xtensa в активированной среде ESP-IDF.')
        args = [executable, '-M', 'esp32', '-m', '4M',
                '-drive', f'file={flash},if=mtd,format=raw',
                '-drive', f'file={efuse},if=none,format=raw,id=efuse',
                '-global', 'driver=nvram.esp32.efuse,property=drive,value=efuse',
                '-global', 'driver=timer.esp32.timg,property=wdt_disable,value=true',
                '-nic', 'user,model=open_eth', '-display', 'none', '-monitor', 'none',
                '-serial', f'tcp:127.0.0.1:{self.port},server=on,wait=on']
        if self.debug: args += ['-gdb', 'tcp:127.0.0.1:3333', '-S']
        artifacts = ROOT / 'artifacts'
        artifacts.mkdir(exist_ok=True)
        self.log = (artifacts / 'qemu-process.log').open('w', encoding='utf-8')
        try:
            self.process = subprocess.Popen(args, stdout=self.log, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    raise RuntimeError('QEMU завершился; смотри artifacts/qemu-process.log')
                try:
                    self.serial = socket.create_connection(('127.0.0.1', self.port), timeout=0.2)
                    self.serial.settimeout(0.2)
                    time.sleep(0.05)
                    if self.process.poll() is not None:
                        raise RuntimeError('Новый QEMU не запустился; подключение закрыто.')
                    return self
                except OSError:
                    time.sleep(0.05)
            raise RuntimeError(f'QEMU не открыл UART на порту {self.port}')
        except BaseException:
            self.__exit__()
            raise

    def __exit__(self, *_):
        """Close UART, terminate only this owned process, and close its log."""
        if self.serial:
            self.serial.close()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        if self.log:
            self.log.close()


class UartStream:
    """Adapt pyserial to the same recv/sendall interface as the QEMU UART socket."""
    def __init__(self, port):
        self.port = port

    def recv(self, count):
        data = self.port.read(min(count, max(1, self.port.in_waiting)))
        if not data: raise socket.timeout()
        return data

    def sendall(self, data):
        pending = memoryview(data)
        while pending:
            written = self.port.write(pending)
            if not written: raise OSError('UART не принимает данные')
            pending = pending[written:]


class SerialPort:
    """Open an ESP32 USB-UART without flashing or toggling boot/reset lines."""
    def __init__(self, port, baud=115200):
        self.name, self.baud = port, baud
        self.port = None

    def __enter__(self):
        import serial
        self.port = serial.serial_for_url(self.name, baudrate=self.baud,
            timeout=0.2, write_timeout=2, do_not_open=True)
        self.port.dtr = self.port.rts = False
        try:
            self.port.open()
            self.serial = UartStream(self.port)
            return self
        except BaseException:
            self.port.close()
            raise

    def __exit__(self, *_):
        if self.port: self.port.close()


def main():
    """Run the native input/output bridge; F10 or Ctrl+] exits."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--debug', action='store_true')
    parser.add_argument('--port', help='COM-порт настоящего ESP32, например COM5')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()
    if args.port and args.debug: parser.error('--debug доступен для QEMU')
    if not args.no_build and not args.port:
        subprocess.run([sys.executable, str(Path(os.environ['IDF_PATH']) / 'tools/idf.py'),
                        '-DIDF_TARGET=esp32', 'build'], cwd=ROOT, check=True)
    print(f'{args.port or "QEMU"}: F10 или Ctrl+] — выйти.')
    if args.debug: print('Ожидание GDB: во втором терминале idf.py gdb')
    try:
        with Terminal() as terminal, (SerialPort(args.port, args.baud) if args.port else Qemu(args.debug)) as qemu:
            qemu.serial.sendall(b'\x1b[930~')
            bridge = ClipboardBridge()
            send_lock = threading.Lock()
            alive = threading.Event()
            alive.set()
            def receive():
                """Forward emulated UART output verbatim to the real terminal."""
                while alive.is_set():
                    try:
                        data = qemu.serial.recv(8192)
                        if not data: break
                        output, reply = bridge.feed(data)
                        terminal.write(output)
                        if reply:
                            with send_lock: qemu.serial.sendall(reply)
                    except socket.timeout: continue
                    except OSError: break
                alive.clear()
            thread = threading.Thread(target=receive, daemon=True)
            thread.start()
            try:
                while alive.is_set():
                    data = terminal.read()
                    if b'\x1d' in data or b'\x03' in data: break
                    if data:
                        with send_lock: qemu.serial.sendall(data)
            finally:
                alive.clear()
                thread.join(timeout=1)
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'Ошибка запуска: {error}', file=sys.stderr)
        sys.exit(1)
