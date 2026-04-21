#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
toolchain_root="${repo_root}/toolchains/gcc-arm-embedded"
pkg_path="${1:-}"

if [[ -z "${pkg_path}" ]]; then
  pkg_path="$(find "${HOME}/Library/Caches/Homebrew/Cask" -name 'arm-gnu-toolchain-*-arm-none-eabi.pkg--*.pkg' -print -quit 2>/dev/null || true)"
fi

if [[ -z "${pkg_path}" || ! -f "${pkg_path}" ]]; then
  echo "Arm GNU toolchain package not found in the Homebrew cache."
  echo "Run 'brew fetch --cask gcc-arm-embedded' first, or pass the package path explicitly."
  exit 1
fi

temp_dir="$(mktemp -d)"
trap 'rm -rf "${temp_dir}"' EXIT

pkgutil --expand-full "${pkg_path}" "${temp_dir}/expanded"
mkdir -p "${toolchain_root}"
rsync -a --delete "${temp_dir}/expanded/Payload/" "${toolchain_root}/"

echo "Extracted toolchain to ${toolchain_root}"
