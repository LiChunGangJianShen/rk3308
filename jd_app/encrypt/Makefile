SRC_DIR = src
INC_DIR = include
LIB_DIR = lib
ALGO_LIB = $(LIB_DIR)/GEN_A05_250317_01.a

CFLAGS = -D_GNU_SOURCE -Wall -O2 -I$(INC_DIR)
CXXFLAGS = -fpermissive
LDFLAGS = -lpthread -ldl -lm -lstdc++ $(ALGO_LIB) -L$(LIB_DIR)
TARGET = encrypt
BUILD_TIME := $(shell date "+%Y-%m-%d %H:%M:%S")
APP_VERSION := V1.0

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst %.c, $(SRC_DIR)/%.o, $(notdir $(SRC)))

# CC=arm-linux-gnueabihf-gcc
# STRIP=arm-linux-gnueabihf-strip
CC=aarch64-linux-gnu-gcc
STRIP=aarch64-linux-gnu-strip

all: pre_build $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LDFLAGS)
	$(STRIP) $(TARGET)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:clean pre_build

pre_build:
	@echo "\n$(BUILD_TIME)\n"
	@echo "version: $(APP_VERSION)\n"

clean:
	rm -f $(OBJ) $(TARGET)
