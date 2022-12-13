find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

    find_path(WAYLAND_CLIENT_INCLUDE_DIR NAMES wayland-client.h HINTS ${WAYLAND_CLIENT_INCLUDE_DIRS})
    add_library(Wayland::client ALIAS PkgConfig::WAYLAND_CLIENT)

    set(Wayland_INCLUDE_DIR ${WAYLAND_CLIENT_INCLUDE_DIR})
    set(Wayland_LIBRARIES ${WAYLAND_CLIENT_LIBRARIES})
    find_package_handle_standard_args(Wayland DEFAULT_MSG Wayland_LIBRARIES Wayland_INCLUDE_DIR)
endif()
