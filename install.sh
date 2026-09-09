#!/usr/bin/env bash

set -euo pipefail

MODE="system"
if [[ "${1:-}" == "--system" ]]; then
  MODE="system"
elif [[ "${1:-}" == "--user" ]]; then
  MODE="user"
elif [[ "${1:-}" != "" ]]; then
  echo "Usage: $0 [--user|--system]"
  echo "  --system Install Telebit (fcitx5) into /usr (requires sudo) (default)"
  echo "  --user   Install Telebit (fcitx5) into \$HOME/.local"
  exit 1
fi

if [[ "$MODE" == "system" ]]; then
  PREFIX="/usr"
else
  PREFIX="${HOME}/.local"
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "==> Building core library and tests (C++17, CMake) ..."
cmake -B "${ROOT_DIR}/build" "${ROOT_DIR}"
cmake --build "${ROOT_DIR}/build"

echo "==> Running C++ tests ..."
"${ROOT_DIR}/build/telebit_telex_tests"

# telebit-setup (the settings/status window) needs GTK4 headers. Packaging
# builds always have them and keep the option ON, because a package without
# the window also has no desktop entry and disappears from every software
# centre. A from-source install on a machine without GTK4 should still get a
# working input method, so here it degrades instead of failing.
#
# The flag is passed on every run, never left to the cache: this build dir is
# reused across --user and --system, and a stale ON would fail the next build
# on a machine that has since lost the headers.
GUI_FLAG="-DTELEBIT_BUILD_GUI=ON"
if ! pkg-config --atleast-version=4.6 gtk4 2>/dev/null; then
  GUI_FLAG="-DTELEBIT_BUILD_GUI=OFF"
  echo "==> GTK4 >= 4.6 headers not found; skipping the telebit-setup window."
  echo "    Install libgtk-4-dev (Debian/Ubuntu) or gtk4-devel (Fedora) to get it."
fi

echo "==> Building fcitx5 addon 'telebit-fcitx5' (mode: ${MODE}, prefix: ${PREFIX}) ..."
cd "${ROOT_DIR}/telebit-fcitx5"
cmake -B build -DCMAKE_INSTALL_PREFIX="${PREFIX}" "${GUI_FLAG}" .
cmake --build build

# The doctor CLI ships in the same package, so its suite runs here rather than
# with the engine tests above — a broken verdict layer must not reach an install.
echo "==> Running doctor CLI tests ..."
./build/cli/telebit_doctor_tests

echo "==> Installing addon 'telebit-fcitx5' into ${PREFIX} ..."
if [[ "$MODE" == "system" ]]; then
  sudo cmake --install build
else
  cmake --install build
fi

ICON_HICOLOR="${PREFIX}/share/icons/hicolor"
if [[ -d "${ICON_HICOLOR}" ]] && command -v gtk-update-icon-cache >/dev/null 2>&1; then
  echo "==> Refreshing GTK icon cache (hicolor) ..."
  if [[ "$MODE" == "system" ]]; then
    sudo gtk-update-icon-cache -f "${ICON_HICOLOR}" 2>/dev/null || true
  else
    gtk-update-icon-cache -f "${ICON_HICOLOR}" 2>/dev/null || true
  fi
fi
# Qt often caches theme icons; stale IM menu art is usually fixed after this + fcitx5 -r
if [[ "$MODE" != "system" ]]; then
  rm -f "${HOME}/.cache/icon-cache.kcache" 2>/dev/null || true
fi

# Without this the launcher exists on disk but does not show up in the
# application menu until something else happens to refresh the database.
APPS_DIR="${PREFIX}/share/applications"
if [[ -d "${APPS_DIR}" ]] && command -v update-desktop-database >/dev/null 2>&1; then
  echo "==> Refreshing the desktop entry database ..."
  if [[ "$MODE" == "system" ]]; then
    sudo update-desktop-database "${APPS_DIR}" 2>/dev/null || true
  else
    update-desktop-database "${APPS_DIR}" 2>/dev/null || true
  fi
fi

if [[ "$MODE" == "system" ]]; then
  ENVD_FILE="${PREFIX}/lib/environment.d/60-telebit-fcitx5.conf"
else
  ENVD_FILE="${HOME}/.config/environment.d/60-telebit-fcitx5.conf"
fi

echo
echo "Done."
if [[ "${GUI_FLAG}" == "-DTELEBIT_BUILD_GUI=ON" ]]; then
  echo "- Open the Telebit window to switch it on and check it: telebit-setup"
  echo "  (or find 'Telebit' in the application menu)"
else
  echo "- Add input method 'telebit-fcitx5' in fcitx5-configtool (Input Method -> Add -> Telebit / telebit-fcitx5)."
fi
echo "- Then restart fcitx5: fcitx5 -r"
echo
echo "- Check the whole input-method path, sandboxed apps included: telebit doctor"
echo
echo "Installed ${ENVD_FILE}: it points GTK_IM_MODULE / QT_IM_MODULE / XMODIFIERS at"
echo "fcitx5 for the whole graphical session, Flatpak apps included."
echo "- Log out and back in for it to take effect (systemd reads environment.d at session start)."
echo "- To opt out: sudo ln -s /dev/null /etc/environment.d/60-telebit-fcitx5.conf"

