#!/usr/bin/env python3
"""Check the peer-facing declarations and opcodes independently of the C++ wrapper API."""
from pathlib import Path
import re
import subprocess
import sys
repo = Path(__file__).resolve().parents[2]
baseline = sys.argv[1] if len(sys.argv) > 1 else '5eb17d37'
paths = ['include/aap/aapxs.h', 'include/aap/android-audio-plugin.h',
         'include/aap/ext/state.h', 'include/aap/ext/parameters.h']
paths += subprocess.check_output(['git', 'ls-files', '*.aidl'], cwd=repo, text=True).splitlines()
for path in paths:
    old = subprocess.check_output(['git', 'show', baseline + ':' + path], cwd=repo)
    if old != (repo / path).read_bytes():
        raise SystemExit('FAIL: peer-facing declaration changed: ' + path)
for name in ['state', 'parameters']:
    path = 'include/aap/core/aapxs/' + name + '-aapxs.h'
    old = subprocess.check_output(['git', 'show', baseline + ':' + path], cwd=repo, text=True)
    new = (repo / path).read_text()
    constants = lambda text: re.findall(r'^const int32_t .*?;', text, re.M)
    if constants(old) != constants(new):
        raise SystemExit('FAIL: opcode/shared-memory constants changed: ' + path)
    path = 'androidaudioplugin/src/main/cpp/core/aapxs/' + name + '-aapxs.cpp'
    old = subprocess.check_output(['git', 'show', baseline + ':' + path], cwd=repo, text=True)
    new = (repo / path).read_text()
    # Actual recipient handlers and reply routing are preserved, not just their declarations.
    for side in ['plugin', 'host']:
        for direction in ['request', 'reply']:
            marker = 'void aap::xs::AAPXSDefinition_' + name.title() + '::aapxs_' + name + '_process_incoming_' + side + '_aapxs_' + direction
            def body(text):
                start = text.index(marker)
                opening = text.index('{', start)
                level = 1
                end = opening + 1
                while level:
                    level += (text[end] == '{') - (text[end] == '}')
                    end += 1
                return text[start:end]
            if body(old) != body(new):
                raise SystemExit('FAIL: counterpart handler changed: ' + marker)
print('PASS: C extension/AAPXS declarations, AIDL, opcodes, capacities and recipient/reply handlers match ' + baseline)
