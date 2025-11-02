SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
RUN_LIB_DIR = /oem
PLATFORM ?= RK3308

CFLAGS = -D_GNU_SOURCE -Wall -O2 -I$(INC_DIR)
CFLAGS += -D$(PLATFORM)
LDFLAGS = -Wl,-rpath,$(RUN_LIB_DIR) -laes -lpthread -ldl -lm -lstdc++ -L$(LIB_DIR)
TARGET = ep_aes

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

all: $(TARGET)
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
	$(STRIP) $(TARGET)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET)
