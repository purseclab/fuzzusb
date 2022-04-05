#!/bin/bash

CURDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SYZHOME=$(realpath $CURDIR/../)

export GOROOT=$HOME/go1.12  # EDIT
export PATH=$GOROOT/bin:$PATH

KERNEL_DIR=$CURDIR/kernel

