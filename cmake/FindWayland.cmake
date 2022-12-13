find_package(PkgConfig)

pkg_search_module(WAYLAND_CLIENT IMPORTED_TARGET wayland-client)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Wayland
    REQUIRED_VARS
        WAYLAND_CLIENT_LIBRARIES
    VERSION_VAR WAYLAND_CLIENT_VERSION
)

if (Wayland_FOUND)
    add_library(Wayland::client ALIAS PkgConfig::WAYLAND_CLIENT)
endif()
