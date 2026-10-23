#!/bin/bash
# build-deb.sh: the black-linux .deb from a finished host build (cmake --build <build> --target stage).
# Run on Ubuntu 24.04 / Linux Mint 22 (or a chroot of it) so the libraries match the player's system.
#   ./build-deb.sh [build dir] [out dir]
set -euo pipefail
cd "$(dirname "$0")"
REPO=$PWD
BUILD="${1:-/work/bl-build}"
OUTDIR="${2:-$REPO/release}"
VERSION="$(cat VERSION)"
MARCH="x86-64-v2"
STAGE="$BUILD/stage"
[[ -f $STAGE/lib/libblack_host.a ]] || { echo "no host build in $STAGE: build the 'stage' target first"; exit 1; }

PKG="$(mktemp -d)/black-linux_${VERSION}_amd64"
mkdir -p "$PKG"/DEBIAN "$PKG"/usr/bin "$PKG"/usr/lib/black-linux "$PKG"/usr/share/black-linux/tools \
         "$PKG"/usr/share/applications "$PKG"/usr/share/icons/hicolor/scalable/apps "$PKG"/usr/share/doc/black-linux

# launchers
for f in black-linux black-linux-build; do
    sed -e "s|@VERSION@|$VERSION|g" -e "s|@MARCH@|$MARCH|g" "packaging/$f" > "$PKG/usr/bin/$f"
    chmod 755 "$PKG/usr/bin/$f"
done

# the recompiler and the host's static libraries (no game code in any of them)
install -m 755 -s "$STAGE/xbe_recomp" "$STAGE/xbe_symbols" "$PKG/usr/lib/black-linux/"
install -m 644 "$STAGE/lib/libblack_host.a" "$STAGE/lib/libSDL3.a" "$STAGE/lib/libglslang.a" \
               "$STAGE/lib/libglslang-default-resource-limits.a" "$STAGE/image_symbols.h" "$PKG/usr/lib/black-linux/"
printf '%s\n' libSDL3.a libglslang.a libglslang-default-resource-limits.a > "$PKG/usr/lib/black-linux/link-libs.txt"

# what the recompiler and the compile of the player's generated code read
D="$PKG/usr/share/black-linux"
install -D -m 644 config/game.toml "$D/config/game.toml"
(cd sdk && find runtime harness/compat -name '*.h' -exec install -D -m 644 {} "$D/sdk/{}" \;)
cp -r sdk/xdk "$D/sdk/xdk"
install -m 644 tools/xdvdfs.py tools/europe_to_usa.py "$D/tools/"
install -m 644 src/common/europe.patch "$D/europe.patch"
install -D -m 644 data/skins/buttons.png "$D/skins/buttons.png"

# desktop integration
install -m 644 packaging/black-linux.desktop "$PKG/usr/share/applications/"
install -m 644 packaging/black-linux.svg "$PKG/usr/share/icons/hicolor/scalable/apps/"

# docs and licences
install -m 644 packaging/README.Debian "$PKG/usr/share/doc/black-linux/README"
{
    echo "black-linux: native Linux build of Black for Android's host (Rangga Casper) and the XboxXBE-Recomp SDK (TGP482)."
    echo "This package contains NO game code or game assets. Black is (c) Criterion Games / Electronic Arts."
    echo
    echo "== Black for Android (src/, desktop/ derived from native/) =="; cat LICENSE.black-android; echo
    echo "== XboxXBE-Recomp SDK (sdk/) =="; cat sdk/LICENSE; echo
    echo "== Third party (toml++, Dear ImGui, SDL3, glslang, Zydis) =="; cat sdk/THIRD_PARTY.txt
    echo "SDL3: zlib licence, (c) Sam Lantinga. glslang: BSD-3-Clause/Apache-2.0, (c) Khronos Group. Zydis: MIT, (c) zyantific."
} > "$PKG/usr/share/doc/black-linux/copyright"

SIZE=$(du -sk "$PKG/usr" | cut -f1)
cat > "$PKG/DEBIAN/control" <<EOF
Package: black-linux
Version: $VERSION
Architecture: amd64
Maintainer: black-linux packager <noreply@example.invalid>
Installed-Size: $SIZE
Depends: libc6 (>= 2.38), libstdc++6, libgcc-s1, libvulkan1, mesa-vulkan-drivers | vulkan-icd, clang, lld, libc6-dev, libstdc++-13-dev | libstdc++-dev, python3, zenity
Recommends: vulkan-tools
Section: games
Priority: optional
Homepage: https://github.com/RanggaCasper/Black-Android
Description: Black (Xbox, 2006) as a native Linux game - bring your own copy
 An unofficial native Linux (x86_64) port of the original Xbox version of
 Black, made by static recompilation (XboxXBE-Recomp): no emulator, no
 Wine, no Electron. Vulkan rendering, SDL3 window, audio, keyboard, mouse
 and controllers.
 .
 The package contains no game code or assets. On the first start it
 recompiles default.xbe from YOUR OWN copy of the game (USA or Europe .iso
 or extracted files) into ~/.local/share/black-linux/bin/BLACK; this takes
 30-90 minutes once on a laptop.
EOF
cat > "$PKG/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor || true
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
exit 0
EOF
chmod 755 "$PKG/DEBIAN/postinst"
cat > "$PKG/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if [ "$1" = purge ]; then
    echo "black-linux: your settings, saves and the built game stay in ~/.local/share/black-linux (delete it by hand)."
fi
exit 0
EOF
chmod 755 "$PKG/DEBIAN/postrm"

mkdir -p "$OUTDIR"
find "$PKG" -type d -exec chmod 755 {} \;
fakeroot dpkg-deb --build -Zxz "$PKG" "$OUTDIR/black-linux_${VERSION}_amd64.deb"
rm -rf "$(dirname "$PKG")"
echo "built $OUTDIR/black-linux_${VERSION}_amd64.deb"
