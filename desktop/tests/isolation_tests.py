#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory() as folder:
    root = Path(folder)
    bundle = root / 'Fixture.vst3'
    module = bundle / 'Contents/x86_64-linux/Fixture.so'
    module.parent.mkdir(parents=True)
    module.write_bytes(b'not a real plugin; isolation fixture only')
    tool = root / 'probe'
    tool.write_text('''#!/bin/sh
set -eu
test "$HOME" = /home/test
test "$VST3_PATH" = /empty-plugins
test ! -d /var/home
test ! -e /host-home
test -f /plugin/Fixture.vst3/Contents/x86_64-linux/Fixture.so
printf 'Filesystem and plugin-path isolation verified\\n'
''')
    tool.chmod(0o755)
    output = subprocess.run([sys.executable, sys.argv[1], '--kind', 'metadata', '--tool', str(tool),
        '--bundle', str(bundle), '--evidence', str(root/'evidence')], text=True, capture_output=True)
    assert output.returncode == 0, output.stdout + output.stderr
    assert 'Filesystem and plugin-path isolation verified' in output.stdout
    provenance = json.loads(output.stdout.splitlines()[0])
    assert len(provenance['tool_sha256']) == 64
    print(output.stdout)

    # A similarly named test must not falsely count as the Editor test.
    tool.write_text("#!/bin/sh\nprintf 'Completed tests in pluginval / Editor Automation\\nCompleted tests in pluginval / Open editor whilst processing\\n'\n")
    partial = subprocess.run([sys.executable, sys.argv[1], '--kind', 'gui', '--tool', str(tool),
        '--bundle', str(bundle), '--evidence', str(root/'evidence')], text=True, capture_output=True)
    assert partial.returncode == 124, partial.stdout + partial.stderr
    assert 'without all required editor cases' in partial.stderr
    tool.write_text("#!/bin/sh\nprintf 'Completed tests in pluginval / Editor\\nCompleted tests in pluginval / Editor Automation\\nCompleted tests in pluginval / Open editor whilst processing\\n'\n")
    complete = subprocess.run([sys.executable, sys.argv[1], '--kind', 'gui', '--tool', str(tool),
        '--bundle', str(bundle), '--evidence', str(root/'evidence')], text=True, capture_output=True)
    assert complete.returncode == 0, complete.stdout + complete.stderr
    print('Exact GUI-case marker acceptance and rejection verified')
