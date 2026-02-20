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
SHARED_LIB = libplumbr.so
SHARED_LIB_VERSION = 1.0.0

# Sources (exclude main.c and server.c for library builds)
SRCS = $(filter-out $(SRC_DIR)/server.c,$(wildcard $(SRC_DIR)/*.c))
AMD_SRCS = $(wildcard $(SRC_DIR)/amd/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
AMD_OBJS = $(patsubst $(SRC_DIR)/amd/%.c,$(OBJ_DIR)/amd_%.o,$(AMD_SRCS))
ALL_OBJS = $(OBJS) $(AMD_OBJS)
LIB_OBJS = $(filter-out $(OBJ_DIR)/main.o,$(ALL_OBJS))
SHARED_OBJS = $(patsubst $(OBJ_DIR)/%.o,$(OBJ_DIR)/%.pic.o,$(LIB_OBJS))

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

# Profile flags
PROFILE_FLAGS = -g -pg

# gRPC / Protobuf
PROTO_DIR = proto
GRPC_GEN_DIR = $(BUILD_DIR)/grpc_gen
GRPC_CPP_PLUGIN = $(shell which grpc_cpp_plugin)
PROTOC = protoc

# Default: release build
.PHONY: all release debug sanitize profile clean test benchmark install server grpc


all: release

# Release build (optimized)
release: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG
release: LDFLAGS += -flto -Wl,--gc-sections
release: $(BIN_DIR)/$(TARGET)

# PGO build (profile-guided optimization)
pgo: clean
	@echo "=== PGO Phase 1: Instrument ==="
	$(MAKE) CFLAGS="$(CFLAGS) $(WARNINGS) -O3 -march=native -fprofile-generate -DNDEBUG" \
	       LDFLAGS="$(LDFLAGS) -fprofile-generate" $(BIN_DIR)/$(TARGET)
	@echo "=== PGO Phase 2: Train ==="
	@python3 -c "import random; f=open('/tmp/pgo_train.txt','w'); \
	  [f.write(random.choice(['password=s3cret','AKIAIOSFODNN7EXAMPLE key','ghp_ABCDEFGHIJKLMNOPQRSTUVWXYZab','2024-01-15 INFO GET /api 200 45ms','DEBUG cache HIT user_9382 ttl=300s','INFO upstream 200 OK latency=12ms','WARN pool at 80%% capacity','kafka OrderCreated','k8s readiness passed','redis HGET 1.2ms'])+'\'\n') for _ in range(500000)]; f.close()"
	$(BIN_DIR)/$(TARGET) -q -p patterns/all.txt < /tmp/pgo_train.txt > /dev/null 2>/dev/null || true
	@echo "=== PGO Phase 3: Optimize ==="
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	$(MAKE) CFLAGS="$(CFLAGS) $(WARNINGS) -Wno-error=coverage-mismatch -Wno-error=missing-profile \
	       -O3 -march=native -flto -fprofile-use -fprofile-correction \
	       -fomit-frame-pointer -fno-plt -ffunction-sections -fdata-sections -DNDEBUG" \
	       LDFLAGS="$(LDFLAGS) -flto -fprofile-use -Wl,--gc-sections" $(BIN_DIR)/$(TARGET)
	@rm -f /tmp/pgo_train.txt
	@echo "=== PGO build complete ==="

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
$(BIN_DIR)/$(TARGET): $(ALL_OBJS) | $(BIN_DIR)
	$(CC) $(ALL_OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

# Build HTTP server
server: CFLAGS += $(WARNINGS) $(OPT_FLAGS) -DNDEBUG
server: LDFLAGS += -flto -Wl,--gc-sections
server: $(BIN_DIR)/plumbr-server

$(BIN_DIR)/plumbr-server: $(OBJ_DIR)/server.o $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(OBJ_DIR)/server.o $(LIB_OBJS) -o $@ $(LDFLAGS)
	@echo "Built: $@"

$(OBJ_DIR)/server.o: $(SRC_DIR)/server.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build static library
lib: $(LIB_DIR)/$(LIBRARY)

$(LIB_DIR)/$(LIBRARY): $(LIB_OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $(LIB_OBJS)
	@echo "Built: $@"

# Build shared library (needs PIC objects)
PIC_OBJ_DIR = $(BUILD_DIR)/pic
PIC_SRCS = $(filter-out $(SRC_DIR)/main.c,$(SRCS))
PIC_OBJS = $(patsubst $(SRC_DIR)/%.c,$(PIC_OBJ_DIR)/%.o,$(PIC_SRCS))
PIC_AMD_OBJS = $(patsubst $(SRC_DIR)/amd/%.c,$(PIC_OBJ_DIR)/amd_%.o,$(AMD_SRCS))
ALL_PIC_OBJS = $(PIC_OBJS) $(PIC_AMD_OBJS)

shared: $(LIB_DIR)/$(SHARED_LIB)

$(LIB_DIR)/$(SHARED_LIB): $(ALL_PIC_OBJS) | $(LIB_DIR)
	$(CC) -shared -Wl,-soname,$(SHARED_LIB).1 -o $@ $(ALL_PIC_OBJS) $(LDFLAGS)
	@echo "Built: $@"

# Compile PIC objects for shared library
$(PIC_OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(PIC_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(PIC_OBJ_DIR)/amd_%.o: $(SRC_DIR)/amd/%.c | $(PIC_OBJ_DIR)
	$(CC) $(CFLAGS) -fPIC -mavx2 -c $< -o $@

$(PIC_OBJ_DIR):
	mkdir -p $@

# Build both libraries
libs: lib shared

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile AMD-specific files with AVX2
$(OBJ_DIR)/amd_%.o: $(SRC_DIR)/amd/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -mavx2 -c $< -o $@

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
test-unit: debug $(BIN_DIR)/test_patterns $(BIN_DIR)/test_redactor $(BIN_DIR)/test_libplumbr $(BIN_DIR)/test_io
	@echo "Running unit tests..."
	$(BIN_DIR)/test_patterns
	$(BIN_DIR)/test_redactor
	$(BIN_DIR)/test_libplumbr
	$(BIN_DIR)/test_io

# Build test executables
$(BIN_DIR)/test_patterns: tests/test_patterns.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_redactor: tests/test_redactor.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_libplumbr: tests/test_libplumbr.c $(LIB_OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $< $(LIB_OBJS) -o $@ $(LDFLAGS)

$(BIN_DIR)/test_io: tests/test_io.c $(LIB_OBJS) | $(BIN_DIR)
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

# Run full benchmark suite (human-readable)
benchmark-full: $(BIN_DIR)/benchmark_suite
	@$(BIN_DIR)/benchmark_suite

# Run benchmark with JSON output (for CI regression checks)
benchmark-json: $(BIN_DIR)/benchmark_suite
	@$(BIN_DIR)/benchmark_suite --json

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
	@echo "  make server   - Build HTTP API server"
	@echo "  make grpc     - Build gRPC API server"
	@echo "  make test     - Quick functionality test"
	@echo "  make benchmark- Run performance benchmark"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install  - Install to /usr/local/bin"
	@echo "  make analyze  - Run static analysis"
	@echo "  make help     - Show this help"

# ─── gRPC server build ─────────────────────────────────────────

# Generate protobuf + gRPC C++ stubs
$(GRPC_GEN_DIR)/plumbr.pb.cc $(GRPC_GEN_DIR)/plumbr.pb.h: $(PROTO_DIR)/plumbr.proto | $(GRPC_GEN_DIR)
	$(PROTOC) -I$(PROTO_DIR) \
		--cpp_out=$(GRPC_GEN_DIR) \
		$<

$(GRPC_GEN_DIR)/plumbr.grpc.pb.cc $(GRPC_GEN_DIR)/plumbr.grpc.pb.h: $(PROTO_DIR)/plumbr.proto | $(GRPC_GEN_DIR)
	$(PROTOC) -I$(PROTO_DIR) \
		--grpc_out=$(GRPC_GEN_DIR) \
		--plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN) \
		$<

$(GRPC_GEN_DIR):
	mkdir -p $@

# Compile generated protobuf sources
$(OBJ_DIR)/plumbr.pb.o: $(GRPC_GEN_DIR)/plumbr.pb.cc $(GRPC_GEN_DIR)/plumbr.pb.h | $(OBJ_DIR)
	g++ -std=c++17 -O2 -I$(GRPC_GEN_DIR) -c $< -o $@

$(OBJ_DIR)/plumbr.grpc.pb.o: $(GRPC_GEN_DIR)/plumbr.grpc.pb.cc $(GRPC_GEN_DIR)/plumbr.grpc.pb.h | $(OBJ_DIR)
	g++ -std=c++17 -O2 -I$(GRPC_GEN_DIR) -c $< -o $@

# Compile gRPC server
$(OBJ_DIR)/grpc_server.o: $(SRC_DIR)/grpc_server.cc $(GRPC_GEN_DIR)/plumbr.pb.h $(GRPC_GEN_DIR)/plumbr.grpc.pb.h | $(OBJ_DIR)
	g++ -std=c++17 -O2 -I$(INC_DIR) -I$(GRPC_GEN_DIR) -c $< -o $@

# Link gRPC server — dynamic link (builder & runner both use Alpine 3.21)
GRPC_LDFLAGS = $(shell pkg-config --libs grpc++ protobuf 2>/dev/null || echo "-lgrpc++ -lgrpc -lgpr -lprotobuf -lupb -laddress_sorting -lre2 -lcares -lz -lssl -lcrypto") -lpcre2-8
GRPC_CXXFLAGS = $(shell pkg-config --cflags grpc++ protobuf 2>/dev/null)

$(BIN_DIR)/plumbr-grpc: $(OBJ_DIR)/grpc_server.o $(OBJ_DIR)/plumbr.pb.o $(OBJ_DIR)/plumbr.grpc.pb.o $(LIB_OBJS) | $(BIN_DIR)
	g++ -std=c++17 $^ -o $@ $(GRPC_LDFLAGS)
	@echo "Built: $@"

grpc: lib $(BIN_DIR)/plumbr-grpc
