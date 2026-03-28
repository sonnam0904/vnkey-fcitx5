#!/usr/bin/env bash
# Build two release .debs for GitHub Releases: jammy (22.04 libc/libstdc++) and noble (24.04).
# Run on ubuntu-22.04 CI with Docker available. Usage: build-release-debs.sh <semver>
set -euo pipefail

VERSION="${1:?semver required (e.g. 1.2.3)}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${ROOT}/release-debs"
mkdir -p "${OUT}"
rm -f "${OUT}"/*.deb

build_native_jammy() {
  export VNKEY_DEB_PACKAGE_VERSION="${VERSION}+jammy"
  unset VNKEY_DEB_PACKAGE_SUFFIX || true
  rm -rf "${ROOT}/vnkey-fcitx/build-deb"
  bash "${ROOT}/scripts/build-deb.sh"
  shopt -s nullglob
  for f in "${ROOT}/vnkey-fcitx/build-deb"/*.deb; do
    mv "$f" "${OUT}/"
  done
  shopt -u nullglob
}

build_native_jammy

if ! command -v docker >/dev/null 2>&1; then
  echo "ERROR: docker required to build noble .deb" >&2
  exit 1
fi

rm -rf "${ROOT}/vnkey-fcitx/build-deb"
docker run --rm \
  -v "${ROOT}:/src" \
  -w /src \
  -e "VNKEY_DEB_PACKAGE_VERSION=${VERSION}+noble" \
  ubuntu:24.04 \
  bash -lc '
    set -euo pipefail
    apt-get update -qq
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      git ca-certificates build-essential cmake pkg-config libfcitx5core-dev
    bash scripts/build-deb.sh
  '

shopt -s nullglob
for f in "${ROOT}/vnkey-fcitx/build-deb"/*.deb; do
  mv "$f" "${OUT}/"
done
shopt -u nullglob

echo "Release .deb artifacts:"
ls -la "${OUT}"
