find_package(PkgConfig)

if(PKG_CONFIG_FOUND)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)

    if(WAYLAND_CLIENT_FOUND)
        find_path(WAYLAND_CLIENT_INCLUDE_DIR NAMES wayland-client.h HINTS ${WAYLAND_CLIENT_INCLUDE_DIRS})
        add_library(Wayland::client ALIAS PkgConfig::WAYLAND_CLIENT)
    endif()

    set(WAYLAND_INCLUDE_DIR ${WAYLAND_CLIENT_INCLUDE_DIR})
    set(WAYLAND_LIBRARIES ${WAYLAND_CLIENT_LIBRARIES})
    find_package_handle_standard_args(Wayland DEFAULT_MSG WAYLAND_LIBRARIES WAYLAND_INCLUDE_DIR)
endif()
