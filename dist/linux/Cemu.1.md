% CEMU(1) Cemu | Wii U emulator
% Exzap; Petergov
% 2022-12-09

# NAME

Cemu - Wii U emulator

# SYNOPSIS

**Cemu** [_OPTIONS_...]

# DESCRIPTION

**Cemu** is a Wii U emulator that is able to run most Wii U games and homebrew in a playable state. It's written in C/C++ and is being actively developed with new features and fixes to increase compatibility, convenience and usability.

# OPTIONS

If an option has argument _n_, a value of 1 (or no argument) enables the option. A value of 0 disables the option.

## Launch options

**-g** _path_, **--game** _path_

: Path of game to launch

**-m** _path_, **--mlc** _path_

: Custom mlc folder location

**-f** [_n_], **--fullscreen** [_n_]

: Launch games in fullscreen mode

**-u** [_n_], **--ud** [_n_]

: Render output upside-down

**-a** _id_, **--account** _id_

: Persistent id of account

**--force-interpreter** [_n_]

: Force interpreter CPU emulation and disable recompiler

**--act-url** _url_

: URL prefix for account server

**--ecs-url** _url_

: URL for ECS service

**-h**, **--help**

: Display a usage message and exit

**-v**, **--version**

: Display Cemu version and exit

## Extractor tool

**-e** _path_, **--extract** _path_

: Path to WUD or WUX file for extraction

**-p** _path_, **--path** _path_

: Path of file to extract (for example meta/meta.xml)

**-o** _path_, **--output** _path_

: Output path for extracted file

# BUGS

To report a bug, visit _https://github.com/cemu-project/Cemu/issues_

# SEE ALSO

Project homepage:

_https://github.com/cemu-project/Cemu_
