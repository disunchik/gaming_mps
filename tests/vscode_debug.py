"""Exercise the installed VS Code C/C++ debug adapter against the owned QEMU."""
from pathlib import Path
import json
import os
import queue
import subprocess
import threading
import time
ROOT = Path(__file__).resolve().parent.parent
if os.name != 'nt':
    print('SKIP: VS Code adapter verification requires Windows')
    raise SystemExit(0)
adapter = next((Path(os.environ['USERPROFILE']) / '.vscode/extensions').glob('ms-vscode.cpptools-*/debugAdapters/bin/OpenDebugAD7.exe'))
config = json.loads((ROOT / '.vscode/launch.json').read_text(encoding='utf-8-sig'))['configurations'][0]
config = json.loads(json.dumps(config).replace('${workspaceFolder}', ROOT.as_posix()))
config.pop('preLaunchTask', None); config.pop('postDebugTask', None)
errors = (ROOT / 'artifacts/vscode-adapter-stderr.log').open('wb')
proc = subprocess.Popen([str(adapter)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors)
messages = queue.Queue(); sequence = 0; history = []; skipped = []
def reader():
    try:
        while True:
            headers = {}
            while True:
                line = proc.stdout.readline()
                if not line: return
                if line == b'\r\n': break
                key, val = line.decode().split(':', 1); headers[key.lower()] = val.strip()
            message = json.loads(proc.stdout.read(int(headers['content-length'])))
            history.append(message); messages.put(message)
    finally: messages.put({'event':'adapter-exited'})
threading.Thread(target=reader, daemon=True).start()
def send(command, arguments=None):
    global sequence
    sequence += 1
    message = {'seq': sequence, 'type': 'request', 'command': command, 'arguments': arguments or {}}
    raw = json.dumps(message).encode()
    proc.stdin.write(f'Content-Length: {len(raw)}\r\n\r\n'.encode() + raw); proc.stdin.flush()
    return sequence

def wait(predicate, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for index, message in enumerate(skipped):
            if predicate(message): return skipped.pop(index)
        message = messages.get(timeout=max(.1, deadline-time.monotonic()))
        if message.get('event') == 'adapter-exited': raise AssertionError('Adapter exited')
        if message.get('type') == 'response' and not message.get('success', False): raise AssertionError(message)
        if predicate(message): return message
        skipped.append(message)
    raise AssertionError('DAP timeout')
def request(command, arguments=None):
    number = send(command, arguments)
    return wait(lambda m: m.get('request_seq') == number)

def stopped():
    event = wait(lambda m: m.get('event') == 'stopped')
    thread = event['body']['threadId']
    stack = request('stackTrace', {'threadId':thread})['body']['stackFrames']
    frame = stack[0]
    assert frame['source']['path'].replace('\\','/').lower().endswith('/main/main.cpp'), frame
    return thread, frame
try:
    request('initialize', {'clientID':'vscode','adapterID':'cppdbg','pathFormat':'path',
        'linesStartAt1':True,'columnsStartAt1':True,'supportsRunInTerminalRequest':False})
    launch = send('launch', config)
    wait(lambda m: m.get('event') == 'initialized')
    breaks = request('setBreakpoints', {'source':{'path':str(ROOT/'main/main.cpp')}, 'breakpoints':[{'line':37}]})
    request('configurationDone')
    thread, first = stopped()
    frames = [first]
    for _ in range(2):
        request('next', {'threadId':thread})
        thread, frame = stopped(); frames.append(frame)
    assert len({f.get('instructionPointerReference', f['line']) for f in frames}) == 3, frames
    request('continue', {'threadId':thread})
    thread, manual = stopped()
    assert manual['line'] == breaks['body']['breakpoints'][0]['line'], (manual,breaks)
    request('scopes', {'frameId':manual['id']})
    architecture = request('evaluate', {'expression':'-exec show architecture','context':'repl'})
    assert 'xtensa' in json.dumps(architecture).lower(), architecture
    request('disconnect', {'terminateDebuggee':False})
    lines = ['PASS: installed VS Code C/C++ debug adapter, generated F5 configuration.',
        'PASS: temporary app_main breakpoint, two next requests and three distinct locations.']
    lines += [str({'line':f['line'], 'pc':f.get('instructionPointerReference')}) for f in frames]
    lines += [f'PASS: user breakpoint resolved to main/main.cpp:{manual["line"]}, continue, stackTrace and scopes.',
              'PASS: connected GDB architecture is Xtensa.',
              'This is a DAP integration test, not a screenshot of the VS Code UI.']
    (ROOT/'artifacts/vscode-debug.txt').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    print('\n'.join(lines))
finally:
    (ROOT/'artifacts/vscode-adapter.json').write_text(json.dumps(history, ensure_ascii=False,indent=2), encoding='utf-8')
    if proc.poll() is None: proc.terminate()
    proc.wait(timeout=10); errors.close()
