#!/usr/bin/env bash
set -euo pipefail

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required. Install it first from https://brew.sh/."
  exit 1
fi

packages=(
  cmake
  ninja
  picotool
)

missing=()
for package in "${packages[@]}"; do
  if brew list --formula "${package}" >/dev/null 2>&1; then
    echo "Already installed: ${package}"
  else
    missing+=("${package}")
  fi
done

if ((${#missing[@]} == 0)); then
  echo "All required packages are already installed."
  exit 0
fi

echo "Installing: ${missing[*]}"
brew install "${missing[@]}"

if ! find /opt/homebrew -name nosys.specs -print -quit 2>/dev/null | grep -q . && \
   ! find /Applications/ArmGNUToolchain -name nosys.specs -print -quit 2>/dev/null | grep -q .; then
  echo "Installing the full Arm GNU bare-metal toolchain cask for nosys.specs support."
  echo "Homebrew may prompt for your macOS password during this step."
  brew install --cask gcc-arm-embedded
fi
