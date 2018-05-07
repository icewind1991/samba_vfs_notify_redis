#!/usr/bin/env bash

cd build/samba
git reset --hard HEAD
git clean -fd

git apply ../../add_vfs_module.patch

cp ../../vfs_notify_redis.c souirce3/modules

PYTHON=python2 ./configure --with-hiredis /usr/include/hiredis/ --jobs 32