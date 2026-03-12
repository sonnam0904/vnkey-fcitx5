add_library(vnkey SHARED
    vnkey.cpp
    ../../engine.cpp
    ../../vietnamese.cpp
    ../../rime_table.cpp
    ../../canonicalize.cpp
    ../../render_utf8.cpp
)

target_include_directories(vnkey PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(vnkey PRIVATE Fcitx5::Core)

# Produce vnkey.so instead of libvnkey.so
set_target_properties(vnkey PROPERTIES PREFIX "")

install(TARGETS vnkey DESTINATION "${FCITX_INSTALL_LIBDIR}/fcitx5")

# Addon config file
configure_file(vnkey-addon.conf.in vnkey-addon.conf)
install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/vnkey-addon.conf"
    RENAME vnkey.conf
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon"
)

# Input Method registration file
install(
    FILES "vnkey.conf.in"
    RENAME "vnkey.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
)

