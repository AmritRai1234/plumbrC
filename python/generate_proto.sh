#!/bin/bash
# Generate Python protobuf stubs from plumbr.proto
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROTO_DIR="$SCRIPT_DIR/../../proto"
OUT_DIR="$SCRIPT_DIR/plumbrc/proto"

echo "Generating Python protobuf stubs..."

python -m grpc_tools.protoc \
    -I"$PROTO_DIR" \
    --python_out="$OUT_DIR" \
    --grpc_python_out="$OUT_DIR" \
    --pyi_out="$OUT_DIR" \
    "$PROTO_DIR/plumbr.proto"

# Fix imports in generated files (grpc_tools generates absolute imports)
sed -i 's/^import plumbr_pb2/from plumbrc.proto import plumbr_pb2/' "$OUT_DIR/plumbr_pb2_grpc.py"

echo "Generated stubs in $OUT_DIR"
echo "  - plumbr_pb2.py"
echo "  - plumbr_pb2_grpc.py"
echo "  - plumbr_pb2.pyi"
