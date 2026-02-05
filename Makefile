# PlumbrC - High-Performance Log Redaction Pipeline
# Makefile

CC = gcc
AR = ar

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
LIB_DIR = $(BUILD_DIR)/lib
BIN_DIR = $(BUILD_DIR)/bin

# Output
TARGET = plumbr
LIBRARY = libplumbr.a

# Sources
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
LIB_OBJS = $(filter-out $(OBJ_DIR)/main.o,$(OBJS))

# Compiler flags
CFLAGS = -std=c11 -I$(INC_DIR) -D_GNU_SOURCE
LDFLAGS = -lpcre2-8 -lpthread

# Warning flags (strict)
WARNINGS = -Wall -Wextra -Werror -Wpedantic \
           -Wformat=2 -Wno-unused-parameter \
           -Wshadow -Wwrite-strings \
           -Wstrict-prototypes -Wold-style-definition \
           -Wredundant-decls -Wnested-externs \
           -Wmissing-include-dirs

# Optimization flags
OPT_FLAGS = -O3 -march=native -flto -fomit-frame-pointer \
            -fno-plt -ffunction-sections -fdata-sections

# Debug flags  
DEBUG_FLAGS = -g -O0 -DDEBUG

# Sanitizer flags
SANITIZE_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

# Profile flags
PROFILE_FLAGS = -g -pg

# Default: release build
.PHONY: all release debug sanitize profile clean test benchmark install

all: release

# Release build (optimized)
release: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG
release: LDFLAGS += -flto -Wl,--gc-sections
release: $(BIN_DIR)/$(TARGET)

# Debug build
debug: CFLAGS += $(WARNINGS) $(DEBUG_FLAGS)
debug: $(BIN_DIR)/$(TARGET)

# Sanitizer build (for testing)
sanitize: CFLAGS += $(WARNINGS) $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
sanitize: LDFLAGS += $(SANITIZE_FLAGS)
sanitize: $(BIN_DIR)/$(TARGET)

# Profile build
profile: CFLAGS += $(WARNINGS) -O2 $(PROFILE_FLAGS)
profile: LDFLAGS += $(PROFILE_FLAGS)
profile: $(BIN_DIR)/$(TARGET)

# Build binary
$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# Build library
lib: $(LIB_DIR)/$(LIBRARY)

$(LIB_DIR)/$(LIBRARY): $(LIB_OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $(LIB_OBJS)
	@echo "Built: $@"

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(BUILD_DIR) $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR):
	mkdir -p $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Install to /usr/local
install: release
	install -m 755 $(BIN_DIR)/$(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/$(TARGET)"

# Uninstall
uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Generate test data
test-data:
	@echo "Generating test data..."
	@mkdir -p test_data
	@for i in $$(seq 1 100000); do \
		echo "2024-01-01 12:00:00 INFO User login from 192.168.1.$$((i % 256))"; \
		echo "2024-01-01 12:00:01 DEBUG API key: AKIAIOSFODNN7EXAMPLE"; \
		echo "2024-01-01 12:00:02 INFO Normal log line with no secrets"; \
	done > test_data/sample.log
	@echo "Generated test_data/sample.log"

# Build and run unit tests
test-unit: debug $(BIN_DIR)/test_patterns $(BIN_DIR)/test_redactor
	@echo "Running unit tests..."
	$(BIN_DIR)/test_patterns
	$(BIN_DIR)/test_redactor

# Build test executables
$(BIN_DIR)/test_patterns: tests/test_patterns.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_redactor: tests/test_redactor.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/benchmark_suite: tests/benchmark.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OPT_FLAGS) $< $(LIB_OBJS) -o $@ $(LDFLAGS)

# Run benchmark
benchmark: release test-data
	@echo "Running benchmark..."
	@echo "File size: $$(du -h test_data/sample.log | cut -f1)"
	@echo "Line count: $$(wc -l < test_data/sample.log)"
	@echo ""
	time $(BIN_DIR)/$(TARGET) < test_data/sample.log > /dev/null

# Run full benchmark suite
benchmark-full: $(BIN_DIR)/benchmark_suite
	@echo "Running full benchmark suite..."
	$(BIN_DIR)/benchmark_suite

# Quick test
test: debug
	@echo "Testing basic functionality..."
	@echo "aws_key=AKIAIOSFODNN7EXAMPLE password=secret123" | $(BIN_DIR)/$(TARGET)

# Check dependencies
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libpcre2-8 && echo "✓ pcre2" || echo "✗ pcre2 (install with: apt install libpcre2-dev)"

# Format code (requires clang-format)
format:
	clang-format -i $(SRC_DIR)/*.c $(INC_DIR)/*.h

# Static analysis (requires cppcheck)
analyze:
	cppcheck --enable=all --std=c11 -I$(INC_DIR) $(SRC_DIR)/*.c

# Show build info
info:
	@echo "PlumbrC Build Configuration"
	@echo "==========================="
	@echo "CC:       $(CC)"
	@echo "CFLAGS:   $(CFLAGS)"
	@echo "LDFLAGS:  $(LDFLAGS)"
	@echo "Sources:  $(SRCS)"
	@echo "Objects:  $(OBJS)"

# Help
help:
	@echo "PlumbrC Makefile Targets"
	@echo "========================"
	@echo "  make          - Build optimized release binary"
	@echo "  make debug    - Build with debug symbols"
	@echo "  make sanitize - Build with address/UB sanitizers"
	@echo "  make profile  - Build for profiling with gprof"
	@echo "  make lib      - Build static library"
	@echo "  make test     - Quick functionality test"
	@echo "  make benchmark- Run performance benchmark"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install  - Install to /usr/local/bin"
	@echo "  make analyze  - Run static analysis"
	@echo "  make help     - Show this help"
