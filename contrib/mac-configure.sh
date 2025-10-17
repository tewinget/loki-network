#!/bin/bash

set -e
set -x

if ! [ -f LICENSE ] || ! [ -d src ] || ! [ -d include/session/router.hpp ]; then
    echo "You need to run this as ./contrib/mac.sh from the top-level Session Router project directory" >&2
    exit 1
fi

mkdir -p build-mac
cd build-mac
cmake \
      -G Ninja \
      -DBUILD_STATIC_DEPS=ON \
      -DSROUTER_TESTS=OFF \
      -DSROUTER_BOOTSTRAP=OFF \
      -DSROUTER_NATIVE_BUILD=OFF \
      -DWITH_LTO=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DMACOS_SYSTEM_EXTENSION=ON \
      -DCODESIGN=ON \
      -DSROUTER_PACKAGE=ON \
      "$@" \
      ..

echo "cmake build configured in build-mac"
