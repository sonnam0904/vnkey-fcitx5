#!/usr/bin/env bash
# Build vnkey-fcitx5 .rpm via CPack.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/vnkey-fcitx/build-rpm"

if [[ -z "${VNKEY_PACKAGE_VERSION:-}" ]]; then
  if ver="$(git -C "${ROOT}" describe --tags --long --always 2>/dev/null)"; then
    export VNKEY_PACKAGE_VERSION="${ver#v}"
  else
    export VNKEY_PACKAGE_VERSION="0.1.0+git$(git -C "${ROOT}" rev-parse --short HEAD)"
  fi
fi

# Modern RPM version strings might dislike hyphens or pluses
export VNKEY_PACKAGE_VERSION="${VNKEY_PACKAGE_VERSION//-/\~}"

cmake -S "${ROOT}/vnkey-fcitx" -B "${BUILD_DIR}" \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"
cpack -G RPM --config "${BUILD_DIR}/CPackConfig.cmake" -B "${BUILD_DIR}"

echo "Built:"
find "${BUILD_DIR}" -maxdepth 1 -name '*.rpm' -print
