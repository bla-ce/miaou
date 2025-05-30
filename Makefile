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

all: $(INCLUDES) $(SRC_DIR)/$(PROGRAM_NAME).s
	mkdir build -p
	nasm -f elf64 -o $(BUILD_DIR)/$(PROGRAM_NAME).o $(SRC_DIR)/$(PROGRAM_NAME).s \
		-g -w+all  \
		-I$(INC_DIR)/ \
		-I$(INC_DIR)/$(UTILS_DIR) \
		-I$(INC_DIR)/$(MALLOC_DIR) \
		-I$(INC_DIR)/$(BOEUF_DIR) \
		-I$(INC_DIR)/$(MIAOU_DIR)
	ld -o $(BUILD_DIR)/$(PROGRAM_NAME) $(BUILD_DIR)/$(PROGRAM_NAME).o

test: main.s $(INCLUDES)
	nasm -f elf64 -o main.o main.s \
		-g -w+all  \
		-I$(INC_DIR)/ \
		-I$(INC_DIR)/$(UTILS_DIR) \
		-I$(INC_DIR)/$(MALLOC_DIR) \
		-I$(INC_DIR)/$(BOEUF_DIR) \
		-I$(INC_DIR)/$(MIAOU_DIR)
	ld -o main main.o

.PHONY: clean
clean:
	rm -f $(PROGRAM_NAME) $(PROGRAM_NAME).o main main.o

