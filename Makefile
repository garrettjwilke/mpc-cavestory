# MPC Cave Story (doukutsu-rs) Runtime & Player for Akai MPC Sample.
# `make` cross-builds bin/mpc-cavestory (aarch64-linux-musl static binary).
# `make test` builds and runs the host unit + integration test suite.
# `make deploy` uploads bin/mpc-cavestory to the MPC hardware via SSH / mpc-boot-thingy.

WARN = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
HOSTCC = cc
HOSTCXX = c++
ZIGCC = zig cc -target aarch64-linux-musl
ZIGCXX = zig c++ -target aarch64-linux-musl

UI = mpc-ui-toolkit/src
BUILD = build
BIN = bin

CARGO_TARGET_DIR = $(CURDIR)/$(BUILD)/cargo
HOST_RS_LIB = $(CARGO_TARGET_DIR)/release/libdoukutsu_rs.a
DEV_RS_LIB  = $(CARGO_TARGET_DIR)/aarch64-unknown-linux-musl/release/libdoukutsu_rs.a

INCLUDES = -I src -I $(UI)

HOST_CFLAGS = $(WARN) -O2 -g $(INCLUDES)
DEV_CFLAGS  = $(WARN) -O2 $(INCLUDES)

SRC_APP = src/input_map.c src/audio_engine.c src/ui_overlay.c src/config.c src/cJSON.c
UI_COMMON_SRC = $(UI)/ui.c $(UI)/widgets.c $(UI)/filebrowser.c $(UI)/input.c
UI_DEVICE_SRC = $(UI)/app.c $(UI)/screen.c $(UI)/surface.c $(UI_COMMON_SRC)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  HOST_LDFLAGS = -lc++ -framework CoreFoundation -framework Foundation -lobjc -lm -pthread
else
  HOST_LDFLAGS = -lstdc++ -lm -pthread -ldl
endif

.PHONY: all test doukutsu-host doukutsu-device deploy clean

all: bin/mpc-cavestory

# -----------------------------------------------------------------------------
# Host doukutsu-rs & Unit / Integration Tests
# -----------------------------------------------------------------------------
doukutsu-host:
	cargo build --manifest-path doukutsu-rs/Cargo.toml \
	  --target-dir $(CARGO_TARGET_DIR) --release \
	  --no-default-features --features backend-mpc --lib

$(BUILD)/host/test-runner: doukutsu-host $(SRC_APP) $(UI_COMMON_SRC) test/test_runner.c | $(BUILD)/host
	$(HOSTCC) $(HOST_CFLAGS) -o $@ \
	  test/test_runner.c $(SRC_APP) $(UI_COMMON_SRC) \
	  $(HOST_RS_LIB) $(HOST_LDFLAGS)

test: $(BUILD)/host/test-runner
	./$(BUILD)/host/test-runner

# -----------------------------------------------------------------------------
# Device Cross-Compilation (aarch64-linux-musl)
# -----------------------------------------------------------------------------
$(BUILD)/device/zig-shims:
	mkdir -p $(BUILD)/device
	printf '#!/bin/sh\nfor a in "$$@"; do case "$$a" in --target=*) ;; *) set -- "$$@" "$$a" ;; esac; shift; done\nexec zig cc -target aarch64-linux-musl "$$@"\n' > $(BUILD)/device/zig-cc
	printf '#!/bin/sh\nfor a in "$$@"; do case "$$a" in --target=*) ;; *) set -- "$$@" "$$a" ;; esac; shift; done\nexec zig c++ -target aarch64-linux-musl "$$@"\n' > $(BUILD)/device/zig-cxx
	printf '#!/bin/sh\nexec zig ar "$$@"\n' > $(BUILD)/device/zig-ar
	chmod +x $(BUILD)/device/zig-cc $(BUILD)/device/zig-cxx $(BUILD)/device/zig-ar
	touch $@

doukutsu-device: $(BUILD)/device/zig-shims
	CC_aarch64_unknown_linux_musl="$(CURDIR)/$(BUILD)/device/zig-cc" \
	CXX_aarch64_unknown_linux_musl="$(CURDIR)/$(BUILD)/device/zig-cxx" \
	AR_aarch64_unknown_linux_musl="$(CURDIR)/$(BUILD)/device/zig-ar" \
	cargo build --manifest-path doukutsu-rs/Cargo.toml \
	  --target-dir $(CARGO_TARGET_DIR) \
	  --target aarch64-unknown-linux-musl --release \
	  --no-default-features --features backend-mpc --lib

bin/mpc-cavestory: doukutsu-device src/main.c $(SRC_APP) $(UI_DEVICE_SRC) | bin
	mkdir -p $(BUILD)/device/obj
	for s in src/main.c $(SRC_APP) $(UI_DEVICE_SRC); do \
	  obj=$(BUILD)/device/obj/$$(basename $$s .c).o; \
	  $(ZIGCC) $(DEV_CFLAGS) -c -o $$obj $$s; \
	done
	$(ZIGCXX) -static -O2 -s -o $@ $(BUILD)/device/obj/*.o \
	  $(DEV_RS_LIB) -lm -pthread -ldl

bin:
	mkdir -p bin

$(BUILD)/host:
	mkdir -p $(BUILD)/host

deploy: bin/mpc-cavestory
	../mpc-boot-thingy/setup.sh --ssh --add bin/mpc-cavestory --title "mpc-cavestory"

clean:
	rm -rf build bin doukutsu-rs/target
