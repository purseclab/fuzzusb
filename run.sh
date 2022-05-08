#!/bin/bash

source ./scripts/common.sh
rm -rf ./workdir/*

../bin/syz-manager -config cfg/main.cfg 

