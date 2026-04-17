#!/usr/bin/env bash
# Regenerate STL files and 4-view preview renders for the Claude Buddy Pico case.
set -euo pipefail
cd "$(dirname "$0")"
python3 buddy_case.py
python3 render.py --all
echo "done. see ../stl/ and ../renders/"
