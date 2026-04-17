#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <fork-repo-url> [target-dir]"
  echo "example: $0 https://github.com/<user>/claude-desktop-buddy-pico.git ../claude-desktop-buddy-pico"
  exit 1
fi

fork_url="$1"
target_dir="${2:-../claude-desktop-buddy-pico}"
upstream_url="https://github.com/anthropics/claude-desktop-buddy.git"
source_root="$(cd "$(dirname "$0")/.." && pwd)"

if [[ -e "$target_dir/.git" ]]; then
  echo "target already looks like a git repo: $target_dir"
else
  git clone "$fork_url" "$target_dir"
fi

git -C "$target_dir" remote get-url upstream >/dev/null 2>&1 || git -C "$target_dir" remote add upstream "$upstream_url"

rsync -a --delete \
  --exclude ".git/" \
  --exclude "build/" \
  --exclude "third_party/" \
  --exclude "toolchains/" \
  --exclude ".DS_Store" \
  "$source_root/" "$target_dir/"

echo "synced working tree into: $target_dir"
echo "origin  -> $(git -C "$target_dir" remote get-url origin)"
echo "upstream -> $(git -C "$target_dir" remote get-url upstream)"
