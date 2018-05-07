#!/usr/bin/env bash

mkdir -p build/samba
cd build/samba

if [ ! -d "$1" ]; then
    git clone -b $1 https://github.com/samba-team/samba --depth 1 $1
fi