# PocketOS, created by Leeko. Copyright (c) 2026 Leeko.
# PC builds of PocketOS's portable code.
#   make test   - unit tests for settings, device model and the Home Assistant protocol
#   make sim    - screen simulator: renders every device screen to PNG (needs LVGL, see below)
#
# LVGL: set LVGL_DIR to a checkout of lvgl v9.5.0 (the same version the firmware uses), e.g.
#   git clone --depth 1 --branch v9.5.0 https://github.com/lvgl/lvgl.git /tmp/lvgl
#   make sim LVGL_DIR=/tmp/lvgl

CC      ?= gcc
# Same warnings-as-errors as the ESP-IDF build, so slips are caught on the PC first
CFLAGS  += -std=c11 -O2 -g -Wall -Werror=all -Wno-sign-compare -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Werror=implicit-function-declaration -D_DEFAULT_SOURCE
CORE    := ../main/core
CJSON   := third_party/cjson
INC     := -I$(CORE) -I$(CJSON) -Itests
CORE_SRC:= $(CORE)/settings.c $(CORE)/model.c $(CORE)/ha_proto.c $(CORE)/battery.c $(CORE)/update.c $(CJSON)/cJSON.c
OUT     := build

LVGL_DIR ?= /tmp/lvgl
UI      := ../main/ui
UI_SRC  := $(wildcard $(UI)/*.c) $(wildcard $(UI)/assets/*.c)
LV_SRC  := $(shell find $(LVGL_DIR)/src -name '*.c' 2>/dev/null)
LV_OBJ  := $(patsubst $(LVGL_DIR)/%.c,$(OUT)/lvgl/%.o,$(LV_SRC))

.PHONY: test sim clean
test: $(OUT)/test_core
	./$(OUT)/test_core

$(OUT)/test_core: tests/test_core.c tests/fixture.h $(CORE_SRC) $(wildcard $(CORE)/*.h)
	@mkdir -p $(OUT)
	$(CC) $(CFLAGS) $(INC) tests/test_core.c $(CORE_SRC) -lm -o $@

$(OUT)/lvgl/%.o: $(LVGL_DIR)/%.c sim/lv_conf.h
	@mkdir -p $(dir $@)
	@$(CC) -O1 -w -DLV_CONF_INCLUDE_SIMPLE -Isim -I$(LVGL_DIR) -c $< -o $@

sim: $(OUT)/sim
	cd $(OUT) && ./sim

$(OUT)/sim: sim/sim.c $(UI_SRC) $(wildcard $(UI)/*.h) $(CORE_SRC) $(LV_OBJ) sim/lv_conf.h
	@$(CC) $(CFLAGS) -DLV_CONF_INCLUDE_SIMPLE -DPOCKETOS_SIM $(INC) -I$(UI) -Isim -I$(LVGL_DIR) sim/sim.c $(UI_SRC) $(CORE_SRC) $(LV_OBJ) -lm -o $@

clean:
	rm -rf $(OUT)
