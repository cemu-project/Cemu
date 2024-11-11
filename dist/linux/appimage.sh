#!/bin/bash

if [[ -z "${GITHUB_WORKSPACE}" ]]; then
	export GITHUB_WORKSPACE="."
fi

curl -sSfLO "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
chmod a+x linuxdeploy*.AppImage
curl -sSfL https://github.com"$(curl https://github.com/probonopd/go-appimage/releases/expanded_assets/continuous | grep "mkappimage-.*-x86_64.AppImage" | head -n 1 | cut -d '"' -f 2)" -o mkappimage.AppImage
chmod a+x mkappimage.AppImage
curl -sSfLO "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
chmod a+x linuxdeploy-plugin-gtk.sh
curl -sSfLO "https://github.com/darealshinji/linuxdeploy-plugin-checkrt/releases/download/continuous/linuxdeploy-plugin-checkrt.sh"
chmod a+x linuxdeploy-plugin-checkrt.sh

if [[ ! -e /usr/lib/x86_64-linux-gnu ]]; then
	sed -i 's#lib\/x86_64-linux-gnu#lib64#g' linuxdeploy-plugin-gtk.sh
fi

mkdir -p AppDir/usr/bin
mkdir -p AppDir/usr/share/Cemu
mkdir -p AppDir/usr/share/applications
mkdir -p AppDir/usr/share/icons/hicolor/128x128/apps
mkdir -p AppDir/usr/share/metainfo
mkdir -p AppDir/usr/lib

cp dist/linux/info.cemu.Cemu.{desktop,png} AppDir/
cp dist/linux/info.cemu.Cemu.metainfo.xml AppDir/usr/share/metainfo/info.cemu.Cemu.appdata.xml

cp -r bin/* AppDir/usr/share/Cemu

mv AppDir/usr/share/Cemu/Cemu AppDir/usr/bin/
chmod +x AppDir/usr/bin/Cemu

cp /usr/lib/x86_64-linux-gnu/{libsepol.so.1,libffi.so.7,libpcre.so.3,libGLU.so.1,libthai.so.0} AppDir/usr/lib

export UPD_INFO="gh-releases-zsync|cemu-project|Cemu|ci|Cemu.AppImage.zsync"
export NO_STRIP=1
./linuxdeploy-x86_64.AppImage --appimage-extract-and-run \
  --appdir="${GITHUB_WORKSPACE}"/AppDir/ \
  -d "${GITHUB_WORKSPACE}"/AppDir/info.cemu.Cemu.desktop \
  -i "${GITHUB_WORKSPACE}"/AppDir/info.cemu.Cemu.png \
  -e "${GITHUB_WORKSPACE}"/AppDir/usr/bin/Cemu \
  --plugin gtk \
  --plugin checkrt

if ! GITVERSION="$(git rev-parse --short HEAD 2>/dev/null)"; then
	GITVERSION=experimental
fi
echo "Cemu Version Cemu-${GITVERSION}"

rm AppDir/usr/lib/libwayland-client.so.0
echo -e "export LC_ALL=C\nexport FONTCONFIG_PATH=/etc/fonts" >> AppDir/apprun-hooks/linuxdeploy-plugin-gtk.sh
VERSION="${GITVERSION}" ./mkappimage.AppImage --appimage-extract-and-run "${GITHUB_WORKSPACE}"/AppDir

mkdir -p "${GITHUB_WORKSPACE}"/artifacts/
mv Cemu-"${GITVERSION}"-x86_64.AppImage "${GITHUB_WORKSPACE}"/artifacts/
