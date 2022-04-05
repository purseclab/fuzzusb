#!/bin/bash

source ./common.sh
rm -rf ./workdir/*

../bin/syz-manager -config cfg/main.cfg 

