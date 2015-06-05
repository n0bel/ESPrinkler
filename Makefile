#############################################################
#
# Root Level Makefile
#
# (c) by CHERTS <sleuthhound@gmail.com>
#
#############################################################

BUILD_BASE	= build
FW_BASE		= firmware

# Base directory for the compiler
XTENSA_TOOLS_ROOT ?= c:/Espressif/xtensa-lx106-elf/bin

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= c:/Espressif/esp_iot_sdk_v1.0.1

# esptool path and port
SDK_TOOLS	?= c:/Espressif/utils
ESPTOOL		?= $(SDK_TOOLS)/esptool.exe
ESPPORT		?= COM2
ESPBAUD		?= 115200

# BOOT = none
# BOOT = old - boot_v1.1
# BOOT = new - boot_v1.3+
BOOT?=none
# APP = 0 - eagle.flash.bin + eagle.irom0text.bin
# APP = 1 - user1.bin
# APP = 2 - user2.bin
APP?=0
# SPI_SPEED = 20MHz, 26.7MHz, 40MHz, 80MHz
SPI_SPEED?=40
# SPI_MODE: QIO, QOUT, DIO, DOUT
SPI_MODE?=QIO
# SPI_SIZE: 256KB, 512KB, 1024KB, 2048KB, 4096KB
SPI_SIZE?=512


# --------------- esphttpd config options ---------------

# If GZIP_COMPRESSION is set to "yes" then the static css, js, and html files will be compressed with gzip before added to the espfs image
# and will be served with gzip Content-Encoding header.
# This could speed up the downloading of these files, but might break compatibility with older web browsers not supporting gzip encoding
# because Accept-Encoding is simply ignored. Enable this option if you have large static files to serve (for e.g. JQuery, Twitter bootstrap)
# By default only js, css and html files are compressed.
# If you have text based static files with different extensions what you want to serve compressed then you will need to add the extension to the following places:
# - Add the extension to this Makefile at the webpages.espfs target to the find command
# - Add the extension to the gzippedFileTypes array in the user/httpd.c file
#
# Adding JPG or PNG files (and any other compressed formats) is not recommended, because GZIP compression does not works effectively on compressed files.

#Static gzipping is disabled by default.
GZIP_COMPRESSION ?= no

# If COMPRESS_W_YUI is set to "yes" then the static css and js files will be compressed with yui-compressor
# This option works only when GZIP_COMPRESSION is set to "yes"
# http://yui.github.io/yuicompressor/
#Disabled by default.
COMPRESS_W_YUI ?= no
YUI-COMPRESSOR ?= /usr/bin/yui-compressor

#If USE_HEATSHRINK is set to "yes" then the espfs files will be compressed with Heatshrink and decompressed
#on the fly while reading the file. Because the decompression is done in the esp8266, it does not require
#any support in the browser.
USE_HEATSHRINK ?= no

#Position and maximum length of espfs in flash memory
ESPFS_POS ?= 0x12000
ESPFS_SIZE ?= 0x2E000

# -------------- End of esphttpd config options -------------


ifeq ($(BOOT), new)
    boot = new
else
    ifeq ($(BOOT), old)
        boot = old
    else
        boot = none
    endif
endif

ifeq ($(APP), 1)
    app = 1
else
    ifeq ($(APP), 2)
        app = 2
    else
        app = 0
    endif
endif

ifeq ($(SPI_SPEED), 26.7)
    freqdiv = 1
	flashimageoptions = -ff 26m
else
    ifeq ($(SPI_SPEED), 20)
        freqdiv = 2
        flashimageoptions = -ff 20m
    else
        ifeq ($(SPI_SPEED), 80)
            freqdiv = 15
			flashimageoptions = -ff 80m
        else
            freqdiv = 0
			flashimageoptions = -ff 40m
        endif
    endif
endif

ifeq ($(SPI_MODE), QOUT)
    mode = 1
	flashimageoptions += -fm qout
else
    ifeq ($(SPI_MODE), DIO)
        mode = 2
		flashimageoptions += -fm dio
    else
        ifeq ($(SPI_MODE), DOUT)
            mode = 3
			flashimageoptions += -fm dout
        else
            mode = 0
			flashimageoptions += -fm qio
        endif
    endif
endif

# flash larger than 1024KB only use 1024KB to storage user1.bin and user2.bin
ifeq ($(SPI_SIZE), 256)
    size = 1
    flash = 256
	flashimageoptions += -fs 2m
else
    ifeq ($(SPI_SIZE), 1024)
        size = 2
        flash = 1024
		flashimageoptions += -fs 8m
    else
        ifeq ($(SPI_SIZE), 2048)
            size = 3
            flash = 1024
			flashimageoptions += -fs 16m
        else
            ifeq ($(SPI_SIZE), 4096)
                size = 4
                flash = 1024
				flashimageoptions += -fs 32m
            else
                size = 0
                flash = 512
				flashimageoptions += -fs 4m
            endif
        endif
    endif
endif

ifeq ($(flash), 512)
  ifeq ($(app), 1)
    addr = 0x01000
  else
    ifeq ($(app), 2)
      addr = 0x41000
    endif
  endif
else
  ifeq ($(flash), 1024)
    ifeq ($(app), 1)
      addr = 0x01000
    else
      ifeq ($(app), 2)
        addr = 0x81000
      endif
    endif
  endif
endif

# name for the target project
TARGET		= app

# which modules (subdirectories) of the project to include in compiling
MODULES		= driver user httpd espfs
EXTRA_INCDIR    = include $(SDK_BASE)/../include espfs .

# libraries used in this project, mainly provided by the SDK
LIBS		= c gcc hal phy pp net80211 lwip wpa main

# compiler flags using during compilation of source files
CFLAGS		= -Os -g -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH -DESPFS_POS=$(ESPFS_POS) -DESPFS_SIZE=$(ESPFS_SIZE)

# linker flags used to generate the main object file
LDFLAGS		= -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

# linker script used for the above linkier step
LD_SCRIPT	= eagle.app.v6.ld

ifneq ($(boot), none)
ifneq ($(app),0)
	LD_SCRIPT = eagle.app.v6.$(boot).$(flash).app$(app).ld
	BIN_NAME = user$(app).$(flash).$(boot)
endif
else
    app = 0
endif

# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-gcc
OBJCOPY := $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-objcopy
OBJDUMP := $(XTENSA_TOOLS_ROOT)/xtensa-lx106-elf-objdump

# no user configurable options below here
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
OBJ		:= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
LIBS		:= $(addprefix -l,$(LIBS))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)
TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

LD_SCRIPT	:= $(addprefix -T$(SDK_BASE)/$(SDK_LDDIR)/,$(LD_SCRIPT))

INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

vpath %.c $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean cleanfs

all: checkdirs $(TARGET_OUT)

$(TARGET_OUT): $(APP_AR)
	$(vecho) "LD $@"
	$(Q) $(LD) -L$(SDK_LIBDIR) $(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $(LIBS) $(APP_AR) -Wl,--end-group -o $@
	$(vecho) "------------------------------------------------------------------------------"
	$(vecho) "Section info:"
	$(Q) $(OBJDUMP) -h -j .data -j .rodata -j .bss -j .text -j .irom0.text $@
	$(vecho) "------------------------------------------------------------------------------"
	$(vecho) "Section info:"
	$(Q) $(SDK_TOOLS)/memanalyzer.exe $(OBJDUMP).exe $@
	$(vecho) "------------------------------------------------------------------------------"
	$(vecho) "Run objcopy, please wait..."
	$(Q) $(OBJCOPY) --only-section .text -O binary $@ eagle.app.v6.text.bin
	$(Q) $(OBJCOPY) --only-section .data -O binary $@ eagle.app.v6.data.bin
	$(Q) $(OBJCOPY) --only-section .rodata -O binary $@ eagle.app.v6.rodata.bin
	$(Q) $(OBJCOPY) --only-section .irom0.text -O binary $@ eagle.app.v6.irom0text.bin
	$(vecho) "objcopy done"
	$(vecho) "Run gen_appbin.exe"
ifeq ($(app), 0)
	$(Q) $(SDK_TOOLS)/gen_appbin.exe $@ 0 $(mode) $(freqdiv) $(size)
	$(Q) mv eagle.app.flash.bin $(FW_BASE)/eagle.flash.bin
	$(Q) mv eagle.app.v6.irom0text.bin $(FW_BASE)/eagle.irom0text.bin
	$(Q) rm eagle.app.v6.*
	$(vecho) "No boot needed."
	$(vecho) "Generate eagle.flash.bin and eagle.irom0text.bin successully in folder $(FW_BASE)."
	$(vecho) "eagle.flash.bin-------->0x00000"
	$(vecho) "eagle.irom0text.bin---->0x40000"
else
    ifeq ($(boot), new)
		$(Q) $(SDK_TOOLS)/gen_appbin.exe $@ 2 $(mode) $(freqdiv) $(size)
		$(vecho) "Support boot_v1.3 and +"
    else
		$(Q) $(SDK_TOOLS)/gen_appbin.exe $@ 1 $(mode) $(freqdiv) $(size)
		$(vecho) "Support boot_v1.1 and +"
    endif
	$(Q) mv eagle.app.flash.bin $(FW_BASE)/upgrade/$(BIN_NAME).bin
	$(Q) rm eagle.app.v6.*
	$(vecho) "Generate $(BIN_NAME).bin successully in folder $(FW_BASE)/upgrade."
	$(vecho) "boot_v1.x.bin------->0x00000"
	$(vecho) "$(BIN_NAME).bin--->$(addr)"
endif
	$(vecho) "Done"

$(APP_AR): $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $^

checkdirs: $(BUILD_DIR) $(FW_BASE)

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(FW_BASE):
	$(Q) test -d $@ || mkdir -p $@
	$(Q) test -d $@/upgrade || mkdir -p $@/upgrade

flashonefile: all
	$(OBJCOPY) --only-section .text -O binary $(TARGET_OUT) eagle.app.v6.text.bin
	$(OBJCOPY) --only-section .data -O binary $(TARGET_OUT) eagle.app.v6.data.bin
	$(OBJCOPY) --only-section .rodata -O binary $(TARGET_OUT) eagle.app.v6.rodata.bin
	$(OBJCOPY) --only-section .irom0.text -O binary $(TARGET_OUT) eagle.app.v6.irom0text.bin
	$(SDK_TOOLS)/gen_appbin_old.exe $(TARGET_OUT) v6
	$(SDK_TOOLS)/gen_flashbin.exe eagle.app.v6.flash.bin eagle.app.v6.irom0text.bin 0x40000
	rm -f eagle.app.v6.data.bin
	rm -f eagle.app.v6.flash.bin
	rm -f eagle.app.v6.irom0text.bin
	rm -f eagle.app.v6.rodata.bin
	rm -f eagle.app.v6.text.bin
	rm -f eagle.app.sym
	mv eagle.app.flash.bin $(FW_BASE)/
	$(vecho) "No boot needed."
	$(vecho) "Generate eagle.app.flash.bin successully in folder $(FW_BASE)."
	$(vecho) "eagle.app.flash.bin-------->0x00000"
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 $(FW_BASE)/eagle.app.flash.bin

flashboot: all #flashinit
ifeq ($(boot), new)
	$(vecho) "Flash boot_v1.3 and +"
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 $(SDK_BASE)/bin/boot_v1.3\(b3\).bin
endif
ifeq ($(boot), old)
	$(vecho) "Flash boot_v1.1 and +"
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 $(SDK_BASE)/bin/boot_v1.1.bin
endif
ifeq ($(boot), none)
	$(vecho) "No boot needed."
endif

flash: all
ifeq ($(app), 0) 
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 $(FW_BASE)/eagle.flash.bin 0x40000 $(FW_BASE)/eagle.irom0text.bin
else
ifeq ($(boot), none)
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x00000 $(FW_BASE)/eagle.flash.bin 0x40000 $(FW_BASE)/eagle.irom0text.bin
else
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) $(addr) $(FW_BASE)/upgrade/$(BIN_NAME).bin
endif
endif

cleanfs:
	$(Q) rm -rf $(FW_BASE)/webpages.espfs


$(FW_BASE)/webpages.espfs: html/ html/wifi/ espfs/mkespfsimage/mkespfsimage
	$(Q) test -d $(FW_BASE) || mkdir $(FW_BASE)
ifeq ("$(COMPRESS_W_YUI)","yes")
	$(Q) rm -rf html_compressed;
	$(Q) cp -r html html_compressed;
	$(Q) echo "Compression assets with yui-compressor. This may take a while..."
	$(Q) for file in `find html_compressed -type f -name "*.js"`; do $(YUI-COMPRESSOR) --type js $$file -o $$file; done
	$(Q) for file in `find html_compressed -type f -name "*.css"`; do $(YUI-COMPRESSOR) --type css $$file -o $$file; done
	$(Q) awk "BEGIN {printf \"YUI compression ratio was: %.2f%%\\n\", (`du -b -s html_compressed/ | sed 's/\([0-9]*\).*/\1/'`/`du -b -s html/ | sed 's/\([0-9]*\).*/\1/'`)*100}"

# mkespfsimage will compress html, css and js files with gzip by default if enabled
# override with -g cmdline parameter
	$(Q) cd html_compressed; find  | ../espfs/mkespfsimage/mkespfsimage > ../$(FW_BASE)/webpages.espfs; cd ..;
else
	$(Q) cd html; find | ../espfs/mkespfsimage/mkespfsimage > ../$(FW_BASE)/webpages.espfs; cd ..
endif
	$(vecho) $$(stat -c '%s' $(FW_BASE)/webpages.espfs) used of $$(( $(ESPFS_SIZE) )) $$(expr $$(stat -c '%s' $(FW_BASE)/webpages.espfs) '*' 100 / $$(( $(ESPFS_SIZE) )))% full

flashinit:
	$(vecho) "Flash init data:"
	$(vecho) "Clear old settings (EEP area):"
	$(vecho) "clear_eep.bin-------->0x79000"
	$(vecho) "Default config (Clear SDK settings):"
	$(vecho) "blank.bin-------->0x7E000"
	$(vecho) "esp_init_data_default.bin-------->0x7C000"
	$(ESPTOOL) -p $(ESPPORT) -b $(ESPBAUD) write_flash $(flashimageoptions) 0x79000 $(SDK_BASE)/bin/clear_eep.bin 0x7c000 $(SDK_BASE)/bin/esp_init_data_default.bin 0x7e000 $(SDK_BASE)/bin/blank.bin

espfs/mkespfsimage/mkespfsimage: espfs/mkespfsimage/
	$(Q) $(MAKE) -C espfs/mkespfsimage USE_HEATSHRINK="$(USE_HEATSHRINK)" GZIP_COMPRESSION="$(GZIP_COMPRESSION)"

htmlflash: $(FW_BASE)/webpages.espfs checkdirs
	$(Q) if [ $$(stat -c '%s' $(FW_BASE)/webpages.espfs) -gt $$(( $(ESPFS_SIZE) )) ]; then echo "webpages.espfs too big!"; false; fi
	$(ESPTOOL) --port $(ESPPORT) --baud $(ESPBAUD) write_flash $(ESPFS_POS) $(FW_BASE)/webpages.espfs


rebuild: clean all


clean:
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) rm -rf $(BUILD_DIR)
	$(Q) rm -rf $(BUILD_BASE)
	$(Q) rm -rf $(FW_BASE)
	$(Q) make -C espfs/mkespfsimage/ clean
ifeq ("$(COMPRESS_W_YUI)","yes")
	$(Q) rm -rf html_compressed
endif
$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))
