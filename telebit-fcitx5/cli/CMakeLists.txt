# The doctor CLI deliberately links nothing from fcitx5. It inspects the system
# from the outside — busctl, the filesystem, /proc — so staying free of
# Fcitx5::Core means it keeps building and keeps running in exactly the broken
# states it exists to explain.
#
# The diagnostics live in a library rather than in each executable's source
# list, because there are three consumers now (the CLI, the tests, and the
# telebit-setup window) and the compile definitions below are not optional:
# a target that forgets TELEBIT_ADDON_DIR still builds and still runs, it just
# quietly stops finding the addon. Attached here, no consumer can forget it.
add_library(telebit_doctor_core STATIC
    doctor.cpp
    probe.cpp
    textfmt.cpp
    verdict.cpp
)
target_compile_features(telebit_doctor_core PUBLIC cxx_std_17)
target_include_directories(telebit_doctor_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# TELEBIT_ADDON_DIR is the directory src/CMakeLists.txt actually installs the
# addon into. Handing it over makes the search authoritative instead of guessing
# from a prefix list: on Fedora/openSUSE that path is /usr/lib64/fcitx5, and
# /usr/lib64 is a *sibling* of /usr/lib, so no amount of scanning /usr/lib finds it.
# PRIVATE because only probe.cpp reads it, and probe.cpp is compiled here.
target_compile_definitions(telebit_doctor_core PRIVATE
    TELEBIT_ADDON_DIR="${FCITX_INSTALL_LIBDIR}/fcitx5")

add_executable(telebit_cli main.cpp)
set_target_properties(telebit_cli PROPERTIES OUTPUT_NAME "telebit")
target_link_libraries(telebit_cli PRIVATE telebit_doctor_core)

# Same version string the addon reports, so `telebit --version` and the addon's
# Version field can never disagree.
target_compile_definitions(telebit_cli PRIVATE
    TELEBIT_VERSION="${_telebit_pkg_ver}")

include(GNUInstallDirs)
install(TARGETS telebit_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Tests cover what can be asserted deterministically: textfmt, verdict, and the
# compositor table in probe.cpp. probe.cpp is linked for that table alone — its
# probe_* entry points read the live system and are never called from here.
add_executable(telebit_doctor_tests tests.cpp)
target_link_libraries(telebit_doctor_tests PRIVATE telebit_doctor_core)

# No install() rule for this target on purpose — it is for ctest and install.sh,
# and shipping it would put a test binary in the user's /usr/bin.
add_test(NAME doctor COMMAND telebit_doctor_tests)
