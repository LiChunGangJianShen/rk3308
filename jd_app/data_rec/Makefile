SRC_DIR = src
INC_DIR = include
LIB_DIR = lib

CFLAGS = -Wall -O2 -I$(INC_DIR)/
LDFLAGS = -lpthread -ldl -lm -L$(LIB_DIR)/
TARGET = data_rec

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst %.c, $(SRC_DIR)/%.o, $(notdir $(SRC)))

# CC=arm-linux-gnueabihf-gcc
# STRIP=arm-linux-gnueabihf-strip
CC=aarch64-linux-gnu-gcc
STRIP=aarch64-linux-gnu-strip

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(CFLAGS)  $(LDFLAGS)
	$(STRIP) $(TARGET)
	cp -f $(TARGET) ../../buildroot/board/rockchip/rk3308/fs-overlay/usr/bin/

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

.PHONY:clean
clean:
	rm -f $(OBJ) $(TARGET)
