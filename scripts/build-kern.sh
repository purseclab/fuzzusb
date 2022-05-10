#!/bin/bash -e

. ./common.sh

KERNEL_TARGET=$KERNEL_DIR/target

pushd $KERNEL_TARGET >/dev/null
make -j`nproc` 
popd 

