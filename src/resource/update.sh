#!/bin/sh

APP=$(cd "$(dirname "0")"/;pwd)
hdiutil attach $TMPDIR/cemu_update/cemu.dmg
cp -rf /Volumes/Cemu/Cemu.app "$APP"
hdiutil detach /Volumes/Cemu/

open -n -a "$APP/Cemu.app"
