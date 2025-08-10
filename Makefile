#-----------------------------------------------------------------------------------------------------------#
# COMMON
#-----------------------------------------------------------------------------------------------------------#
ifeq ($(HOSTTYPE),)
HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif

CC = clang
CFLAGS_LIBRARIES = $(shell pkg-config --cflags libpcap)
CFLAGS_RELEASE   = -std=c23 -Wall -Wextra -Werror -I$(ALL_INCLUDE) $(CFLAGS_LIBRARIES) -isystem $(shell $(CC) -print-file-name=include) # Make cc include explicit
CFLAGS_DEBUG     = -g -O0 -fsanitize=address
CFLAGS_ALL       = $(CFLAGS_RELEASE) $(CFLAGS_DEBUG)


LDFLAGS_LIBRARIES = -pthread $(shell pkg-config --libs libpcap) -lc
LDFLAGS_RELEASE   = $(LDFLAGS_LIBRARIES)
LDFLAGS_DEBUG     = -g -fsanitize=address
LDFLAGS_ALL       = $(LDFLAGS_RELEASE) $(LDFLAGS_DEBUG)

MAKE = make

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin

#-----------------------------------------------------------------------------------------------------------#
# APP
#-----------------------------------------------------------------------------------------------------------#
# Config
APP_BIN = $(BIN_DIR)/ft_nmap
APP_INCLUDE = include -Isrc

APP_OBJ_DIR = $(OBJ_DIR)/src
APP_OBJ = $(patsubst $(APP_DIR)/%.c,$(APP_OBJ_DIR)/%.o,$(APP_SRC))

APP_DIR = src
APP_SRC = $(wildcard $(APP_DIR)/*.c)
# Rule
app: $(APP_BIN)
# Link
$(APP_BIN): $(APP_OBJ)
	$(CC) $(LDFLAGS_RELEASE) -o $@ $(APP_OBJ)
# Compile
$(APP_OBJ): $(APP_OBJ_DIR)/%.o: $(APP_DIR)/%.c
	$(CC) $(CFLAGS_RELEASE)  -c $< -o $@

#-----------------------------------------------------------------------------------------------------------#
# APP-DEBUG
#-----------------------------------------------------------------------------------------------------------#
# Config
APP_DEBUG_BIN = $(BIN_DIR)/ft_nmap.out
APP_DEBUG_OBJ_DIR = $(OBJ_DIR)/app-debug
APP_DEBUG_OBJ = $(patsubst $(APP_DIR)/%.c,$(APP_DEBUG_OBJ_DIR)/%.o,$(APP_SRC))
# Rule
app-debug: $(APP_DEBUG_BIN)
# Link
$(APP_DEBUG_BIN): $(APP_DEBUG_OBJ)
	$(CC) $(LDFLAGS_ALL) -o $@ $(APP_DEBUG_OBJ)
# Compile
$(APP_DEBUG_OBJ): $(APP_DEBUG_OBJ_DIR)/%.o: $(APP_DIR)/%.c
	$(CC) $(CFLAGS_ALL) -c $< -o $@

#-----------------------------------------------------------------------------------------------------------#
# DEBUG
#-----------------------------------------------------------------------------------------------------------#
# Config
DEBUG_BIN = $(BIN_DIR)/debug.out

DEBUG_DIR = debug
DEBUG_SRC = $(wildcard $(DEBUG_DIR)/*.c)

DEBUG_OBJ_DIR = $(OBJ_DIR)/debug
DEBUG_OBJ = $(patsubst $(DEBUG_DIR)/%.c,$(DEBUG_OBJ_DIR)/%.o,$(DEBUG_SRC))
# Rule
debug: $(DEBUG_BIN)
# Link
$(DEBUG_BIN): $(DEBUG_OBJ)
	$(CC) $(LDFLAGS_ALL) -o $@ $(DEBUG_OBJ)
# Compile
$(DEBUG_OBJ): $(DEBUG_OBJ_DIR)/%.o: $(DEBUG_DIR)/%.c
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@

#-----------------------------------------------------------------------------------------------------------#
# TEST
#-----------------------------------------------------------------------------------------------------------#
# Tests
TEST_BIN = $(BIN_DIR)/test.out

TEST_OBJ_DIR = $(OBJ_DIR)/test
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRC))

TEST_DIR = test
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)

# Test dependencies - need memory.c and error.c for tests
TEST_DEP_SRC = $(APP_DIR)/memory.c $(APP_DIR)/error.c
TEST_DEP_OBJ = $(patsubst $(APP_DIR)/%.c,$(APP_OBJ_DIR)/%.o,$(TEST_DEP_SRC))


# Config
TEST_BIN = $(BIN_DIR)/test.out

TEST_OBJ_DIR = $(OBJ_DIR)/test
TEST_OBJ = $(patsubst $(TEST_DIR)/%.c,$(TEST_OBJ_DIR)/%.o,$(TEST_SRC))

TEST_DIR = test
TEST_SRC = $(wildcard $(TEST_DIR)/*.c)

# Rule
test: $(TEST_BIN)
run-test: $(TEST_BIN)
	./$(TEST_BIN)
# Link
$(TEST_BIN): $(TEST_OBJ) $(TEST_DEP_OBJ)
	$(CC) $(LDFLAGS_ALL) -o $@ $(TEST_OBJ) $(TEST_DEP_OBJ)
# Compile
$(TEST_OBJ): $(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS_DEBUG) -c $< -o $@


#-----------------------------------------------------------------------------------------------------------#
# HOOK
#-----------------------------------------------------------------------------------------------------------#
# Config
# Hook malloc
HOOK_DIR = hook
HOOK_SRC = $(wildcard $(HOOK_DIR)/*.c)
HOOK_OBJ_DIR = $(OBJ_DIR)/hook
HOOK_OBJ = $(patsubst $(HOOK_DIR)/%.c,$(HOOK_OBJ_DIR)/%.o,$(HOOK_SRC))

# Hook shared library
ifeq ($(shell uname -s),Darwin)
# HOOK_LIB = $(BIN_DIR)/libhook.dylib
# HOOK_LDFLAGS = -dynamiclib -fPIC \
#                -current_version 1.0 -compatibility_version 1.0 \
#                -undefined dynamic_lookup
else
HOOK_LIB = $(BIN_DIR)/libhook.so
HOOK_LDFLAGS = -shared -fPIC
endif
# Hook library
# Link
$(HOOK_LIB): $(HOOK_OBJ)
	$(CC) $(HOOK_LDFLAGS) -o $@ $(HOOK_OBJ)
# Compile
$(HOOK_OBJ): $(HOOK_OBJ_DIR)/%.o: $(HOOK_DIR)/%.c
	$(CC) $(CFLAGS_RELEASE) -c $< -o $@

#-----------------------------------------------------------------------------------------------------------#
# ALL
#-----------------------------------------------------------------------------------------------------------#
# Config
ALL_INCLUDE = $(APP_INCLUDE)
ALL_BIN = $(APP_BIN) $(APP_DEBUG_BIN) $(DEBUG_BIN) $(TEST_BIN) $(HOOK_LIB)
ALL_OBJ = $(APP_OBJ) $(APP_DEBUG_OBJ) $(DEBUG_OBJ) $(TEST_OBJ) $(HOOK_OBJ)
# Rule
all: $(ALL_BIN)

clean:
	rm -rf $(ALL_BIN) $(ALL_OBJ)
	find build -mindepth 1 -not -name '.gitkeep' -not -type d -delete
	find build -mindepth 1 -type d -empty -delete

re: clean
	$(MAKE) -j all

#-----------------------------------------------------------------------------------------------------------#
# VM
#-----------------------------------------------------------------------------------------------------------#
vm-up:
	@if [ -d "$$HOME/goinfre" ] && [ -d "$$HOME/sgoinfre" ]; then \
		.cluster/cluster2nix.sh --cmd "nix run --impure .#vm"; \
	else \
		nix run --impure .#vm; \
	fi

vm-clean:
	pkill -f "2223" || true
	lsof -ti:2223 | xargs -r kill -9 || true
	rm -rf ./nixos.qcow2 ./result

vm-restart: vm-clean vm-up
.PHONY: all app debug test clean re run-test vm-up vm-down vm-restart app-debug

#-----------------------------------------------------------------------------------------------------------#