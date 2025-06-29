INC_DIR = inc
MALLOC_DIR = malloc
UTILS_DIR = utils
MIAOU_DIR = miaou
BOEUF_DIR = boeuf
BUILD_DIR	= build
SRC_DIR	= src
INCLUDES = $(shell find $(INC_DIR) -type f -name '*.inc')

ifndef PROGRAM_NAME
$(error PROGRAM_NAME is not set. Please pass it on the command line, e.g., make PROGRAM_NAME=client)
endif

INCLUDE_FLAGS = -I$(INC_DIR)/ \
	-I$(INC_DIR)/$(UTILS_DIR) \
	-I$(INC_DIR)/$(MALLOC_DIR) \
	-I$(INC_DIR)/$(BOEUF_DIR) \
	-I$(INC_DIR)/$(MIAOU_DIR)

BASE_FLAGS = -felf64 -w+all -g

all: $(INCLUDES) $(SRC_DIR)/$(PROGRAM_NAME).s
	mkdir build -p
	nasm -o $(BUILD_DIR)/$(PROGRAM_NAME).o $(SRC_DIR)/$(PROGRAM_NAME).s \
		$(BASE_FLAGS) $(INCLUDE_FLAGS)
	ld -o $(BUILD_DIR)/$(PROGRAM_NAME) $(BUILD_DIR)/$(PROGRAM_NAME).o

.PHONY: clean
clean:
	rm -f $(PROGRAM_NAME) $(PROGRAM_NAME).o main main.o

