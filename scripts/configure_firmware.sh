#!/usr/bin/env bash
set -euo pipefail

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is not installed. Run scripts/bootstrap_macos.sh first."
  exit 1
fi

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/build/pico"

export PICO_SDK_PATH="${PICO_SDK_PATH:-${repo_root}/third_party/pico-sdk}"
export PIMORONI_PICO_PATH="${PIMORONI_PICO_PATH:-${repo_root}/third_party/pimoroni-pico}"
local_toolchain_dir="${repo_root}/toolchains/gcc-arm-embedded"
cmake_args=(
  --fresh
  -S "${repo_root}"
  -B "${build_dir}"
  -G Ninja
  -DPICO_BOARD=pico2_w
  -DPICO_SDK_PATH="${PICO_SDK_PATH}"
  -DPIMORONI_PICO_PATH="${PIMORONI_PICO_PATH}"
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
)

mkdir -p "${build_dir}"

if [[ -x "${local_toolchain_dir}/bin/arm-none-eabi-gcc" ]]; then
  export PATH="${local_toolchain_dir}/bin:${PATH}"
  cmake_args+=(
    -DCMAKE_C_COMPILER="${local_toolchain_dir}/bin/arm-none-eabi-gcc"
    -DCMAKE_CXX_COMPILER="${local_toolchain_dir}/bin/arm-none-eabi-g++"
    -DCMAKE_ASM_COMPILER="${local_toolchain_dir}/bin/arm-none-eabi-gcc"
  )
fi

cmake "${cmake_args[@]}" "$@"
