# SPDX-FileCopyrightText: 2022 Alexandre Bouvier <contact@amb.tf>
# SPDX-License-Identifier: GPL-3.0-or-later

find_path(DiscordRPC_INCLUDE_DIR discord_rpc.h)

find_library(DiscordRPC_LIBRARY discord-rpc)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DiscordRPC
    REQUIRED_VARS
        DiscordRPC_LIBRARY
        DiscordRPC_INCLUDE_DIR
)

if (DiscordRPC_FOUND AND NOT TARGET DiscordRPC::discord-rpc)
    add_library(DiscordRPC::discord-rpc UNKNOWN IMPORTED)
    set_target_properties(DiscordRPC::discord-rpc PROPERTIES
        IMPORTED_LOCATION "${DiscordRPC_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${DiscordRPC_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(
    DiscordRPC_INCLUDE_DIR
    DiscordRPC_LIBRARY
)
