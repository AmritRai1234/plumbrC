# AMD Optimizations for PlumbrC

Target: AMD Ryzen 5000 Series + AMD Radeon Graphics

## CPU Optimizations (Zen 3 Architecture)
- AVX2 SIMD for parallel string matching
- AMD-specific prefetch patterns
- Cache-line aligned data structures (64 bytes)
- Branch prediction hints

## GPU Acceleration (OpenCL/ROCm)
- Batch pattern matching on GPU
- PCIe transfer optimization
- Shared memory utilization

## Compiler Flags
```
-march=znver3      # Zen 3 specific
-mtune=znver3      # Tune for Zen 3
-mavx2             # Enable AVX2
-mfma              # Fused multiply-add
-mpclmul           # Carry-less multiplication
```

## Files
- `simd_match.c`   - AVX2 pattern matching
- `gpu_batch.c`    - OpenCL batch processing (future)
- `prefetch.h`     - AMD prefetch hints
