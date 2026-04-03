#!/usr/bin/env bash
# Build telebit-fcitx5 .rpm via CPack.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/telebit-fcitx5/build-rpm"

if [[ -z "${TELEBIT_PACKAGE_VERSION:-}" ]]; then
  if ver="$(git -C "${ROOT}" describe --tags --long --always 2>/dev/null)"; then
    export TELEBIT_PACKAGE_VERSION="${ver#v}"
  else
    export TELEBIT_PACKAGE_VERSION="0.1.0+git$(git -C "${ROOT}" rev-parse --short HEAD)"
  fi
fi

# Modern RPM version strings refuse hyphens and pluses
TELEBIT_PACKAGE_VERSION="${TELEBIT_PACKAGE_VERSION//-/\~}"
export TELEBIT_PACKAGE_VERSION="${TELEBIT_PACKAGE_VERSION//+/\~}"

if [[ -n "${TELEBIT_RPM_PACKAGE_SUFFIX:-}" ]]; then
  export TELEBIT_PACKAGE_VERSION="${TELEBIT_PACKAGE_VERSION}~${TELEBIT_RPM_PACKAGE_SUFFIX}"
fi

cmake -S "${ROOT}/telebit-fcitx5" -B "${BUILD_DIR}" \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"
cpack -G RPM --config "${BUILD_DIR}/CPackConfig.cmake" -B "${BUILD_DIR}"

echo "Built:"
find "${BUILD_DIR}" -maxdepth 1 -name '*.rpm' -print
