# Host unit tests for the portable kernel code (no ESP-IDF needed).
#   make -C test/host run
KERNEL := ../../components/kernel
CC     ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -O1 -g \
          -fsanitize=address,undefined -I$(KERNEL)/include
SRCS   := test_main.c \
          $(KERNEL)/src/proto/k_proto.c \
          $(KERNEL)/src/geo/k_geo.c \
          $(KERNEL)/src/tiles/k_tpk.c

build/test_all: $(SRCS) $(wildcard $(KERNEL)/include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SRCS) -o $@ -lm

build/pack_check: pack_check.c $(KERNEL)/src/tiles/k_tpk.c $(wildcard $(KERNEL)/include/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) pack_check.c $(KERNEL)/src/tiles/k_tpk.c -o $@

# Cross-language check: tools/tilepack.py writes packs, the firmware parser reads them.
packs: build/pack_check
	python3 ../../tools/tilepack.py demo --out build/demo_maps --zmin 12 --zmax 14
	for f in build/demo_maps/*.tpk; do ./build/pack_check $$f || exit 1; done

run: build/test_all
	./build/test_all

.PHONY: packs

clean:
	rm -rf build

.PHONY: run clean
