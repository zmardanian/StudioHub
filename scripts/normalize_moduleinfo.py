#!/usr/bin/env python3
"""Convert Steinberg moduleinfotool's JSONC output to strict JSON in place."""
from __future__ import annotations

import json
import os
import sys
import tempfile
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} MODULEINFO_JSON", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    source = path.read_text(encoding="utf-8")
    # The SDK 3.8.1 creator emits JSONC with trailing commas. Remove them while
    # respecting quoted strings, then use json.loads as the strict validity gate.
    normalized_chars: list[str] = []
    in_string = False
    escaped = False
    for index, char in enumerate(source):
        if in_string:
            normalized_chars.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            normalized_chars.append(char)
            continue
        if char == ",":
            following = index + 1
            while following < len(source) and source[following].isspace():
                following += 1
            if following < len(source) and source[following] in "}]":
                continue
        normalized_chars.append(char)
    normalized = "".join(normalized_chars)
    data = json.loads(normalized)
    if not isinstance(data, dict) or not isinstance(data.get("Classes"), list):
        raise ValueError("moduleinfo must be an object containing a Classes array")
    rendered = json.dumps(data, ensure_ascii=False, indent=2) + "\n"
    fd, temporary = tempfile.mkstemp(prefix=path.name + ".", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            stream.write(rendered)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print(f"Strict JSON metadata valid: {path}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        print(f"moduleinfo validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
