# Host simulator: compiles the REAL UI sources against a fake kernel.
# Run from the project root:   make -C sim run
ROOT := ..
UI   := $(ROOT)/components/ui
KRN  := $(ROOT)/components/kernel
CC   ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Wshadow -O1 -g -fsanitize=address,undefined \
          -I$(ROOT)/sim/stubs -I$(ROOT)/sim -I$(KRN)/include -I$(UI)/include

# ui_task.c is replaced by the stack in sim_main.c (it needs FreeRTOS).
UI_SRCS := $(filter-out $(UI)/src/core/ui_task.c,$(shell find $(UI)/src -name '*.c'))
SRCS    := sim_main.c sim_kernel.c $(KRN)/src/geo/k_geo.c $(UI_SRCS)

build/sim: $(SRCS)
	@mkdir -p build out
	$(CC) $(CFLAGS) $(SRCS) -o $@ -lm

# The sim writes into sim/out relative to the project root.
run: build/sim
	@mkdir -p $(ROOT)/sim/out
	cd $(ROOT) && ./sim/build/sim

clean:
	rm -rf build out

.PHONY: run clean
