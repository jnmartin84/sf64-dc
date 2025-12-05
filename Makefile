# Build options can be changed by modifying the makefile or by building with 'make SETTING=value'.
# It is also possible to override the settings in Defaults in a file called .make_options as 'SETTING=value'.

-include .make_options
MAKEFLAGS += --no-builtin-rules --no-print-directory

####################################################
#### Begin User Configurable Options and Cheats ####
####################################################

### Enable MMU translation of N64 segmented addresses
MMU_SEGMENTED ?= 1

### Enable scaling of light intensity instead of clamping
SCALE_LIGHTS ?= 0

### Enable 320x240 resolution
LOWRES ?= 0

### Enable 32KHz sample rate
USE_32KHZ ?= 0

### Enable JP Audio Voice
# Place JP N64 ROM named baserom.jp.rev0.z64 in project root,
# then run `make decompress extract VERSION=jp REV=rev0` to extract voice files.
AUDIO_VOICE_JP ?= 0

### Enable testing mode
# Turns on no damage, extra everything, and level select
TESTING_MODE ?= 0

### Take no damage
I_DONT_WANT_TO_DIE ?= 0

### Get laser upgrades, extra lives, extra bombs
EXTRA_EVERYTHING ?= 0

### Level select
# At the map screen, use the analog stick to select a level.
# Press D-Pad Up to select an advanced level phase (warp zone or Andross fight).
# Useful for debugging and speedrunning training.
MODS_LEVEL_SELECT ?= 0

### MR logo
# Set a custom IP.BIN boot logo when building CDI files
MR_LOGO ?= assets/dreamcast/mrlogo.mr

### Music URL
# Set the URL for downloading music files
MUSIC_ARCHIVE_URL ?= https://archive.org/download/sf64_ost_seqid/sf64_ost_seqid.tgz

##################################################
#### End User Configurable Options and Cheats ####
##################################################
# Only change things below here if you know what you are doing!

# If COMPARE is 1, check the output md5sum after building
COMPARE ?= 0
# If NON_MATCHING is 1, define the NON_MATCHING C flag when building
NON_MATCHING ?= 1
# if WERROR is 1, pass -Werror to CC_CHECK, so warnings would be treated as errors
WERROR ?= 0
# Keep .mdebug section in build
KEEP_MDEBUG ?= 0
# Check code syntax with host compiler
RUN_CC_CHECK ?= 0
CC_CHECK_COMP ?= kos-cc
# Dump build object files
OBJDUMP_BUILD ?= 0
# Number of threads to compress with
N_THREADS ?= $(shell nproc)
# Dreamcast uses kos-cc
COMPILER ?= kos-cc
# Whether to colorize build messages
COLOR ?= 1
# Whether to hide commands or not
VERBOSE ?= 1
# Command for printing messages during the make.
PRINT ?= printf

VERSION ?= us
REV ?= rev1

MUSIC_ARCHIVE        := sf64_ost_seqid.tgz
BASEROM              := baserom.$(VERSION).$(REV).z64
BASEROM_UNCOMPRESSED := baserom.$(VERSION).$(REV).uncompressed.z64
TARGET               := sf64

### Output ###

BUILD_DIR := build
TOOLS	  := tools
PYTHON	  := python3

ELF           := $(TARGET).elf
BIN           := $(TARGET).bin
CDI_CDR       := $(TARGET)-cdr.cdi
CDI_ODE       := $(TARGET)-ode.cdi
DSISO         := $(TARGET)-ds.iso
FILES_ZIP     := $(TARGET).zip
SF_DATA_PATH  := sf_data
SF_MUSIC_PATH := music

#### Setup ####

# If gcc is used, define the NON_MATCHING flag respectively so the files that
# are safe to be used can avoid using GLOBAL_ASM which doesn't work with gcc.
CFLAGS += -DCOMPILER_GCC -DNON_MATCHING=1 -Wno-int-conversion -falign-functions=32 -fno-data-sections -DAVOID_UB=1 -MMD -MP -Wno-incompatible-pointer-types -Wno-missing-braces -Wno-unused-variable -Wno-switch  -DGBI_FLOATS -fno-toplevel-reorder

ifeq ($(SCALE_LIGHTS),1)
  CFLAGS += -DSCALE_LIGHTS
endif

ifeq ($(MMU_SEGMENTED),1)
  CFLAGS += -DMMU_SEGMENTED
endif

ifeq ($(LOWRES),1)
  CFLAGS += -DLOWRES
endif

ifeq ($(USE_32KHZ),1)
  CFLAGS += -DUSE_32KHZ
endif

ifeq ($(TESTING_MODE),1)
  CFLAGS += -DTESTING_MODE
endif

ifneq (,$(filter 1,$(TESTING_MODE) $(I_DONT_WANT_TO_DIE)))
  CFLAGS += -DI_DONT_WANT_TO_DIE
endif

ifneq (,$(filter 1,$(TESTING_MODE) $(EXTRA_EVERYTHING)))
  CFLAGS += -DEXTRA_EVERYTHING
endif

ifneq (,$(filter 1,$(TESTING_MODE) $(MODS_LEVEL_SELECT)))
  CFLAGS += -DMODS_LEVEL_SELECT
endif

ifeq ($(AUDIO_VOICE_JP),1)
  CFLAGS += -DAUDIO_VOICE_JP
endif

NON_MATCHING := 1

CC       := kos-cc
OPTFLAGS := -Os
CC_CHECK  = @:

BUILD_DEFINES ?=

# Version check
ifeq ($(VERSION),jp)
    BUILD_DEFINES   += -DVERSION_JP=1
endif

ifeq ($(VERSION),us)
    BUILD_DEFINES   += -DVERSION_US=1
endif

ifeq ($(VERSION),eu)
    BUILD_DEFINES   += -DVERSION_EU=1
	REV := rev0
endif

ifeq ($(VERSION),au)
	BUILD_DEFINES	+= -DVERSION_AU=1
	REV := rev0
endif

ifeq ($(VERSION),ln)
	BUILD_DEFINES	+= -DVERSION_LN=1
	REV := rev0
endif

ifeq ($(NON_MATCHING),1)
    BUILD_DEFINES   += -DNON_MATCHING -DAVOID_UB
    CPPFLAGS += -DNON_MATCHING -DAVOID_UB
endif

MAKE = gmake
CPPFLAGS += -fno-dollars-in-identifiers -P
LDFLAGS  := --no-check-sections --accept-unknown-input-arch --emit-relocs

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(OS),Windows_NT)
$(error Native Windows is currently unsupported for building this repository, use WSL instead c:)
else ifeq ($(UNAME_S),Linux)
    DETECTED_OS := linux
    #Detect aarch64 devices (Like Raspberry Pi OS 64-bit)
    #If it's found, then change the compiler to a version that can compile in 32 bit mode.
    ifeq ($(UNAME_M),aarch64)
        CC_CHECK_COMP := arm-linux-gnueabihf-gcc
    endif
else ifeq ($(UNAME_S),Darwin)
    DETECTED_OS := macos
    MAKE := gmake
    CPPFLAGS += -xc++
    CC_CHECK_COMP := clang
endif

# Support python venv's if one is installed.
PYTHON_VENV = .venv/bin/python3
ifneq "$(wildcard $(PYTHON_VENV) )" ""
  PYTHON = $(PYTHON_VENV)
endif

ifeq ($(VERBOSE),0)
  V := @
endif

ifeq ($(COLOR),1)
NO_COL  := \033[0m
RED     := \033[0;31m
GREEN   := \033[0;32m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
BLINK   := \033[33;5m
endif

# Common build print status function
define print
  @$(PRINT) "$(GREEN)$(1) $(YELLOW)$(2)$(GREEN) -> $(BLUE)$(3)$(NO_COL)\n"
endef

define print2
  @$(PRINT) "$(GREEN)$(1)$(NO_COL)\n"
endef

### Compiler ###
AS		:= kos-as
LD		:= kos-ld
OBJCOPY		:= kos-objcopy
OBJDUMP		:= kos-objdump
ICONV		:= iconv
ASM_PROC        := $(PYTHON) $(TOOLS)/asm-processor/build.py
CAT             := cat
TORCH           := $(TOOLS)/Torch/cmake-build-release/torch

# Returns the path to the command $(1) if exists. Otherwise returns an empty string.
find-command = $(shell which $(1) 2>/dev/null)

# Prefer clang as C preprocessor if installed on the system
ifneq (,$(call find-command,clang))
  CPP      := clang
  CPPFLAGS := -E -P -x c -Wno-trigraphs -Wmissing-prototypes -Wstrict-prototypes -D_LANGUAGE_ASSEMBLY
else
  CPP      := cpp
  CPPFLAGS := -P -Wno-trigraphs -Wmissing-prototypes -Wstrict-prototypes -D_LANGUAGE_ASSEMBLY
endif

ASM_PROC_FLAGS  := --input-enc=utf-8 --output-enc=euc-jp --convert-statics=global-with-filename

SPLAT           ?= $(PYTHON) $(TOOLS)/splat/split.py
SPLAT_YAML      ?= $(TARGET).$(VERSION).$(REV).yaml

COMPTOOL		:= $(TOOLS)/comptool.py
COMPTOOL_DIR	:= baserom
MIO0			:= $(TOOLS)/mio0


IINC := -Iinclude -Ibin/$(VERSION).$(REV) -I.
IINC += -Ilib/ultralib/include -Ilib/ultralib/include/PR -Ilib/ultralib/include/ido -I./src/audio

ifeq ($(KEEP_MDEBUG),0)
  RM_MDEBUG = $(OBJCOPY) --remove-section .mdebug $@
else
  RM_MDEBUG = @:
endif

# Check code syntax with host compiler
CHECK_WARNINGS := -Wall -Wextra -Wimplicit-fallthrough -Wno-unknown-pragmas -Wno-missing-braces -Wno-sign-compare -Wno-uninitialized
# Have CC_CHECK pretend to be a MIPS compiler
#MIPS_BUILTIN_DEFS := -DMIPSEB -D_MIPS_FPSET=16 -D_MIPS_ISA=2 -D_ABIO32=1 -D_MIPS_SIM=_ABIO32 -D_MIPS_SZINT=32 -D_MIPS_SZPTR=32
ifneq ($(RUN_CC_CHECK),0)
#   The -MMD flags additionaly creates a .d file with the same name as the .o file.
    CHECK_WARNINGS    := -Wno-unused-variable -Wno-int-conversion
    CC_CHECK          := $(CC_CHECK_COMP)
    CC_CHECK_FLAGS    := -MMD -MP -fno-builtin -fsyntax-only -funsigned-char -fdiagnostics-color -std=gnu89 -DNON_MATCHING -DAVOID_UB -DCC_CHECK=1

    # Ensure that gcc treats the code as 32-bit
    ifeq ($(UNAME_M),aarch64)
        CC_CHECK_FLAGS += -march=armv7-a+fp
    else
        CC_CHECK_FLAGS += -m32
    endif
	ifneq ($(WERROR), 0)
        CHECK_WARNINGS += -Werror
    endif
else
    CC_CHECK          := @:
endif

ASFLAGS         :=
# -march=vr4300 -32 -G0
COMMON_DEFINES  := -D_MIPS_SZLONG=32
GBI_DEFINES     := -DF3DEX_GBI
RELEASE_DEFINES := -DNDEBUG
AS_DEFINES      :=  -D_LANGUAGE_ASSEMBLY -D_ULTRA64
#-DMIPSEB
C_DEFINES       := -DLANGUAGE_C -D_LANGUAGE_C -DBUILD_VERSION=VERSION_H ${RELEASE_DEFINES}
ENDIAN          :=
# -EL
# -EB

ICONV_FLAGS     := --from-code=UTF-8 --to-code=EUC-JP

# Use relocations and abi fpr names in the dump
OBJDUMP_FLAGS := --disassemble --reloc --disassemble-zeroes -Mreg-names=32 -Mno-aliases

ifneq ($(OBJDUMP_BUILD), 0)
    OBJDUMP_CMD = $(OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(@:.o=.dump.s)
    OBJCOPY_BIN = $(OBJCOPY) -O binary $@ $@.bin
else
    OBJDUMP_CMD = @:
    OBJCOPY_BIN = @:
endif

#### Files ####

$(shell mkdir -p asm bin linker_scripts/$(VERSION)/$(REV)/auto)

SRC_DIRS      := $(shell find src -type d)
# Temporary, until we decide how we're gonna handle other versions
ifeq ($(VERSION), jp)
SRC_DIRS      := $(shell find srcjp -type d)
endif
ifeq ($(VERSION), eu)
SRC_DIRS      := $(shell find srceu -type d)
endif
ASM_DIRS      := $(shell find asm/$(VERSION)/$(REV) -type d -not -path "asm/$(VERSION)/$(REV)/nonmatchings/*")
BIN_DIRS      := $(shell find bin -type d)


C_FILES       := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
C_FILES       := $(filter-out %.inc.c,$(C_FILES))
BIN_FILES     := $(foreach dir,$(BIN_DIRS),$(wildcard $(dir)/*.bin))
O_FILES       := $(foreach f,$(C_FILES:.c=.o),$(BUILD_DIR)/$f) \
                 $(foreach f,$(BIN_FILES:.bin=.o),$(BUILD_DIR)/$f)

# Automatic dependency files
DEP_FILES := $(O_FILES:.o=.d) \
             $(O_FILES:.o=.asmproc.d)

# create build directories
$(shell mkdir -p $(BUILD_DIR)/linker_scripts/$(VERSION)/$(REV) $(BUILD_DIR)/linker_scripts/$(VERSION)/$(REV)/auto $(foreach dir,$(SRC_DIRS) $(ASM_DIRS) $(BIN_DIRS),$(BUILD_DIR)/$(dir)))

# per-file flags

build/src/libc_math64.o:  OPTFLAGS := -O3
build/src/audio/mixer.o: OPTFLAGS := -O3
build/src/gfx/gfx_retro_dc.o: OPTFLAGS := -O3
build/src/sys/sys_matrix.o: OPTFLAGS := -O2
build/src/sys/sys_math.o: OPTFLAGS := -O3

# cc & asm-processor
build/src/libultra/gu/lookat.o:      OPTFLAGS := -O3
build/src/libultra/gu/ortho.o:       OPTFLAGS := -O3
build/src/libultra/gu/perspective.o: OPTFLAGS := -O3
build/src/libultra/gu/mtxutil.o:     OPTFLAGS := -O3

# directory flags
ASSET_OPTFLAGS := -fno-common -fno-toplevel-reorder -fno-zero-initialized-in-bss
ASSET_DIRS := ast_7_ti_1 ast_7_ti_2 ast_8_ti ast_9_ti ast_A_ti \
              ast_allies ast_andross ast_aquas ast_area_6 ast_arwing \
              ast_bg_planet ast_bg_space ast_blue_marine ast_bolse ast_common \
              ast_corneria ast_ending ast_ending_award_back ast_ending_award_front \
              ast_ending_expert ast_enmy_planet ast_enmy_space ast_font_3d \
              ast_fortuna ast_great_fox ast_katina ast_landmaster ast_logo \
              ast_macbeth ast_map ast_meteo ast_option ast_radio \
              ast_sector_x ast_sector_y ast_sector_z ast_solar ast_star_wolf \
              ast_text ast_titania ast_title ast_training ast_ve1_boss \
              ast_venom_1 ast_venom_2 ast_versus ast_vs_menu ast_warp_zone ast_zoness

# Apply the ASSET_OPTFLAGS to each of the ASSET_DIRS
$(foreach dir,$(ASSET_DIRS),$(eval build/src/assets/$(dir)/%.o: OPTFLAGS := $(ASSET_OPTFLAGS)))

# music files
MUSIC_FILES := $(SF_MUSIC_PATH)/02.adp $(SF_MUSIC_PATH)/03.adp $(SF_MUSIC_PATH)/04.adp \
               $(SF_MUSIC_PATH)/05.adp $(SF_MUSIC_PATH)/06.adp $(SF_MUSIC_PATH)/07.adp \
               $(SF_MUSIC_PATH)/08.adp $(SF_MUSIC_PATH)/09.adp $(SF_MUSIC_PATH)/10.adp \
               $(SF_MUSIC_PATH)/12.adp $(SF_MUSIC_PATH)/13.adp $(SF_MUSIC_PATH)/14.adp \
               $(SF_MUSIC_PATH)/17.adp $(SF_MUSIC_PATH)/18.adp $(SF_MUSIC_PATH)/19.adp \
               $(SF_MUSIC_PATH)/21_2.adp $(SF_MUSIC_PATH)/23.adp $(SF_MUSIC_PATH)/28.adp \
               $(SF_MUSIC_PATH)/33.adp $(SF_MUSIC_PATH)/33_2.adp $(SF_MUSIC_PATH)/34.adp \
               $(SF_MUSIC_PATH)/35.adp $(SF_MUSIC_PATH)/36.adp $(SF_MUSIC_PATH)/37.adp \
               $(SF_MUSIC_PATH)/38.adp $(SF_MUSIC_PATH)/39.adp $(SF_MUSIC_PATH)/40.adp \
               $(SF_MUSIC_PATH)/42.adp $(SF_MUSIC_PATH)/43.adp $(SF_MUSIC_PATH)/44.adp \
               $(SF_MUSIC_PATH)/45.adp $(SF_MUSIC_PATH)/46.adp $(SF_MUSIC_PATH)/47.adp \
               $(SF_MUSIC_PATH)/49.adp $(SF_MUSIC_PATH)/50.adp $(SF_MUSIC_PATH)/51.adp \
               $(SF_MUSIC_PATH)/54.adp $(SF_MUSIC_PATH)/55.adp $(SF_MUSIC_PATH)/56.adp \
               $(SF_MUSIC_PATH)/58.adp $(SF_MUSIC_PATH)/60.adp $(SF_MUSIC_PATH)/61.adp \
               $(SF_MUSIC_PATH)/62.adp $(SF_MUSIC_PATH)/63.adp $(SF_MUSIC_PATH)/64.adp \
               $(SF_MUSIC_PATH)/65.adp

# Dreamcast-specific objects
DC_OBJS := build/dcconsole.o \
           build/dclogo.o

# Audio objects
AUDIO_OBJS := build/src/audio/libwav.o \
              build/src/audio/sndwav.o \
              build/src/audio/mixer.o \
              build/src/dcaudio/driver.o \
              build/src/audio/audio_synthesis.o \
              build/src/audio/audio_heap.o \
              build/src/audio/audio_load.o \
              build/src/audio/audio_playback.o \
              build/src/audio/audio_effects.o \
              build/src/audio/audio_seqplayer.o \
              build/src/audio/audio_general.o \
              build/src/audio/audio_thread.o \
              build/src/audio/wave_samples.o \
              build/src/audio/note_data.o

# Graphics objects
GFX_OBJS := build/src/gfx/gfx_buf.o \
            build/src/gfx/gfx_dc.o \
            build/src/gfx/gfx_retro_dc.o

# System objects
SYS_OBJS := build/src/sys/sys_joybus.o \
            build/src/sys/sys_lib.o \
            build/src/sys/sys_lights.o \
            build/src/sys/sys_main.o \
            build/src/sys/sys_math.o \
            build/src/sys/sys_matrix.o \
            build/src/sys/sys_memory.o \
            build/src/sys/sys_timer.o \
            build/src/sys/sys_save.o \
            build/src/sys/sys_fault.o

# libultra objects
LIBULTRA_OBJS := build/src/libultra/gu/perspective.o \
                 build/src/libultra/gu/lookat.o \
                 build/src/libultra/gu/ortho.o \
                 build/src/libultra/gu/mtxutil.o

# Engine objects
ENGINE_OBJS := build/src/engine/fox_360.o \
               build/src/engine/fox_beam.o \
               build/src/engine/fox_bg.o \
               build/src/engine/fox_boss.o \
               build/src/engine/fox_tank.o \
               build/src/engine/fox_demo.o \
               build/src/engine/fox_display.o \
               build/src/engine/fox_load.o \
               build/src/engine/fox_edata.o \
               build/src/engine/fox_edisplay.o \
               build/src/engine/fox_enmy.o \
               build/src/engine/fox_enmy2.o \
               build/src/engine/fox_effect.o \
               build/src/engine/fox_fade.o \
               build/src/engine/fox_blur.o \
               build/src/engine/fox_hud.o \
               build/src/engine/fox_col1.o \
               build/src/engine/fox_std_lib.o \
               build/src/engine/fox_game.o \
               build/src/engine/fox_col2.o \
               build/src/engine/fox_pause.o \
               build/src/engine/fox_play.o \
               build/src/engine/fox_rcp.o \
               build/src/engine/fox_radio.o \
               build/src/engine/fox_reset.o \
               build/src/engine/fox_versus.o \
               build/src/engine/fox_message.o \
               build/src/engine/fox_save.o \
               build/src/engine/fox_context.o \
               build/src/engine/fox_shapes.o \
               build/src/engine/fox_wheels.o \
               build/src/engine/fox_msg_palette.o

# Overlay objects - i1
OVERLAY_I1_OBJS := build/src/overlays/ovl_i1/fox_i1.o \
                   build/src/overlays/ovl_i1/fox_co.o \
                   build/src/overlays/ovl_i1/fox_ve1.o \
                   build/src/overlays/ovl_i1/fox_tr.o \
                   build/src/overlays/ovl_i1/fox_tr360.o

# Overlay objects - i2
OVERLAY_I2_OBJS := build/src/overlays/ovl_i2/fox_i2.o \
                   build/src/overlays/ovl_i2/fox_me.o \
                   build/src/overlays/ovl_i2/fox_sx.o

# Overlay objects - i3
OVERLAY_I3_OBJS := build/src/overlays/ovl_i3/fox_i3.o \
                   build/src/overlays/ovl_i3/fox_a6.o \
                   build/src/overlays/ovl_i3/fox_zo.o \
                   build/src/overlays/ovl_i3/fox_so.o \
                   build/src/overlays/ovl_i3/fox_aq.o

# Overlay objects - i4
OVERLAY_I4_OBJS := build/src/overlays/ovl_i4/fox_i4.o \
                   build/src/overlays/ovl_i4/fox_fo.o \
                   build/src/overlays/ovl_i4/fox_bo.o \
                   build/src/overlays/ovl_i4/fox_ka.o \
                   build/src/overlays/ovl_i4/fox_sz.o

# Overlay objects - i5
OVERLAY_I5_OBJS := build/src/overlays/ovl_i5/fox_i5.o \
                   build/src/overlays/ovl_i5/fox_ti_cs.o \
                   build/src/overlays/ovl_i5/fox_ti.o \
                   build/src/overlays/ovl_i5/fox_ma.o \
                   build/src/overlays/ovl_i5/fox_ground.o

# Overlay objects - i6
OVERLAY_I6_OBJS := build/src/overlays/ovl_i6/fox_i6.o \
                   build/src/overlays/ovl_i6/fox_andross.o \
                   build/src/overlays/ovl_i6/fox_ve2.o \
                   build/src/overlays/ovl_i6/fox_sy.o \
                   build/src/overlays/ovl_i6/fox_turret.o

# Overlay objects - menu
OVERLAY_MENU_OBJS := build/src/overlays/ovl_menu/fox_i_menu.o \
                     build/src/overlays/ovl_menu/fox_title.o \
                     build/src/overlays/ovl_menu/fox_option.o \
                     build/src/overlays/ovl_menu/fox_map.o

# Overlay objects - ending
OVERLAY_ENDING_OBJS := build/src/overlays/ovl_ending/fox_end1.o \
                       build/src/overlays/ovl_ending/fox_end2.o

# Overlay objects - unused
OVERLAY_UNUSED_OBJS := build/src/overlays/ovl_unused/fox_unused.o

# Combine all overlays
OVERLAY_OBJS := $(OVERLAY_I1_OBJS) \
                $(OVERLAY_I2_OBJS) \
                $(OVERLAY_I3_OBJS) \
                $(OVERLAY_I4_OBJS) \
                $(OVERLAY_I5_OBJS) \
                $(OVERLAY_I6_OBJS) \
                $(OVERLAY_MENU_OBJS) \
                $(OVERLAY_ENDING_OBJS) \
                $(OVERLAY_UNUSED_OBJS)

# Miscellaneous objects
MISC_OBJS := build/src/libc_math64.o \
             build/src/dmatable.o \
             build/src/buffers.o \
             build/src/dc/ast_radio.o

# Final link objects (before asset symbols)
FINAL_OBJS := build/src/ultra_reimpl.o \
              build/src/engine/fox_rcp_init.o \
              build/src/audio/audio_context.o \
              build/src/audio/audio_tables.o \
              build/src/gfx/gfx_cc.o \
              build/src/gfx/gfx_gldc.o

# Asset ELF symbols list
ASSET_ELFS := ast_common ast_bg_space ast_bg_planet ast_arwing ast_landmaster \
              ast_blue_marine ast_versus ast_enmy_planet ast_enmy_space \
              ast_great_fox ast_star_wolf ast_allies ast_corneria ast_meteo \
              ast_titania ast_7_ti_2 ast_8_ti ast_9_ti ast_A_ti ast_7_ti_1 \
              ast_sector_x ast_sector_z ast_aquas ast_area_6 ast_venom_1 \
              ast_venom_2 ast_ve1_boss ast_bolse ast_fortuna ast_sector_y \
              ast_solar ast_zoness ast_katina ast_macbeth ast_warp_zone \
              ast_title ast_map ast_option ast_vs_menu ast_text ast_font_3d \
              ast_andross ast_logo ast_ending ast_ending_award_front \
              ast_ending_award_back ast_ending_expert ast_training

# Generate --just-symbols flags for all asset ELFs
ASSET_SYMBOLS := $(foreach elf,$(ASSET_ELFS),-Wl,--just-symbols=build/src/assets/$(elf)/$(elf).elf)

# Libraries
LIBS := -lc -lm -lkallisti -lGL

default: $(ELF)

all: elf bin cdi-cdr cdi-ode files-zip dsiso

toolchain:
	@$(MAKE) -s -C $(TOOLS)

torch:
	@$(MAKE) -s -C $(TOOLS) torch
	rm -f torch.hash.yml

init:
	@$(MAKE) clean
	@$(MAKE) decompress
	@$(MAKE) extract -j $(N_THREADS)
	@$(MAKE) assets -j $(N_THREADS)
	@$(MAKE) objects -j $(N_THREADS)
	@touch initted.touch

elf: $(ELF)

bin: $(BIN)

cdi-cdr: $(CDI_CDR)

cdi-ode: $(CDI_ODE)

files-zip: $(FILES_ZIP)

dsiso: $(DSISO)

#### Main Targets ###
$(ELF): sf-data objects
	$(call print2,Building $(ELF)...)
	kos-cc -Wl,-v \
		$(DC_OBJS) \
		$(AUDIO_OBJS) \
		$(GFX_OBJS) \
		$(SYS_OBJS) \
		$(LIBULTRA_OBJS) \
		$(ENGINE_OBJS) \
		$(OVERLAY_OBJS) \
		$(MISC_OBJS) \
		$(ASSET_SYMBOLS) \
		$(FINAL_OBJS) \
		-o $(ELF) $(LIBS)

$(BIN): $(ELF)
	$(call print2,Creating $(BIN)...)
	sh-elf-objcopy -O binary $(ELF) $(BIN)

$(CDI_ODE): $(ELF) $(MUSIC_FILES)
	$(call print2,Creating CDI file for ODE use...)
	mkdcdisc -a jnmartin84 -n "Star Fox 64" -r 20251205 -i $(MR_LOGO) -f sf64.ico -d $(SF_MUSIC_PATH) -d $(SF_DATA_PATH) -e $(ELF) -o $(CDI_ODE) -N

$(CDI_CDR): $(ELF) $(MUSIC_FILES)
	$(call print2,Creating CDI file for burning to CD-R...)
	mkdcdisc -a jnmartin84 -n "Star Fox 64" -r 20251205 -i $(MR_LOGO) -f sf64.ico -d $(SF_MUSIC_PATH) -d $(SF_DATA_PATH) -e $(ELF) -o $(CDI_CDR)

$(DSISO): $(BIN) $(MUSIC_FILES)
	$(call print2,Creating DreamShell ISO file...)
	mkisofs -G ip.bin -V "STARFOX64" -r -J -l -graft-points -o $(DSISO) \
		$(SF_MUSIC_PATH)=$(SF_MUSIC_PATH) $(SF_DATA_PATH)=$(SF_DATA_PATH) 1ST_READ.BIN=$(BIN) sf64.ico

$(FILES_ZIP): $(ELF) $(BIN) $(MUSIC_FILES)
	$(call print2,Creating ZIP archive for plain files...)
	zip -r $(FILES_ZIP) $(SF_MUSIC_PATH) $(SF_DATA_PATH) $(ELF) $(BIN) sf64.ico

sf-data: initted.touch
	@mkdir -p $(SF_DATA_PATH)
	$(call print2,Generating Dreamcast replacement graphics...)
	@$(KOS_BASE)/utils/pvrtex/pvrtex -i assets/dreamcast/dclogo.png -o dclogo.tex -f ARGB1555 -s 128
	@$(KOS_BASE)/utils/pvrtex/pvrtex -i assets/dreamcast/aVsDCConsoleTex.png -o console.tex -f ARGB1555 -s 256
	@$(KOS_BASE)/utils/bin2o/bin2o dclogo.tex dclogo build/dclogo.o
	@$(KOS_BASE)/utils/bin2o/bin2o console.tex vs_dc_console build/dcconsole.o
	@rm dclogo.tex
	@rm console.tex
	$(call print2,Copying audio bank/sequence data...)
ifeq ($(AUDIO_VOICE_JP),1)
	cp bin/jp/rev0/audio_bank.bin $(SF_DATA_PATH)/audbank.bin
	cp bin/jp/rev0/audio_seq.bin $(SF_DATA_PATH)/audseq.bin
	cp bin/jp/rev0/audio_table.bin $(SF_DATA_PATH)/audtable.bin
else
	@cp bin/$(VERSION)/$(REV)/audio_bank.bin $(SF_DATA_PATH)/audbank.bin
	@cp bin/$(VERSION)/$(REV)/audio_seq.bin $(SF_DATA_PATH)/audseq.bin
	@cp bin/$(VERSION)/$(REV)/audio_table.bin $(SF_DATA_PATH)/audtable.bin
endif
	$(call print2,Generating asset segments...)
	@sh-elf-ld -EL -t -e 0 -Ttext=05000000 build/src/assets/ast_text/ast_text.o -o build/src/assets/ast_text/ast_text.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_text/ast_text.elf $(SF_DATA_PATH)/text.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=01000000 build/src/assets/ast_common/ast_common.o -o build/src/assets/ast_common/ast_common.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_common/ast_common.elf $(SF_DATA_PATH)/common.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=02000000 build/src/assets/ast_bg_space/ast_bg_space.o -o build/src/assets/ast_bg_space/ast_bg_space.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bg_space/ast_bg_space.elf $(SF_DATA_PATH)/bgspace.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=02000000 build/src/assets/ast_bg_planet/ast_bg_planet.o -o build/src/assets/ast_bg_planet/ast_bg_planet.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bg_planet/ast_bg_planet.elf $(SF_DATA_PATH)/bgplanet.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_arwing/ast_arwing.o -o build/src/assets/ast_arwing/ast_arwing.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_arwing/ast_arwing.elf $(SF_DATA_PATH)/arwing.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_landmaster/ast_landmaster.o -o build/src/assets/ast_landmaster/ast_landmaster.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_landmaster/ast_landmaster.elf $(SF_DATA_PATH)/landmstr.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_blue_marine/ast_blue_marine.o -o build/src/assets/ast_blue_marine/ast_blue_marine.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_blue_marine/ast_blue_marine.elf $(SF_DATA_PATH)/blumarin.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_versus/ast_versus.o -o build/src/assets/ast_versus/ast_versus.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_versus/ast_versus.elf $(SF_DATA_PATH)/versus.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=04000000 build/src/assets/ast_enmy_planet/ast_enmy_planet.o -o build/src/assets/ast_enmy_planet/ast_enmy_planet.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_enmy_planet/ast_enmy_planet.elf $(SF_DATA_PATH)/enmyplnt.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=04000000 build/src/assets/ast_enmy_space/ast_enmy_space.o -o build/src/assets/ast_enmy_space/ast_enmy_space.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_enmy_space/ast_enmy_space.elf $(SF_DATA_PATH)/enmyspce.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=0E000000 build/src/assets/ast_great_fox/ast_great_fox.o -o build/src/assets/ast_great_fox/ast_great_fox.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_great_fox/ast_great_fox.elf $(SF_DATA_PATH)/greatfox.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=0F000000 build/src/assets/ast_star_wolf/ast_star_wolf.o -o build/src/assets/ast_star_wolf/ast_star_wolf.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_star_wolf/ast_star_wolf.elf $(SF_DATA_PATH)/starwolf.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=0D000000 build/src/assets/ast_allies/ast_allies.o -o build/src/assets/ast_allies/ast_allies.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_allies/ast_allies.elf $(SF_DATA_PATH)/allies.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_corneria/ast_corneria.o -o build/src/assets/ast_corneria/ast_corneria.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_corneria/ast_corneria.elf $(SF_DATA_PATH)/corneria.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_meteo/ast_meteo.o -o build/src/assets/ast_meteo/ast_meteo.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_meteo/ast_meteo.elf $(SF_DATA_PATH)/meteo.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_titania/ast_titania.o -o build/src/assets/ast_titania/ast_titania.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_titania/ast_titania.elf $(SF_DATA_PATH)/titania.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_x/ast_sector_x.o -o build/src/assets/ast_sector_x/ast_sector_x.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_x/ast_sector_x.elf $(SF_DATA_PATH)/sectorx.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_z/ast_sector_z.o -o build/src/assets/ast_sector_z/ast_sector_z.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_z/ast_sector_z.elf $(SF_DATA_PATH)/sectorz.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_aquas/ast_aquas.o -o build/src/assets/ast_aquas/ast_aquas.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_aquas/ast_aquas.elf $(SF_DATA_PATH)/aquas.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_area_6/ast_area_6.o -o build/src/assets/ast_area_6/ast_area_6.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_area_6/ast_area_6.elf $(SF_DATA_PATH)/area6.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_venom_1/ast_venom_1.o -o build/src/assets/ast_venom_1/ast_venom_1.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_venom_1/ast_venom_1.elf $(SF_DATA_PATH)/venom1.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_venom_2/ast_venom_2.o -o build/src/assets/ast_venom_2/ast_venom_2.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_venom_2/ast_venom_2.elf $(SF_DATA_PATH)/venom2.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=09000000 build/src/assets/ast_ve1_boss/ast_ve1_boss.o -o build/src/assets/ast_ve1_boss/ast_ve1_boss.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ve1_boss/ast_ve1_boss.elf $(SF_DATA_PATH)/ve1boss.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_bolse/ast_bolse.o -o build/src/assets/ast_bolse/ast_bolse.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bolse/ast_bolse.elf $(SF_DATA_PATH)/bolse.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_fortuna/ast_fortuna.o -o build/src/assets/ast_fortuna/ast_fortuna.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_fortuna/ast_fortuna.elf $(SF_DATA_PATH)/fortuna.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_y/ast_sector_y.o -o build/src/assets/ast_sector_y/ast_sector_y.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_y/ast_sector_y.elf $(SF_DATA_PATH)/sectory.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_solar/ast_solar.o -o build/src/assets/ast_solar/ast_solar.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_solar/ast_solar.elf $(SF_DATA_PATH)/solar.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_zoness/ast_zoness.o -o build/src/assets/ast_zoness/ast_zoness.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_zoness/ast_zoness.elf $(SF_DATA_PATH)/zoness.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_katina/ast_katina.o -o build/src/assets/ast_katina/ast_katina.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_katina/ast_katina.elf $(SF_DATA_PATH)/katina.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_macbeth/ast_macbeth.o -o build/src/assets/ast_macbeth/ast_macbeth.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_macbeth/ast_macbeth.elf $(SF_DATA_PATH)/macbeth.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_warp_zone/ast_warp_zone.o -o build/src/assets/ast_warp_zone/ast_warp_zone.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_warp_zone/ast_warp_zone.elf $(SF_DATA_PATH)/warpzone.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_title/ast_title.o -o build/src/assets/ast_title/ast_title.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_title/ast_title.elf $(SF_DATA_PATH)/title.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_map/ast_map.o -o build/src/assets/ast_map/ast_map.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_map/ast_map.elf $(SF_DATA_PATH)/map.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_option/ast_option.o -o build/src/assets/ast_option/ast_option.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_option/ast_option.elf $(SF_DATA_PATH)/option.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_vs_menu/ast_vs_menu.o -o build/src/assets/ast_vs_menu/ast_vs_menu.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_vs_menu/ast_vs_menu.elf $(SF_DATA_PATH)/vsmenu.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=09000000 build/src/assets/ast_font_3d/ast_font_3d.o -o build/src/assets/ast_font_3d/ast_font_3d.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_font_3d/ast_font_3d.elf $(SF_DATA_PATH)/font3d.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=0C000000 build/src/assets/ast_andross/ast_andross.o -o build/src/assets/ast_andross/ast_andross.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_andross/ast_andross.elf $(SF_DATA_PATH)/andross.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=0F000000 build/src/assets/ast_logo/ast_logo.o -o build/src/assets/ast_logo/ast_logo.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_logo/ast_logo.elf $(SF_DATA_PATH)/logo.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_ending/ast_ending.o -o build/src/assets/ast_ending/ast_ending.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending/ast_ending.elf $(SF_DATA_PATH)/ending.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_award_front/ast_ending_award_front.o -o build/src/assets/ast_ending_award_front/ast_ending_award_front.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_award_front/ast_ending_award_front.elf $(SF_DATA_PATH)/awdfront.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_award_back/ast_ending_award_back.o -o build/src/assets/ast_ending_award_back/ast_ending_award_back.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_award_back/ast_ending_award_back.elf $(SF_DATA_PATH)/awdback.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_expert/ast_ending_expert.o -o build/src/assets/ast_ending_expert/ast_ending_expert.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_expert/ast_ending_expert.elf $(SF_DATA_PATH)/endexprt.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_training/ast_training.o -o build/src/assets/ast_training/ast_training.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_training/ast_training.elf $(SF_DATA_PATH)/training.bin
	@sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_7_ti_1/ast_7_ti_1.o -o build/src/assets/ast_7_ti_1/ast_7_ti_1.elf
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_7_ti_1/ast_7_ti_1.elf $(SF_DATA_PATH)/7ti1.bin
	@sh-elf-ld -EL -t -Ttext=07000000 -Tdata=07000000 build/src/assets/ast_7_ti_2/ast_7_ti_2.o -o build/src/assets/ast_7_ti_2/ast_7_ti_2.elf --no-check-sections --allow-shlib-undefined -r
	@sh-elf-ld -EL -t -Ttext=09000000 -Tdata=09000000 build/src/assets/ast_9_ti/ast_9_ti.o -o build/src/assets/ast_9_ti/ast_9_ti.elf --no-check-sections --allow-shlib-undefined -r
	@sh-elf-ld -EL -t -Ttext=08000000 -Tdata=08000000 build/src/assets/ast_8_ti/ast_8_ti.o -o build/src/assets/ast_8_ti/ast_8_ti.elf --no-check-sections --allow-shlib-undefined -r
	@sh-elf-ld -EL -t -R build/src/assets/ast_8_ti/ast_8_ti.elf -Ttext=09000000 build/src/assets/ast_9_ti/ast_9_ti.o -o build/src/assets/ast_9_ti/ast_9_ti.elf --no-check-sections
	@sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_7_ti_2/ast_7_ti_2.elf -Ttext=08000000 build/src/assets/ast_8_ti/ast_8_ti.o -o build/src/assets/ast_8_ti/ast_8_ti.elf --no-check-sections -z muldefs
	@sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_8_ti/ast_8_ti.elf -Ttext=07000000 build/src/assets/ast_7_ti_2/ast_7_ti_2.o -o build/src/assets/ast_7_ti_2/ast_7_ti_2.elf --no-check-sections -z muldefs
	@sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_8_ti/ast_8_ti.elf -R build/src/assets/ast_9_ti/ast_9_ti.elf -Ttext=0A000000 build/src/assets/ast_A_ti/ast_A_ti.o -o build/src/assets/ast_A_ti/ast_A_ti.elf --no-check-sections -z muldefs
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_A_ti/ast_A_ti.elf $(SF_DATA_PATH)/ati.bin
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_9_ti/ast_9_ti.elf $(SF_DATA_PATH)/9ti.bin
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_8_ti/ast_8_ti.elf $(SF_DATA_PATH)/8ti.bin
	@sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_7_ti_2/ast_7_ti_2.elf $(SF_DATA_PATH)/7ti2.bin

$(MUSIC_FILES): $(MUSIC_ARCHIVE)
	@mkdir -p $(SF_MUSIC_PATH)
	$(call print2,Unpacking music archive...)
	@tar xzf $(MUSIC_ARCHIVE) -C $(SF_MUSIC_PATH) --exclude='._*'
	$(call print2,Converting WAV to ADPCM...)
	@for f in $(SF_MUSIC_PATH)/*.wav; do \
		[ -e "$$f" ] || continue; \
		adpout="$${f%.wav}.adp"; \
		echo "Converting: $$f -> $$adpout"; \
		${KOS_BASE}/utils/wav2adpcm/wav2adpcm -n -i -t "$$f" "$$adpout"; \
	done
	@rm $(SF_MUSIC_PATH)/*.wav

$(MUSIC_ARCHIVE):
	$(call print2,Music archive not present -- downloading from $(MUSIC_ARCHIVE_URL)...)
	@mkdir -p $(SF_MUSIC_PATH)
	@if command -v wget >/dev/null 2>&1; then \
		wget -O "$(MUSIC_ARCHIVE)" "$(MUSIC_ARCHIVE_URL)"; \
	elif command -v curl >/dev/null 2>&1; then \
		curl -L -o "$(MUSIC_ARCHIVE)" "$(MUSIC_ARCHIVE_URL)"; \
	else \
		echo "Error: Can't download $(MUSIC_ARCHIVE) as neither wget nor curl were found on this system."; \
		echo "Download $(MUSIC_ARCHIVE) from $(MUSIC_ARCHIVE_URL)"; \
		echo "then place the file in this folder in order to proceed."; \
		exit 1; \
	fi

objects: $(O_FILES)

initted.touch:
	$(call print2,Project not initialized. Running init...)
	@$(MAKE) init

decompress: $(BASEROM)
	$(call print2,Decompressing ROM...)
	@$(PYTHON) $(COMPTOOL) $(DECOMPRESS_OPT) -dse $(COMPTOOL_DIR) -m $(MIO0) $(BASEROM) $(BASEROM_UNCOMPRESSED)

extract:
	@$(RM) -r asm/$(VERSION)/$(REV) bin/$(VERSION)/$(REV)
	$(call print2,Unifying yamls...)
	@$(CAT) yamls/$(VERSION)/$(REV)/header.yaml yamls/$(VERSION)/$(REV)/main.yaml yamls/$(VERSION)/$(REV)/assets.yaml yamls/$(VERSION)/$(REV)/overlays.yaml > $(SPLAT_YAML)
	$(call print2,Extracting...)
	@$(SPLAT) $(SPLAT_YAML)
	@rm -rf src/driverominit.c
	@rm -rf src/libc_sprintf.c
	@rm -rf src/libultra/gu/cosf.c
	@rm -rf src/libultra/gu/sinf.c
	@rm -rf src/libultra/gu/sqrtf.c
	@rm -rf src/libultra/debug/
	@rm -rf src/libultra/host/
	@rm -rf src/libultra/io/
	@rm -rf src/libultra/libc/
	@rm -rf src/libultra/os/
	@rm -rf src/libultra/rmon/

assets:
	$(call print2,Extracting assets from ROM...)
	@$(TORCH) code $(BASEROM_UNCOMPRESSED)
	@$(TORCH) header $(BASEROM_UNCOMPRESSED)
	@$(TORCH) modding export $(BASEROM_UNCOMPRESSED)

mod:
	@$(TORCH) modding import code $(BASEROM_UNCOMPRESSED)

distclean:
	@rm $(MUSIC_ARCHIVE)
	@rm $(BASEROM)

clean:
	@rm -f torch.hash.yml
	@git clean -fdx asm/$(VERSION)/$(REV)
	@git clean -fdx bin/$(VERSION)/$(REV)
	@git clean -fdx build/
	@git clean -fdx src/assets/
	@git clean -fdx include/assets/
	@git clean -fdx linker_scripts/$(VERSION)/$(REV)/*.ld
	@-rm -f $(ELF)
	@-rm -f $(BIN)
	@-rm -f $(CDI_CDR)
	@-rm -f $(CDI_ODE)
	@-rm -f $(FILES_ZIP)
	@-rm -f $(DSISO)
	@-rm -f initted.touch
	@-rm -rf $(SF_DATA_PATH)
	@-rm -rf $(SF_MUSIC_PATH)

format:
	@$(PYTHON) $(TOOLS)/format.py -j $(N_THREADS)

checkformat:
	@$(TOOLS)/check_format.sh -j $(N_THREADS)

context:
	$(call print2,Generating ctx.c...)
	@$(PYTHON) ./$(TOOLS)/m2ctx.py $(filter-out $@, $(MAKECMDGOALS))

#### Various Recipes ####

# PreProcessor
$(BUILD_DIR)/%.ld: %.ld
	$(call print,PreProcessor:,$<,$@)
	$(V)$(CPP) $(CPPFLAGS) $(BUILD_DEFINES) $(IINC) $< > $@

# Binary
$(BUILD_DIR)/%.o: %.bin
	$(call print,Binary:,$<,$@)
	$(V)$(OBJCOPY) -I binary -O elf32-big $< $@

# C
$(BUILD_DIR)/%.o: %.c
	$(call print,Compiling:,$<,$@)
	@$(CC_CHECK) $(CC_CHECK_FLAGS) $(IINC) -I $(dir $*) $(CHECK_WARNINGS) $(BUILD_DEFINES) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(C_DEFINES) $(MIPS_BUILTIN_DEFS) -o $@ $<
	$(V)$(CC) -c $(CFLAGS) $(BUILD_DEFINES) $(IINC) $(WARNINGS) $(MIPS_VERSION) $(ENDIAN) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(C_DEFINES) $(OPTFLAGS) -o $@ $<
	$(V)$(OBJDUMP_CMD)
	$(V)$(RM_MDEBUG)

# Patch ll.o
build/src/libultra/libc/ll.o: src/libultra/libc/ll.c
	$(call print,Patching:,$<,$@)
	@$(CC_CHECK) $(CC_CHECK_FLAGS) $(IINC) -I $(dir $*) $(CHECK_WARNINGS) $(BUILD_DEFINES) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(C_DEFINES) $(MIPS_BUILTIN_DEFS) -o $@ $<
	$(V)$(CC) -c $(CFLAGS) $(BUILD_DEFINES) $(IINC) $(WARNINGS) $(MIPS_VERSION) $(ENDIAN) $(COMMON_DEFINES) $(RELEASE_DEFINES) $(GBI_DEFINES) $(C_DEFINES) $(OPTFLAGS) -o $@ $<
	$(V)$(PYTHON) $(TOOLS)/set_o32abi_bit.py $@
	$(V)$(OBJDUMP_CMD)
	$(V)$(RM_MDEBUG)

-include $(DEP_FILES)

# Print target for debugging
print-% : ; $(info $* is a $(flavor $*) variable set to [$($*)]) @true

.PHONY: default all clean init extract format checkformat decompress assets context toolchain sf-data objects elf bin cdi-cdr cdi-ode files-zip dsiso
