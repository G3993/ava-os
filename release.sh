#!/bin/bash
# One-shot release: version bump → build Mac + Windows → sign + package →
# site downloads (permanent names) + page version → commit/push both repos →
# GitHub release. Usage: ./release.sh 1.2
set -euo pipefail
cd "$(dirname "$0")"
VER="${1:?usage: ./release.sh <version>}"
SITE="$HOME/soundtemple-nyc"
BUILD=$(( $(plutil -extract CFBundleVersion raw "build/AVA OS.app/Contents/Info.plist" 2>/dev/null || echo 1) + 1 ))
sed -i '' "s|<key>CFBundleShortVersionString</key><string>[^<]*</string>|<key>CFBundleShortVersionString</key><string>$VER</string>|; s|<key>CFBundleVersion</key><string>[^<]*</string>|<key>CFBundleVersion</key><string>$BUILD</string>|" Info.plist.in
echo "── build $VER ──"; cmake --build build -j8 | tail -1; cmake --build build-win -j8 | tail -1
echo "── package ──"; STAGE=$(mktemp -d); mkdir -p "$STAGE/s"; cp -R "build/AVA OS.app" "$STAGE/s/"
codesign --force --deep --sign - --identifier nyc.soundtemple.os --options runtime "$STAGE/s/AVA OS.app"
ln -s /Applications "$STAGE/s/Applications"; mkdir -p dist
hdiutil create -volname "AVA OS" -srcfolder "$STAGE/s" -ov -quiet -format UDZO -fs HFS+ "dist/AVA-OS-$VER-macOS-arm64.dmg"
./package-win.sh | tail -1; cp dist/AVA-OS-Windows.zip "dist/AVA-OS-$VER-Windows-x64.zip"
echo "── site ──"; cp "dist/AVA-OS-$VER-macOS-arm64.dmg" "$SITE/downloads/AVA-OS-macOS.dmg"; cp "dist/AVA-OS-$VER-Windows-x64.zip" "$SITE/downloads/AVA-OS-Windows.zip"
sed -i '' "s|· Version [0-9.]*|· Version $VER|" "$SITE/ava-os.html"
echo "── commit + push ──"
git add -A && git commit -qm "AVA OS $VER" && git push -q origin main
( cd "$SITE" && git add downloads ava-os.html && git commit -qm "AVA OS $VER downloads" && git push -q origin main )
gh release create "v$VER" "dist/AVA-OS-$VER-macOS-arm64.dmg" "dist/AVA-OS-$VER-Windows-x64.zip" --title "AVA OS $VER" --notes "Install notes: https://www.avartifacts.com/ava-os.html"
echo "── done: $VER live at https://www.avartifacts.com/ava-os.html ──"
