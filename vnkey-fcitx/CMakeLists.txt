cmake_minimum_required(VERSION 3.21)
project(vnkey-fcitx VERSION 0.1.0)

find_package(Fcitx5Core REQUIRED)

include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")

add_subdirectory(src)

if(DEFINED ENV{VNKEY_PACKAGE_VERSION} AND NOT "$ENV{VNKEY_PACKAGE_VERSION}" STREQUAL "")
  set(_vnkey_pkg_ver "$ENV{VNKEY_PACKAGE_VERSION}")
elseif(DEFINED ENV{VNKEY_DEB_PACKAGE_VERSION} AND NOT "$ENV{VNKEY_DEB_PACKAGE_VERSION}" STREQUAL "")
  set(_vnkey_pkg_ver "$ENV{VNKEY_DEB_PACKAGE_VERSION}")
else()
  set(_vnkey_pkg_ver "${PROJECT_VERSION}")
endif()

set(CPACK_GENERATOR "DEB" "RPM")
set(CPACK_PACKAGE_NAME "vnkey-fcitx5")
set(CPACK_PACKAGE_VENDOR "vnkey")
set(CPACK_PACKAGE_VERSION "${_vnkey_pkg_ver}")
set(CPACK_PACKAGE_CONTACT "https://github.com/sonnam0904/vnkey-fcitx5")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vietnamese Telex input method for fcitx5")

# DEB settings
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "vnkey-fcitx5 <https://github.com/sonnam0904/vnkey-fcitx5>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Vietnamese Telex engine and fcitx5 addon (vnkey). After install, add vnkey in fcitx5-configtool and restart fcitx5.")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "fcitx5, libfcitx5core5 | libfcitx5core7 | libfcitx5core8 | libfcitx5core9 | libfcitx5core10")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# RPM settings
set(CPACK_RPM_PACKAGE_SUMMARY "Vietnamese Telex input method for fcitx5")
set(CPACK_RPM_PACKAGE_NAME "vnkey-fcitx5")
set(CPACK_RPM_PACKAGE_VERSION "${_vnkey_pkg_ver}")
set(CPACK_RPM_PACKAGE_LICENSE "GPLv3+")
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES "fcitx5")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

include(CPack)

