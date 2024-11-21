SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
BIN_DIR = bin
ETC_DIR = etc
RUN_ALGO_LIB_DIR = /oem

TARGET = jdapp
CFLAGS = -D_GNU_SOURCE -Wall -O2 -I$(INC_DIR)
LDFLAGS = -Wl,-rpath,$(RUN_ALGO_LIB_DIR) -ljdaudio -lpthread -lasound -ldl -lm -lstdc++ -L$(LIB_DIR)

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst %.c, $(SRC_DIR)/%.o, $(notdir $(SRC)))

VERSION = V1.00
BUILD_TIME = $(shell date "+%Y-%m-%d %H:%M:%S")

# CC = gcc
# STRIP = strip
CC = aarch64-linux-gnu-gcc
STRIP = aarch64-linux-gnu-strip

all: prebuild $(TARGET) 
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(CFLAGS) $(LDFLAGS)
	$(STRIP) $(TARGET)
	cp -f $(TARGET) $(BIN_DIR)
	cp -f $(TARGET) ../../buildroot/board/rockchip/rk3308/fs-overlay/usr/bin/
	cp -f $(LIB_DIR)/libjdaudio.so ../../device/rockchip/rk3308/oem_jd100/
	cp -f $(ETC_DIR)/.usb_config ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S41sysinit ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S50usbdevice ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S90jdapp ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:prebuild clean
prebuild:
	echo "#define VERSION \"$(VERSION)\"" >$(INC_DIR)/version.h
	echo "#define BUILD_TIME	\"$(BUILD_TIME)\""	>$(INC_DIR)/build_time.h
clean:
	rm -rf $(OBJ) $(TARGET)