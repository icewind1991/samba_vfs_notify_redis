#!/usr/bin/env bash

mkdir -p build
cd build

if [ ! -d "samba" ]; then
    git clone https://github.com/samba-team/samba
fi

cd samba

git reset --hard HEAD
git checkout $1