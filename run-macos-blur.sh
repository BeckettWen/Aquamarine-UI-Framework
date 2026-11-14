#!/bin/sh
# Configure, build, and launch the macOS widget + system-window backdrop blur test.
# Requires an installed CMake and Xcode; this script installs nothing.

set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$PROJECT_DIR"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "run-macos-blur.sh only runs on macOS." >&2
    exit 1
fi

BUILD_DIR=${LYRA_MACOS_BLUR_BUILD_DIR:-build-macos-blur}

if ! command -v xcrun >/dev/null 2>&1; then
    echo "Xcode command-line tools (xcrun) are required." >&2
    exit 1
fi

cmake -G Xcode -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DLYRA_ENABLE_BACKDROP_BLUR=ON \
    -DLYRA_ENABLE_MACOS_WINDOW_BLUR=ON
cmake --build "$BUILD_DIR" --config Debug --target x19 -- -quiet

echo "Launching macOS backdrop test. Metal blurs Lyra widget backdrops; AppKit blurs the desktop behind the window."
echo "If system blur is supported, content behind the window should show through transparent areas."
exec "$BUILD_DIR/Debug/x19" "$@"
