#!/usr/bin/env bash

SAMBA_VERSION=$1
shift
cd build/samba/$SAMBA_VERSION

cp ../../../src/vfs_notify_redis.c source3/modules

python2 ./buildtools/bin/waf build $@