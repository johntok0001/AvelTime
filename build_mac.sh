#!/bin/bash
# Builds Avel Time (VST3 + AU) and installs it for the current user.
# Needs only the Xcode Command Line Tools: CMake and JUCE are downloaded automatically.
set -e
cd "$(dirname "$0")"

if ! command -v cmake >/dev/null 2>&1; then
    CM=cmake-3.30.5-macos-universal
    if [ ! -d ".tools/$CM" ]; then
        echo "Downloading CMake..."
        mkdir -p .tools
        curl -sL "https://github.com/Kitware/CMake/releases/download/v3.30.5/$CM.tar.gz" | tar xz -C .tools
    fi
    export PATH="$PWD/.tools/$CM/CMake.app/Contents/bin:$PATH"
fi

cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --config Release --target AvelTime_VST3 AvelTime_AU AvelTime_Standalone -j"$(sysctl -n hw.ncpu)"

ART="build-mac/AvelTime_artefacts/Release"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"Avel Time.vst3" ~/Library/Audio/Plug-Ins/Components/"Avel Time.component"
cp -R "$ART/VST3/Avel Time.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "$ART/AU/Avel Time.component" ~/Library/Audio/Plug-Ins/Components/
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/"Avel Time.vst3" ~/Library/Audio/Plug-Ins/Components/"Avel Time.component" || true
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo ""
echo "Avel Time installed:"
echo "  ~/Library/Audio/Plug-Ins/VST3/Avel Time.vst3"
echo "  ~/Library/Audio/Plug-Ins/Components/Avel Time.component"
echo "Standalone app: $ART/Standalone/Avel Time.app"
