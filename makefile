CC = gcc
CFLAGS = -lm
SRC_DIR = src
BIN_DIR = bin

SPECIAL_SRC = $(SRC_DIR)/reference-paper-algo.c
SPECIAL_BIN = $(BIN_DIR)/REF_PAPER_ALGO
SRCS = $(wildcard $(SRC_DIR)/*.c)
GENERIC_SRCS = $(filter-out $(SPECIAL_SRC), $(SRCS))
GENERIC_BINS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%, $(GENERIC_SRCS))
EXECS = $(GENERIC_BINS) $(SPECIAL_BIN)
all: $(EXECS)
$(BIN_DIR)/%: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<
$(SPECIAL_BIN): $(SPECIAL_SRC) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

outputs:
	./bench.sh

clean:
	rm -f $(BIN_DIR)/*

.PHONY: all clean

