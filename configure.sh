#!/usr/bin/env bash

cd build/samba/$1
git reset --hard HEAD
git clean -fd

git apply ../../../add_vfs_module.patch

PYTHON=python2 ./configure --with-hiredis /usr/include/hiredis $@