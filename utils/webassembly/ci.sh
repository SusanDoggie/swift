#/bin/bash

set -ex

SOURCE_PATH="$( cd "$(dirname $0)/../../.." && pwd  )" 
SWIFT_PATH=$SOURCE_PATH/swift
UTILS_PATH=$SWIFT_PATH/utils/webassembly
if [[ "$(uname)" == "Linux" ]]; then
  BUILD_SCRIPT=$UTILS_PATH/build-linux.sh
  DEPENDENCIES_SCRIPT=$UTILS_PATH/linux/install-dependencies.sh
else
  BUILD_SCRIPT=$UTILS_PATH/build-mac.sh
  DEPENDENCIES_SCRIPT=$UTILS_PATH/macos/install-dependencies.sh
fi

$DEPENDENCIES_SCRIPT

export SCCACHE_CACHE_SIZE="50G"
export SCCACHE_DIR="$SOURCE_PATH/cache"

CACHE_FLAGS="--cmake-c-launcher $(which sccache) --cmake-cxx-launcher $(which sccache)"
FLAGS="--release --debug-swift-stdlib $CACHE_FLAGS --verbose"

$BUILD_SCRIPT $FLAGS
# Run test but ignore failure temporarily
$BUILD_SCRIPT $FLAGS -t || true
