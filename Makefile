.PHONY: all clean

BUILD_DIR=build

include $(N64_INST)/include/n64.mk
include midi64/inc.mk

OBJS += $(BUILD_DIR)/src/main.o \
        $(BUILD_DIR)/src/audio_engine.o \
        $(BUILD_DIR)/src/envelope.o \
        $(BUILD_DIR)/src/gui.o \
        $(BUILD_DIR)/src/input.o \
        $(BUILD_DIR)/src/lfo.o \
        $(BUILD_DIR)/src/voice.o \
        $(BUILD_DIR)/src/wavetable.o

all: wavtable64.z64

$(BUILD_DIR)/wavtable64.elf: $(OBJS)

wavtable64.z64: N64_ROM_TITLE="N64 Wavetable Synth"

clean:
	rm -rf $(BUILD_DIR) wavtable64.z64
