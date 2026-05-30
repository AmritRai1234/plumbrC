# PlumbrC - High-Performance Log Redaction Library
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
LIBRARY = libplumbr.a
SHARED_LIB = libplumbr.so

# Sources (library only — no main.c, no server.c)
LIB_SRCS = $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))
AMD_SRCS = $(wildcard $(SRC_DIR)/amd/*.c)
LIB_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(LIB_SRCS))
AMD_OBJS = $(patsubst $(SRC_DIR)/amd/%.c,$(OBJ_DIR)/amd_%.o,$(AMD_SRCS))
ALL_LIB_OBJS = $(LIB_OBJS) $(AMD_OBJS)

# GPU support (optional — build with 'make gpu' or 'GPU=1 make')
GPU_SRCS = $(wildcard $(SRC_DIR)/gpu/*.c)
GPU_OBJS = $(patsubst $(SRC_DIR)/gpu/%.c,$(OBJ_DIR)/gpu_%.o,$(GPU_SRCS))

# Compiler flags
CFLAGS = -std=c11 -I$(INC_DIR) -I$(SRC_DIR)/amd -D_GNU_SOURCE
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

# Default: build static library
.PHONY: all lib shared libs debug sanitize clean test test-unit benchmark-full benchmark-json check-deps format analyze info help gpu gpu-test gpu-benchmark

all: lib

# ─── Library Builds ───────────────────────────────────────────

# Static library (release)
lib: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG
lib: $(LIB_DIR)/$(LIBRARY)

$(LIB_DIR)/$(LIBRARY): $(ALL_LIB_OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $(ALL_LIB_OBJS)
	@echo "Built: $@"

# Shared library (release, PIC)
PIC_OBJ_DIR = $(BUILD_DIR)/pic
PIC_OBJS = $(patsubst $(SRC_DIR)/%.c,$(PIC_OBJ_DIR)/%.o,$(LIB_SRCS))
PIC_AMD_OBJS = $(patsubst $(SRC_DIR)/amd/%.c,$(PIC_OBJ_DIR)/amd_%.o,$(AMD_SRCS))
ALL_PIC_OBJS = $(PIC_OBJS) $(PIC_AMD_OBJS)

shared: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG
shared: $(LIB_DIR)/$(SHARED_LIB)

$(LIB_DIR)/$(SHARED_LIB): $(ALL_PIC_OBJS) | $(LIB_DIR)
	$(CC) -shared -Wl,-soname,$(SHARED_LIB).1 -o $@ $(ALL_PIC_OBJS) $(LDFLAGS)
	@echo "Built: $@"

# Build both libraries
libs: lib shared

# Debug build (for tests)
debug: CFLAGS += $(WARNINGS) $(DEBUG_FLAGS)
debug: $(ALL_LIB_OBJS)

# Sanitizer build (for testing)
sanitize: CFLAGS += $(WARNINGS) $(DEBUG_FLAGS) $(SANITIZE_FLAGS)
sanitize: LDFLAGS += $(SANITIZE_FLAGS)
sanitize: $(ALL_LIB_OBJS)

# GPU-accelerated build (requires OpenCL)
gpu: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG -DPLUMBR_GPU
gpu: LDFLAGS += -lOpenCL
gpu: $(ALL_LIB_OBJS) $(GPU_OBJS)
	@mkdir -p $(LIB_DIR)
	$(AR) rcs $(LIB_DIR)/$(LIBRARY) $(ALL_LIB_OBJS) $(GPU_OBJS)
	@echo "Built (GPU): $(LIB_DIR)/$(LIBRARY)"

# ─── Compile Rules ────────────────────────────────────────────

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/amd_%.o: $(SRC_DIR)/amd/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -mavx2 -c $< -o $@

$(OBJ_DIR)/gpu_%.o: $(SRC_DIR)/gpu/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(PIC_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(PIC_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(PIC_OBJ_DIR)/amd_%.o: $(SRC_DIR)/amd/%.c | $(PIC_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -mavx2 -c $< -o $@

# Create directories
$(BUILD_DIR) $(OBJ_DIR) $(LIB_DIR) $(BIN_DIR) $(PIC_OBJ_DIR):
	mkdir -p $@

# ─── Tests ────────────────────────────────────────────────────

test-unit: debug $(BIN_DIR)/test_patterns $(BIN_DIR)/test_redactor $(BIN_DIR)/test_libplumbr $(BIN_DIR)/test_io $(BIN_DIR)/test_security
	@echo "Running unit tests..."
	$(BIN_DIR)/test_patterns
	$(BIN_DIR)/test_redactor
	$(BIN_DIR)/test_libplumbr
	$(BIN_DIR)/test_io
	$(BIN_DIR)/test_security

test: test-unit

# DNA Genomics tests
dna-test: debug $(BIN_DIR)/test_dna
	@echo "Running DNA pathogen scanner tests..."
	$(BIN_DIR)/test_dna

$(BIN_DIR)/test_patterns: tests/test_patterns.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_redactor: tests/test_redactor.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_libplumbr: tests/test_libplumbr.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_io: tests/test_io.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_security: tests/test_security.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_dna: tests/test_dna.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

# ─── Benchmarks ───────────────────────────────────────────────

$(BIN_DIR)/benchmark_suite: tests/benchmark.c $(ALL_LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OPT_FLAGS) $< $(ALL_LIB_OBJS) -o $@ $(LDFLAGS)

benchmark-full: $(BIN_DIR)/benchmark_suite
	@$(BIN_DIR)/benchmark_suite

benchmark-json: $(BIN_DIR)/benchmark_suite
	@$(BIN_DIR)/benchmark_suite --json

# ─── GPU Tests & Benchmarks ───────────────────────────────────

gpu-test: gpu
	@echo "Running GPU-enabled unit tests..."
	@mkdir -p $(BIN_DIR)
	$(CC) -std=c11 -I$(INC_DIR) -I$(SRC_DIR)/amd -D_GNU_SOURCE -DPLUMBR_GPU -g -O0 tests/test_libplumbr.c $(ALL_LIB_OBJS) $(GPU_OBJS) -o $(BIN_DIR)/test_libplumbr_gpu -lpcre2-8 -lpthread -lOpenCL
	$(BIN_DIR)/test_libplumbr_gpu

gpu-benchmark: gpu
	@mkdir -p $(BIN_DIR)
	$(CC) -std=c11 -I$(INC_DIR) -I$(SRC_DIR)/amd -D_GNU_SOURCE -DPLUMBR_GPU $(OPT_FLAGS) tests/benchmark.c $(ALL_LIB_OBJS) $(GPU_OBJS) -o $(BIN_DIR)/benchmark_suite_gpu -lpcre2-8 -lpthread -lOpenCL
	@$(BIN_DIR)/benchmark_suite_gpu

# ─── Fuzz Targets (requires clang with libFuzzer) ─────────────

FUZZ_CC = clang
FUZZ_FLAGS = -g -O1 -fsanitize=fuzzer,address -I$(INC_DIR) -D_GNU_SOURCE
FUZZ_LDFLAGS = -lpcre2-8 -lpthread -fsanitize=fuzzer,address
FUZZ_LIB_SRCS = $(LIB_SRCS)

.PHONY: fuzz fuzz-json fuzz-redactor

fuzz: $(BIN_DIR)/fuzz_json $(BIN_DIR)/fuzz_redactor
	@echo "Fuzz harnesses built in $(BIN_DIR)/"
	@echo "  Run: $(BIN_DIR)/fuzz_json -max_len=4096 -timeout=5"
	@echo "  Run: $(BIN_DIR)/fuzz_redactor -max_len=65536 -timeout=10"

$(BIN_DIR)/fuzz_json: tests/fuzz_json.c $(FUZZ_LIB_SRCS) | $(BIN_DIR)
	$(FUZZ_CC) $(FUZZ_FLAGS) $^ -o $@ $(FUZZ_LDFLAGS)

$(BIN_DIR)/fuzz_redactor: tests/fuzz_redactor.c $(FUZZ_LIB_SRCS) | $(BIN_DIR)
	$(FUZZ_CC) $(FUZZ_FLAGS) $^ -o $@ $(FUZZ_LDFLAGS)

fuzz-json: $(BIN_DIR)/fuzz_json
	$(BIN_DIR)/fuzz_json -max_len=4096 -timeout=5 -runs=100000

fuzz-redactor: $(BIN_DIR)/fuzz_redactor
	$(BIN_DIR)/fuzz_redactor -max_len=65536 -timeout=10 -runs=100000

# ─── Install (library + headers) ──────────────────────────────

install: lib
	install -d /usr/local/lib /usr/local/include/plumbr
	install -m 644 $(LIB_DIR)/$(LIBRARY) /usr/local/lib/
	install -m 644 $(INC_DIR)/*.h /usr/local/include/plumbr/
	@echo "Installed libplumbr.a to /usr/local/lib/"
	@echo "Installed headers to /usr/local/include/plumbr/"

uninstall:
	rm -f /usr/local/lib/$(LIBRARY)
	rm -rf /usr/local/include/plumbr

# ─── Utilities ────────────────────────────────────────────────

clean:
	rm -rf $(BUILD_DIR)

check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists libpcre2-8 && echo "✓ pcre2" || echo "✗ pcre2 (install with: apt install libpcre2-dev)"

format:
	clang-format -i $(SRC_DIR)/*.c $(SRC_DIR)/amd/*.c $(INC_DIR)/*.h

analyze:
	cppcheck --enable=all --std=c11 -I$(INC_DIR) $(SRC_DIR)/*.c

info:
	@echo "PlumbrC Library Build Configuration"
	@echo "===================================="
	@echo "CC:       $(CC)"
	@echo "CFLAGS:   $(CFLAGS)"
	@echo "LDFLAGS:  $(LDFLAGS)"
	@echo "Sources:  $(LIB_SRCS)"

help:
	@echo "PlumbrC Library — Makefile Targets"
	@echo "==================================="
	@echo "  make            - Build static library (libplumbr.a)"
	@echo "  make shared     - Build shared library (libplumbr.so)"
	@echo "  make libs       - Build both static and shared libraries"
	@echo "  make test       - Build and run all unit tests"
	@echo "  make sanitize   - Build with address/UB sanitizers"
	@echo "  make benchmark-full - Run performance benchmarks"
	@echo "  make fuzz       - Build libFuzzer harnesses (requires clang)"
	@echo "  make install    - Install library + headers to /usr/local"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make analyze    - Run static analysis"
	@echo "  make help       - Show this help"
