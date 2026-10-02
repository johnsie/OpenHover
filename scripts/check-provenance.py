#!/usr/bin/env python3
"""Fails unless every file in the repository is recorded in PROVENANCE.md.

    scripts/check-provenance.py

The ledger lists each file as a table row that begins with `| `path` |`. A file that
is not listed, or a listed file that does not exist, is an error. Run it before every
commit; see docs/clean-room-policy.md.
"""
import re, subprocess, sys

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout.splitlines()

present = set(git("ls-files", "--cached", "--others", "--exclude-standard"))
listed = set()
for line in open("PROVENANCE.md", encoding="utf-8"):
    m = re.match(r"\|\s*`([^`]+)`\s*\|", line)
    if m:
        listed.add(m.group(1))

missing = sorted(present - listed)
stale = sorted(listed - present)
for path in missing:
    print(f"NOT IN LEDGER: {path}")
for path in stale:
    print(f"IN LEDGER BUT MISSING: {path}")
if missing or stale:
    sys.exit(1)
print(f"provenance ok: {len(present)} files recorded")
