ifeq ($(HOSTTYPE),)
HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

CC = clang
# Pkg-config gives us the proper include/lib flags for libpcap –
# `pthread` has no .pc file on most Linux systems, so we use the
# compiler-provided `-pthread` switch instead.
CFLAGS_LIBRARIES = $(shell pkg-config --cflags libpcap)
CFLAGS_RELEASE   = -std=c23 -Wall -Wextra -Werror -I$(ALL_INCLUDE) $(CFLAGS_LIBRARIES) -fblocks -isystem $(shell $(CC) -print-file-name=include) # Make cc include explicit
CFLAGS_DEBUG     = $(CFLAGS_RELEASE) -g -O0 -fsanitize=address
LDFLAGS_LIBRARIES = -pthread $(shell pkg-config --libs libpcap)
LDFLAGS_RELEASE   = $(LDFLAGS_LIBRARIES)
LDFLAGS_DEBUG     = $(LDFLAGS_RELEASE) -g -fsanitize=address

MAKE = make

# Build Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Debug
DEBUG_BIN = $(BIN_DIR)/debug.out

DEBUG_DIR = debug
DEBUG_SRC = $(wildcard $(DEBUG_DIR)/*.c)

DEBUG_OBJ_DIR = $(OBJ_DIR)/debug
DEBUG_OBJ = $(patsubst $(DEBUG_DIR)/%.c,$(DEBUG_OBJ_DIR)/%.o,$(DEBUG_SRC))

# Tests
TEST_BIN = $(BIN_DIR)/test.out

TEST_OBJ_DIR = $(OBJ_DIR)/test
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRC))

TEST_DIR = test
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)

# Test dependencies - need memory.c and error.c for tests
TEST_DEP_SRC = $(APP_DIR)/memory.c $(APP_DIR)/error.c
TEST_DEP_OBJ = $(patsubst $(APP_DIR)/%.c,$(APP_OBJ_DIR)/%.o,$(TEST_DEP_SRC))

# App
APP_BIN = $(BIN_DIR)/ft_nmap
APP_INCLUDE = include

APP_OBJ_DIR = $(OBJ_DIR)/src
APP_OBJ = $(patsubst $(APP_DIR)/%.c,$(APP_OBJ_DIR)/%.o,$(APP_SRC))

APP_DIR = src
APP_SRC = $(wildcard $(APP_DIR)/*.c)

# All
ALL_INCLUDE = $(APP_INCLUDE)
ALL_BIN = $(APP_BIN) $(DEBUG_BIN) $(TEST_BIN)
ALL_OBJ = $(APP_OBJ) $(DEBUG_OBJ) $(TEST_OBJ)

#-----------------------------------------------------------------------------------------------------------#
# Rules
#-----------------------------------------------------------------------------------------------------------#
app: $(APP_BIN)


# App
# Link
$(APP_BIN): $(APP_OBJ)
	$(CC) $(LDFLAGS_RELEASE) -o $@ $(APP_OBJ)
# Compile
$(APP_OBJ): $(APP_OBJ_DIR)/%.o: $(APP_DIR)/%.c
	$(CC) $(CFLAGS_RELEASE)  -c $< -o $@

# Debug
# Link
$(DEBUG_BIN): $(DEBUG_OBJ)
	$(CC) $(LDFLAGS_DEBUG) -o $@ $(DEBUG_OBJ)
# Compile
$(DEBUG_OBJ): $(DEBUG_OBJ_DIR)/%.o: $(DEBUG_DIR)/%.c
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

# Tests
# Link
$(TEST_BIN): $(TEST_OBJ) $(TEST_DEP_OBJ)
	$(CC) $(LDFLAGS_DEBUG) -o $@ $(TEST_OBJ) $(TEST_DEP_OBJ)
# Compile
$(TEST_OBJ): $(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@


all:  $(ALL_BIN)


debug: $(DEBUG_BIN)

test: $(TEST_BIN)

run-app: $(APP_BIN)
	./$(APP_BIN)

run-test: $(TEST_BIN)
	./$(TEST_BIN)

debug-test: $(TEST_BIN)
	gdb $(TEST_BIN)

clean:
	rm -rf $(ALL_BIN) $(ALL_OBJ)

re: clean
	$(MAKE) all

vm-up:
	@if [ -d "$$HOME/goinfre" ] && [ -d "$$HOME/sgoinfre" ]; then \
		.cluster/cluster2nix.sh --cmd "nix run --impure .#vm"; \
	elif grep -qEi "(Microsoft|WSL)" /proc/version 2>/dev/null; then \
		nix --extra-experimental-features 'nix-command flakes' run --impure .#vm; \
	else \
		nix run --impure .#vm; \
	fi

vm-clean:
	rm -rf ./nixos.qcow2 ./result

vm-restart: vm-clean vm-up

.PHONY: all app debug test clean re run-test vm-up vm-clean vm-restart flake-update flake-show flake-check vm-build vm-run dev-shell

#-----------------------------------------------------------------------------------------------------------#
