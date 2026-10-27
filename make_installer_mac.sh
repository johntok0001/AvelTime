#!/bin/bash
# Builds a macOS .pkg installer (VST3 + AU) into dist/. Run build_mac.sh first.
set -e
cd "$(dirname "$0")"
VERSION=$(grep -m1 "project(AvelTime VERSION" CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
ART="build-mac/AvelTime_artefacts/Release"
[ -d "$ART/VST3/Avel Time.vst3" ] || { echo "Run build_mac.sh first"; exit 1; }

rm -rf dist/pkgroot && mkdir -p dist/pkgroot/vst3 dist/pkgroot/au
cp -R "$ART/VST3/Avel Time.vst3" dist/pkgroot/vst3/
cp -R "$ART/AU/Avel Time.component" dist/pkgroot/au/
codesign --force --deep -s - dist/pkgroot/vst3/"Avel Time.vst3" dist/pkgroot/au/"Avel Time.component" || true

pkgbuild --root dist/pkgroot/vst3 --identifier com.avel.time.vst3 --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/VST3" dist/vst3.pkg
pkgbuild --root dist/pkgroot/au --identifier com.avel.time.au --version "$VERSION" --install-location "/Library/Audio/Plug-Ins/Components" dist/au.pkg

cat > dist/distribution.xml <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Avel Time $VERSION</title>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
    </choices-outline>
    <choice id="vst3" title="VST3"><pkg-ref id="com.avel.time.vst3"/></choice>
    <choice id="au" title="Audio Unit"><pkg-ref id="com.avel.time.au"/></choice>
    <pkg-ref id="com.avel.time.vst3">vst3.pkg</pkg-ref>
    <pkg-ref id="com.avel.time.au">au.pkg</pkg-ref>
</installer-gui-script>
XML
productbuild --distribution dist/distribution.xml --package-path dist "dist/AvelTime-$VERSION-macOS.pkg"
rm -rf dist/pkgroot dist/vst3.pkg dist/au.pkg dist/distribution.xml
echo "Installer: dist/AvelTime-$VERSION-macOS.pkg"
