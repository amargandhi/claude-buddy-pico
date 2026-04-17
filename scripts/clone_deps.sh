#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
third_party_dir="${repo_root}/third_party"

mkdir -p "${third_party_dir}"

clone_if_missing() {
  local url="$1"
  local dest="$2"

  if [[ -d "${dest}/.git" ]]; then
    echo "Skipping existing clone: ${dest}"
    return
  fi

  echo "Cloning ${url} -> ${dest}"
  git clone --depth=1 "${url}" "${dest}"
  git -C "${dest}" submodule update --init --recursive
}

clone_if_missing "https://github.com/raspberrypi/pico-sdk.git" "${third_party_dir}/pico-sdk"
clone_if_missing "https://github.com/raspberrypi/pico-examples.git" "${third_party_dir}/pico-examples"
clone_if_missing "https://github.com/pimoroni/pimoroni-pico.git" "${third_party_dir}/pimoroni-pico"
clone_if_missing "https://github.com/pimoroni/pimoroni-pico-rp2350.git" "${third_party_dir}/pimoroni-pico-rp2350"
