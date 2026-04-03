#!/usr/bin/env bash
# Build telebit-fcitx5 .deb via CPack. Set TELEBIT_DEB_PACKAGE_VERSION for Debian package version
# (e.g. semver from semantic-release or 1.2.1+gitabcdef for CI artifacts).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/telebit-fcitx5/build-deb"

if [[ -z "${TELEBIT_DEB_PACKAGE_VERSION:-}" ]]; then
  if ver="$(git -C "${ROOT}" describe --tags --long --always 2>/dev/null)"; then
    export TELEBIT_DEB_PACKAGE_VERSION="${ver#v}"
  else
    export TELEBIT_DEB_PACKAGE_VERSION="0.1.0+git$(git -C "${ROOT}" rev-parse --short HEAD)"
  fi
fi

# Optional: CI matrix tag (e.g. jammy / noble) appended for unique .deb names.
if [[ -n "${TELEBIT_DEB_PACKAGE_SUFFIX:-}" ]]; then
  export TELEBIT_DEB_PACKAGE_VERSION="${TELEBIT_DEB_PACKAGE_VERSION}+${TELEBIT_DEB_PACKAGE_SUFFIX}"
fi

cmake -S "${ROOT}/telebit-fcitx5" -B "${BUILD_DIR}" \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"
cpack --config "${BUILD_DIR}/CPackConfig.cmake" -B "${BUILD_DIR}"

echo "Built:"
find "${BUILD_DIR}" -maxdepth 1 -name '*.deb' -print
