SAMBA_VERSION?=samba-4.8.1
JOBS?=32
BUILD_DIRECTORY=build/samba/$(SAMBA_VERSION)

all: build/bin/$(SAMBA_VERSION)/notify_redis.so

build/samba/$(SAMBA_VERSION): get_samba_sources.sh configure.sh
	./get_samba_sources.sh $(SAMBA_VERSION)
	./configure.sh $(SAMBA_VERSION) --jobs $(JOBS)
	mkdir -p build/bin/$(SAMBA_VERSION)

build/bin/$(SAMBA_VERSION)/notify_redis.so: $(BUILD_DIRECTORY) src/vfs_notify_redis.c
	mkdir -p build/bin/$(SAMBA_VERSION)
	./build.sh $(SAMBA_VERSION) --jobs $(JOBS)
	cp $(BUILD_DIRECTORY)/bin/modules/vfs/notify_redis.so build/bin/$(SAMBA_VERSION)



