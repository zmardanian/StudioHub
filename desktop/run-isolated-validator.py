#!/usr/bin/env python3
"""Run one exact VST3 bundle with no user-home mounts and no host PID namespace."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--kind', choices=['gui', 'non-gui', 'steinberg', 'metadata'], required=True)
p.add_argument('--tool', type=Path, required=True)
p.add_argument('--bundle', type=Path, required=True)
p.add_argument('--evidence', type=Path, required=True)
a = p.parse_args()
try:
    tool, bundle, evidence = a.tool.resolve(strict=True), a.bundle.resolve(strict=True), a.evidence.resolve()
    if not tool.is_file() or not os.access(tool, os.X_OK) or not bundle.is_dir() or bundle.suffix != '.vst3':
        raise ValueError('Invalid exact validator executable or bundle')
    modules = list((bundle / 'Contents/x86_64-linux').glob('*.so'))
    if len(modules) != 1:
        raise ValueError('Expected exactly one native module')
    bwrap = shutil.which('bwrap')
    if not bwrap:
        raise ValueError('Bubblewrap is unavailable; isolated validation is BLOCKED')
    evidence.mkdir(parents=True, exist_ok=True)
    provenance = {'kind': a.kind, 'tool': str(tool), 'tool_sha256': hashlib.sha256(tool.read_bytes()).hexdigest(),
                  'module_sha256': hashlib.sha256(modules[0].read_bytes()).hexdigest(),
                  'isolation': 'bubblewrap: private PID/network/mount namespaces; no normal home mounted',
                  'display': os.environ.get('DISPLAY'), 'gdk_backend': 'x11'}
    print(json.dumps(provenance), flush=True)
    sandbox_bundle = '/plugin/' + bundle.name
    cmd = [bwrap, '--die-with-parent', '--unshare-all', '--ro-bind', '/usr', '/usr',
           '--symlink', 'usr/bin', '/bin', '--symlink', 'usr/lib', '/lib', '--symlink', 'usr/lib64', '/lib64',
           '--ro-bind', '/etc', '/etc', '--dev', '/dev', '--proc', '/proc', '--tmpfs', '/tmp',
           '--dir', '/home/test/config', '--dir', '/home/test/data', '--dir', '/home/test/cache',
           '--dir', '/empty-plugins', '--dir', '/tool', '--dir', '/plugin',
           '--ro-bind', str(tool), '/tool/validator', '--ro-bind', str(bundle), sandbox_bundle,
           '--bind', str(evidence), '/evidence', '--chdir', '/home/test', '--clearenv']
    environment = {'PATH': '/usr/bin:/bin', 'HOME': '/home/test', 'XDG_CONFIG_HOME': '/home/test/config',
                   'XDG_DATA_HOME': '/home/test/data', 'XDG_CACHE_HOME': '/home/test/cache',
                   'GDK_BACKEND': 'x11', 'QT_QPA_PLATFORM': 'xcb', 'LANG': 'C.UTF-8'}
    for key in ('VST_PATH', 'VST3_PATH', 'LV2_PATH', 'CLAP_PATH', 'DSSI_PATH', 'LADSPA_PATH'):
        environment[key] = '/empty-plugins'
    if a.kind in ('gui', 'non-gui'):
        display = os.environ.get('DISPLAY')
        if not display:
            raise ValueError('GUI validation requires DISPLAY')
        environment['DISPLAY'] = display
        cmd += ['--ro-bind', '/tmp/.X11-unix', '/tmp/.X11-unix']
        authority = os.environ.get('XAUTHORITY')
        if authority:
            authority_path = Path(authority).resolve(strict=True)
            cmd += ['--ro-bind', str(authority_path), '/xauthority']
            environment['XAUTHORITY'] = '/xauthority'
    for key, value in environment.items():
        cmd += ['--setenv', key, value]
    if a.kind in ('gui', 'non-gui'):
        arguments = ['--validate', sandbox_bundle, '--strictness-level', '5', '--output-dir', '/evidence']
        if a.kind == 'non-gui':
            arguments += ['--skip-gui-tests']
    elif a.kind == 'steinberg':
        arguments = [sandbox_bundle]
    else:
        arguments = ['-validate', '-path', sandbox_bundle]
    child = subprocess.Popen(cmd + ['/tool/validator'] + arguments, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors='replace')
    observed = set()
    for line in child.stdout:
        print(line, end='', flush=True)
        for test in ('Editor', 'Open editor whilst processing', 'Editor Automation'):
            if line.strip() == ('Completed tests in pluginval / ' + test):
                observed.add(test)
    code = child.wait()
    if a.kind == 'gui':
        print(json.dumps({'gui_cases_completed': sorted(observed)}), flush=True)
        if code == 0 and len(observed) != 3:
            print('FAIL: full-GUI validator returned success without all required editor cases', file=sys.stderr)
            sys.exit(124)
    sys.exit(code if code >= 0 else 128 - code)
except (OSError, ValueError) as e:
    print('BLOCKED: ' + str(e), file=sys.stderr)
    sys.exit(125)
