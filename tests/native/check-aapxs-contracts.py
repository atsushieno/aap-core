#!/usr/bin/env python3
"""Compare SDK class declarations and native layouts against a Git revision."""
import argparse
import io
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tarfile
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('baseline', help='Git revision before the changes')
parser.add_argument('--compile-commands', type=Path, help='Use an existing NDK compile_commands.json for a target ABI')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[2]
compiler = os.environ.get('CXX', 'clang++')
compiler_flags = ['-std=c++17']
if args.compile_commands:
    commands = json.loads(args.compile_commands.read_text())
    entry = next(e for e in commands if e['file'].endswith('/typed-aapxs.cpp'))
    command = entry.get('arguments') or shlex.split(entry['command'])
    compiler, compiler_flags = command[0], []
    skip = False
    for flag in command[1:]:
        if skip:
            skip = False
        elif flag in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
        elif flag not in ('-c', '-MD', '-MMD', entry['file']):
            compiler_flags.append(flag)
classes = ('aap::xs::TypedAAPXS', 'aap::AAPXSMidi2InitiatorSession')

def records(text):
    decoder = json.JSONDecoder()
    while text.strip():
        text = text.lstrip()
        record, end = decoder.raw_decode(text)
        yield record
        text = text[end:]

def parameters(node):
    return [(p.get('name'), p['type']['qualType'], 'init' in p)
            for p in node.get('inner', []) if p['kind'] == 'ParmVarDecl']

def declarations(record):
    # Only class declarations are inspected; implementation bodies and local types are skipped.
    result = []
    access = 'private' if record.get('tagUsed') == 'class' else 'public'
    for node in record.get('inner', []):
        kind = node['kind']
        if kind == 'AccessSpecDecl':
            access = node['access']
        elif node.get('isImplicit'):
            continue
        elif kind == 'FieldDecl':
            result.append((access, kind, node.get('name'), node['type']['qualType']))
        elif kind in ('CXXConstructorDecl', 'CXXDestructorDecl', 'CXXMethodDecl'):
            if any(child['kind'] == 'TemplateArgument' for child in node.get('inner', [])):
                continue
            result.append((access, kind, node.get('name'), node['type']['qualType'],
                           node.get('storageClass'), node.get('virtual', False), parameters(node)))
        elif kind == 'FunctionTemplateDecl':
            primary = next(n for n in node['inner'] if n['kind'] == 'CXXMethodDecl')
            template = [(n['kind'], n.get('name'), n.get('tagUsed')) for n in node['inner']
                        if n['kind'] in ('TemplateTypeParmDecl', 'NonTypeTemplateParmDecl')]
            result.append((access, kind, node.get('name'), template, primary['type']['qualType'], parameters(primary)))
        elif kind == 'CXXRecordDecl' and node.get('completeDefinition'):
            result.append((access, kind, node.get('name'), declarations(node)))
        elif kind in ('TypeAliasDecl', 'TypedefDecl'):
            result.append((access, kind, node.get('name'), node['type']['qualType']))
    return result

with tempfile.TemporaryDirectory(prefix='aapxs-contracts-') as temporary:
    build = Path(temporary)
    archive = subprocess.check_output(['git', 'archive', args.baseline, 'include'], cwd=repo)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(build / 'baseline')
    compat = build / 'compat.h'
    compat.write_text('''#include <atomic>
#include <ctime>
#ifdef __APPLE__
inline int clock_nanosleep(clockid_t, int, const timespec* delay, timespec* remaining) {
    return nanosleep(delay, remaining);
}
#endif
''')
    probe = build / 'probe.cpp'
    probe.write_text('''#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/core/AAPXSMidi2InitiatorSession.h"
int layouts[] = {sizeof(aap::xs::TypedAAPXS), alignof(aap::xs::TypedAAPXS),
                 sizeof(aap::AAPXSMidi2InitiatorSession), alignof(aap::AAPXSMidi2InitiatorSession)};
''')
    def inspect(include, label):
        flags = [compiler, '-I', str(include), *compiler_flags, '-include', str(compat)]
        api = {}
        for name in classes:
            text = subprocess.check_output(flags + ['-Xclang', '-ast-dump=json', '-Xclang',
                '-ast-dump-filter=' + name, '-fsyntax-only', str(probe)], text=True)
            cls = next(n for n in records(text) if n['kind'] == 'CXXRecordDecl'
                       and n.get('name') == name.split('::')[-1] and n.get('completeDefinition'))
            api[name] = declarations(cls)
        text = subprocess.check_output(flags + ['-Xclang', '-fdump-record-layouts', '-c', str(probe),
                                               '-o', str(build / (label + '.o'))], text=True)
        layouts = {}
        for name in classes:
            block = next(b for b in text.split('*** Dumping AST Record Layout')
                         if re.search(r'^\s*0 \| class ' + re.escape(name) + r'\s*$', b, re.M))
            layouts[name] = block.strip()
        return api, layouts
    before, before_layout = inspect(build / 'baseline/include', 'baseline')
    after, after_layout = inspect(repo / 'include', 'current')
    if before != after:
        raise SystemExit('FAIL: SDK class declarations changed')
    if before_layout != after_layout:
        raise SystemExit('FAIL: native class layouts changed')
    print('PASS: public/protected/private declarations, template signatures, virtual methods, '
          'default-argument presence, field offsets, sizes and alignment match ' + args.baseline)
