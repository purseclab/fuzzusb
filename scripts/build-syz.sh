#!/bin/bash

CUR_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $CUR_DIR/./common.sh

pushd $CUR_DIR/../..
make extract TARGETOS=linux TARGETARCH=amd64 SOURCEDIR=$KERNEL_TARGET
make generate
make clean
make -j$(nproc) TARGETOS=linux TARGETARCH=amd64
popd
