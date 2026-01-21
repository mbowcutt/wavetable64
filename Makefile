BUILD_DIR=build
include $(N64_INST)/include/n64.mk

OBJS = $(BUILD_DIR)/main.o \
       $(BUILD_DIR)/audio_engine.o \
       $(BUILD_DIR)/gui.o \
       $(BUILD_DIR)/input.o \
       $(BUILD_DIR)/voice.o \
       $(BUILD_DIR)/wavetable.o

AUDIOCONV_FLAGS ?=

all: wavtable64.z64

$(BUILD_DIR)/wavtable64.elf: $(OBJS)

wavtable64.z64: N64_ROM_TITLE="N64 Wavetable Synth"

clean:
	rm -rf $(BUILD_DIR) wavtable64.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
