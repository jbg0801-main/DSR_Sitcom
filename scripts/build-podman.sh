#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
podman run --rm -v "$ROOT":/src:Z -w /src docker.io/library/fedora:44 bash -lc '
  set -euo pipefail
  dnf install -y mingw64-gcc-c++ mingw64-winpthreads-static cmake make >/tmp/dnf.log
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-toolchain.cmake
  cmake --build build -j"$(nproc)"
  ls -la build/dinput8.dll build/sitcom/sounds | head
'
