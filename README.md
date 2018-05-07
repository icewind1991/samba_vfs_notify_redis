# vfs-notify-redis

vfs plugin to push update notifications to redis 

# Requirements

The vfs plugin requires [`hiredis`](https://github.com/redis/hiredis) installed for building and usage.

# Building

Currently vfs modules can only be build inside the samba source tree, for convenience
this repo includes a set of scripts to get the samba sources, add the vfs plugin and build everything.

```bash
SAMBA_VERSION=samba-4.8.1 JOBS=8 make
``` 

`SAMBA_VERSION` needs to be set to a valid branch or tag in the [samba sources](http://github.com/samba-team/samba/).

The compiled vfs plugin will be placed at `build/bin/$SAMBA_VERSION/notify_redis.so`

## Installation

To install the vfs plugin simply copy the `notify_redis.so` library for the correct samba version into the samba vfs module directory (`/usr/lib/samba/vfs/`).

# Usage

Enable the vfs module for the desired shares by adding the following to `smb.conf`

## Enabling

```
vfs objects = notify_redis
```

## Configuring

The redis connection details can be configured by settings the `notify_redis:hostname` and `notify_redis:port` options in `smb.conf`.
The list that the events will be pushed to can be configured using `notify_redis:list` (defaults to `notify`).

## Output

Modify events will be pushed into the redis list in the following format

- `write|$path`
- `remame|$from|$to`
- `remove|$path`