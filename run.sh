#!/bin/sh
# Configure, build, and launch Lyra with the native renderer for this platform.
set -eu

PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$PROJECT_DIR"

if [ "$(uname -s)" = "Darwin" ]; then
    BUILD_DIR=${LYRA_BUILD_DIR:-build-metal}
    cmake -G Xcode -S "$PROJECT_DIR" -B "$BUILD_DIR" \
        -DLYRA_ENABLE_BACKDROP_BLUR=ON \
        -DLYRA_ENABLE_MACOS_WINDOW_BLUR=OFF
    cmake --build "$BUILD_DIR" --config Debug --target x19 -- -quiet
    exec "$BUILD_DIR/Debug/x19" "$@"
fi

exec ./build/x19 "$@"
