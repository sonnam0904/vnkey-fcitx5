# Desktop entry, AppStream metadata and the application icon — the three files
# that decide whether Telebit is a *package* or an *application* as far as a
# software centre is concerned.

# The release date is the commit date of the tree being built: on a release
# build that commit is the release, and on a from-source build the field is at
# least not a lie. No git (a released tarball) means no <releases> block at
# all, which validates clean — inventing a date would not.
set(TELEBIT_METAINFO_RELEASES "")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cd --date=short
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE _telebit_commit_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _telebit_commit_date_rc)
  # CMake's regex has no brace quantifiers, hence the spelled-out date shape.
  if(_telebit_commit_date_rc EQUAL 0 AND
     _telebit_commit_date MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
    set(TELEBIT_METAINFO_RELEASES
        "  <releases>\n    <release version=\"${TELEBIT_ADDON_VERSION}\" date=\"${_telebit_commit_date}\"/>\n  </releases>")
  endif()
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/io.github.sonnam0904.Telebit.metainfo.xml.in"
  "${CMAKE_CURRENT_BINARY_DIR}/io.github.sonnam0904.Telebit.metainfo.xml"
  @ONLY)

# Exec has to be the absolute path, not a bare `telebit-setup`. An
# `install.sh --user` install puts the binary in ~/.local/bin, which the
# graphical session's PATH (inherited from systemd, not from ~/.profile) does
# not normally contain — the launcher would be in the menu and do nothing.
include(GNUInstallDirs)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/io.github.sonnam0904.Telebit.desktop.in"
  "${CMAKE_CURRENT_BINARY_DIR}/io.github.sonnam0904.Telebit.desktop"
  @ONLY)

# The addon component describes the fcitx5 engine, which exists in every build.
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/io.github.sonnam0904.Telebit.Addon.metainfo.xml"
  DESTINATION share/metainfo)

# The desktop-application component and its entry only make sense when the
# window they launch was built. Shipping a .desktop whose Exec does not exist
# would put a broken launcher in the menu and a broken entry in the store.
if(TELEBIT_BUILD_GUI)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/io.github.sonnam0904.Telebit.metainfo.xml"
    DESTINATION share/metainfo)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/io.github.sonnam0904.Telebit.desktop"
    DESTINATION share/applications)
  # Same artwork as the fcitx5 input-method icon, installed a second time under
  # the AppStream ID. src/telebit-fcitx5.conf.in refers to the icon by the name
  # `telebit-fcitx5`, so that copy cannot simply be renamed.
  install(
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/../icons/telebit-fcitx5.svg"
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME "io.github.sonnam0904.Telebit.svg")
endif()
