#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

[[ "$(cat VERSION)" =~ ^[0-9]+\.[0-9]+\.[0-9]+([.-][A-Za-z0-9.-]+)?$ ]]
[[ -f src/metrology/CMakeLists.txt ]]
[[ -f tests/analysis/test_level_analyzer.cpp ]]
[[ -f tests/metrology/test_metrology.cpp ]]
[[ -f tools/CMakeLists.txt ]]
[[ -f cmake/StudioHubConfig.cmake.in ]]

python3 - <<'PY'
from pathlib import Path
root=Path('.')
src=(root/'src/CMakeLists.txt').read_text()
assert src.count('dsp/core/parameter_smoother.cpp') == 1
assert src.count('analysis/signal_generator.cpp') == 1
assert src.count('analysis/sweep_generator.cpp') == 1
assert src.count('analysis/test_signals.cpp') == 1
assert src.count('analysis/level_analyzer.cpp') == 1
assert 'add_subdirectory(metrology)' in src
assert 'CMakePackageConfigHelpers' in (root/'CMakeLists.txt').read_text()
doctor=(root/'scripts/doctor.sh').read_text()
assert 'ROOT=' in doctor
assert '$ROOT/VERSION' in doctor

tests=(root/'tests/CMakeLists.txt').read_text()
for f in ['analysis/test_level_analyzer.cpp','metrology/test_metrology.cpp']:
    assert f in tests
    assert (root/'tests'/f).is_file()

for p in root.rglob('*'):
    if not p.is_file() or p.suffix not in {'.cpp','.hpp','.h','.cmake'}:
        continue
    rel=p.relative_to(root)
    if rel.parts and rel.parts[0] in {'third_party', 'build', 'build-vst3'}:
        continue
    text=p.read_text(errors='ignore')
    assert 'TODO' not in text, str(rel)
    assert 'FIXME' not in text, str(rel)
print('StudioHub structural check PASS')
PY
