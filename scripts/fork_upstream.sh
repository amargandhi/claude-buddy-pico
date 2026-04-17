#!/usr/bin/env bash
set -euo pipefail

upstream_repo="anthropics/claude-desktop-buddy"
upstream_url="https://github.com/${upstream_repo}.git"

if ! command -v gh >/dev/null 2>&1; then
  echo "GitHub CLI is required. Install it first."
  exit 1
fi

gh auth status -h github.com
gh repo fork "${upstream_repo}" --clone=false

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  if ! git remote get-url upstream >/dev/null 2>&1; then
    git remote add upstream "${upstream_url}"
    echo "Added git remote: upstream -> ${upstream_url}"
  fi
fi

echo "Fork request completed. If this repo is not connected to your fork yet, add your origin remote next."
