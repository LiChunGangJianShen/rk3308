SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
ETC_DIR = etc
ALGO_LIB = $(LIB_DIR)/libjd_feedbackdestroy-251027-2.2.14.a
RUN_ALGO_LIB_DIR = /oem
USE_DYNAMIC_LIB	= 1
EN_AES_TEST = 0

CFLAGS = -D_GNU_SOURCE -Wall -O2 -I$(INC_DIR)
CXXFLAGS = -fpermissive
ifeq ($(USE_DYNAMIC_LIB), 1)
LDFLAGS = -Wl,-rpath,$(RUN_ALGO_LIB_DIR) -ljd_feedbackdestroy -lasound -lpthread -ldl -lm -lstdc++ -L$(LIB_DIR)
else
LDFLAGS = -lasound -lpthread -ldl -lm -lstdc++ $(ALGO_LIB) -L$(LIB_DIR)
endif
ifeq ($(EN_AES_TEST), 1)
AES_LDFLAGS = -Wl,-rpath,$(RUN_ALGO_LIB_DIR) -laes
AES_CFLAGS = -DAES_TEST
endif
TARGET = jdapp
BUILD_TIME := $(shell date "+%Y-%m-%d %H:%M:%S")
APP_VERSION := V1.7

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst %.c, $(SRC_DIR)/%.o, $(notdir $(SRC)))

# CC=arm-linux-gnueabihf-gcc
# STRIP=arm-linux-gnueabihf-strip
CC=aarch64-linux-gnu-gcc
STRIP=aarch64-linux-gnu-strip

all: pre_build $(TARGET)

$(TARGET): $(OBJ)
ifeq ($(EN_AES_TEST), 1)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS) $(AES_LDFLAGS)
else
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
endif
	$(STRIP) $(TARGET)
	cp -f $(TARGET) ../../buildroot/board/rockchip/rk3308/fs-overlay/usr/bin/
ifeq ($(USE_DYNAMIC_LIB), 1)
	cp -f $(LIB_DIR)/libjd_feedbackdestroy.so ../../device/rockchip/rk3308/oem_h20/
else
	rm -f ../../device/rockchip/rk3308/oem_h20/libjd_feedbackdestroy.so
endif
	cp -f $(ETC_DIR)/asound.conf ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/
	cp -f $(ETC_DIR)/.usb_config ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S41sysinit ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S50usbdevice ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/
	cp -f $(ETC_DIR)/S90jdapp ../../buildroot/board/rockchip/rk3308/fs-overlay/etc/init.d/

%.o: %.c
ifeq ($(EN_AES_TEST), 1)
	$(CC) -c $< -o $@ $(CFLAGS) $(AES_CFLAGS)
else
	$(CC) -c $< -o $@ $(CFLAGS)
endif

.PHONY:clean pre_build

pre_build:
	@echo "\n$(BUILD_TIME)\n"
	@echo "#define BUILD_TIME \"$(BUILD_TIME)\"" > include/build_time.h
	@echo "app-version: $(APP_VERSION)\n"
	@echo "#define APP_VERSION \"$(APP_VERSION)\"" > include/app_version.h

clean:
	rm -f $(OBJ) $(TARGET)
