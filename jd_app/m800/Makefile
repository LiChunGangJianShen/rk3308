PWD = $(shell pwd)
SRC_DIR = $(PWD)/src
INC_DIR = $(PWD)/include
LIB_DIR = $(PWD)/lib

RADIR = /oem
ENDLIB = 1

PLATFORM ?= RK3308

CFLAGS = -D_GNU_SOURCE -Wall -O2 -I$(INC_DIR)

ifeq ($(ENDLIB), 1)
LDFLAGS = -Wl,-rpath,$(RADIR) -lVHDM800_16K_7MIC -lasound -lpthread -latomic -ldl -lm -lstdc++ -L$(LIB_DIR)
else
LDFLAGS = -lasound -lpthread -latomic -ldl -lm -lstdc++ -L$(LIB_DIR)
endif
TARGET = $(PWD)/fs-overlay/usr/bin/jdapp

BUILD_TIME := $(shell date "+%Y-%m-%d %H:%M:%S")
APP_VERSION := V1.0.0

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst %.c, $(SRC_DIR)/%.o, $(notdir $(SRC)))

RK3308_CC=aarch64-linux-gnu-gcc
RK3308_STRIP=aarch64-linux-gnu-strip
RK3562_CC=aarch64-none-linux-gnu-gcc
RK3562_STRIP=aarch64-none-linux-gnu-strip

ifeq ($(PLATFORM), RK3308)
CC=$(RK3308_CC)
STRIP=$(RK3308_STRIP)
else ifeq ($(PLATFORM), RK3562)
CC=$(RK3562_CC)
STRIP=$(RK3562_STRIP)
else
$(error Unsupported platform: $(PLATFORM))
endif

all: pre_build $(TARGET) install

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
	$(STRIP) $(TARGET)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:clean pre_build install
install:
	@cp -rf $(PWD)/fs-overlay $(PWD)/../../buildroot/board/rockchip/rk3308/
	@rm -rf $(PWD)/../../device/rockchip/rk3308/oem_m800/*.so
	@cp -rf $(LIB_DIR)/libVHDM800_16K_7MIC.so $(PWD)/../../device/rockchip/rk3308/oem_m800/
pre_build:
	@echo "\n$(BUILD_TIME)\n"
	@echo "#define BUILD_TIME \"$(BUILD_TIME)\"" > include/build_time.h
	@echo "app-version: $(APP_VERSION)\n"
	@echo "#define APP_VERSION \"$(APP_VERSION)\"" > include/app_version.h
clean:
	rm -f $(OBJ) $(TARGET)
