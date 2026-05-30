// Package plumbr provides Go bindings for PlumbrC — high-performance log redaction.
//
// Usage:
//
//	p, err := plumbr.New(nil)
//	if err != nil {
//	    log.Fatal(err)
//	}
//	defer p.Close()
//
//	safe := p.Redact("api_key=AKIAIOSFODNN7EXAMPLE")
//	fmt.Println(safe) // "api_key=[REDACTED:aws_access_key]"
package plumbr

/*
#cgo CFLAGS: -I${SRCDIR}/../../include -I${SRCDIR}/../../src/amd -D_GNU_SOURCE -O3 -std=c11
#cgo LDFLAGS: -L${SRCDIR}/../../build/lib -lplumbr -lpcre2-8 -lpthread -lm

#include <stdlib.h>
#include <libplumbr.h>
*/
import "C"

import (
	"errors"
	"fmt"
	"runtime"
	"unsafe"
)

// Error codes from PlumbrC.
var (
	ErrAlloc          = errors.New("plumbr: memory allocation failed")
	ErrPattern        = errors.New("plumbr: invalid pattern")
	ErrInputTooLarge  = errors.New("plumbr: input exceeds max line size")
	ErrBufferTooSmall = errors.New("plumbr: output buffer too small")
	ErrNullInput      = errors.New("plumbr: null input")
	ErrInitFailed     = errors.New("plumbr: failed to create instance")
)

func errorFromCode(code C.ssize_t) error {
	switch code {
	case -1:
		return ErrAlloc
	case -2:
		return ErrPattern
	case -3:
		return ErrInputTooLarge
	case -4:
		return ErrBufferTooSmall
	case -5:
		return ErrNullInput
	default:
		return fmt.Errorf("plumbr: unknown error (code %d)", code)
	}
}

// Config holds configuration for creating a Plumbr instance.
type Config struct {
	// Path to a custom pattern file. Empty string uses built-in defaults.
	PatternFile string
	// Path to a directory of pattern files.
	PatternDir string
	// Compliance profiles (e.g., "hipaa,pci,gdpr").
	Compliance string
	// Number of worker threads. 0 = auto-detect.
	NumThreads int
	// Suppress stats output.
	Quiet bool
}

// Stats holds redaction statistics.
type Stats struct {
	LinesProcessed  uint64
	LinesModified   uint64
	PatternsMatched uint64
	BytesProcessed  uint64
	ElapsedSeconds  float64
}

// Plumbr is a high-performance log redaction engine.
//
// Each instance must be used from a single goroutine. For concurrent use,
// create multiple instances.
type Plumbr struct {
	ptr *C.libplumbr_t
}

// New creates a new Plumbr instance. Pass nil for default configuration.
func New(config *Config) (*Plumbr, error) {
	var cConfig *C.libplumbr_config_t

	if config != nil {
		cc := C.libplumbr_config_t{}

		if config.PatternFile != "" {
			cs := C.CString(config.PatternFile)
			defer C.free(unsafe.Pointer(cs))
			cc.pattern_file = cs
		}
		if config.PatternDir != "" {
			cs := C.CString(config.PatternDir)
			defer C.free(unsafe.Pointer(cs))
			cc.pattern_dir = cs
		}
		if config.Compliance != "" {
			cs := C.CString(config.Compliance)
			defer C.free(unsafe.Pointer(cs))
			cc.compliance = cs
		}
		cc.num_threads = C.int(config.NumThreads)
		if config.Quiet {
			cc.quiet = 1
		}
		cConfig = &cc
	}

	ptr := C.libplumbr_new(cConfig)
	if ptr == nil {
		return nil, ErrInitFailed
	}

	p := &Plumbr{ptr: ptr}
	runtime.SetFinalizer(p, (*Plumbr).Close)
	return p, nil
}

// Close releases all resources. Safe to call multiple times.
func (p *Plumbr) Close() {
	if p.ptr != nil {
		C.libplumbr_free(p.ptr)
		p.ptr = nil
		runtime.SetFinalizer(p, nil)
	}
}

// Redact redacts a string and returns the result.
func (p *Plumbr) Redact(input string) string {
	result, _ := p.RedactBytes([]byte(input))
	return string(result)
}

// RedactBytes redacts raw bytes using a pre-allocated output buffer.
// Returns the redacted bytes.
func (p *Plumbr) RedactBytes(input []byte) ([]byte, error) {
	if len(input) == 0 {
		return []byte{}, nil
	}

	// Allocate output buffer (2x input + 64 for redaction tags)
	outCap := len(input)*2 + 64
	output := make([]byte, outCap)

	n := C.libplumbr_redact_into(
		p.ptr,
		(*C.char)(unsafe.Pointer(&input[0])),
		C.size_t(len(input)),
		(*C.char)(unsafe.Pointer(&output[0])),
		C.size_t(outCap),
	)

	if n < 0 {
		return nil, errorFromCode(n)
	}

	return output[:n], nil
}

// RedactInto redacts into a caller-provided buffer (zero-allocation).
// Returns the number of bytes written.
func (p *Plumbr) RedactInto(input, output []byte) (int, error) {
	if len(input) == 0 {
		return 0, nil
	}

	n := C.libplumbr_redact_into(
		p.ptr,
		(*C.char)(unsafe.Pointer(&input[0])),
		C.size_t(len(input)),
		(*C.char)(unsafe.Pointer(&output[0])),
		C.size_t(len(output)),
	)

	if n < 0 {
		return 0, errorFromCode(n)
	}
	return int(n), nil
}

// RedactBuffer redacts a newline-separated buffer in one call.
func (p *Plumbr) RedactBuffer(input []byte) ([]byte, error) {
	if len(input) == 0 {
		return []byte{}, nil
	}

	var outLen C.size_t
	result := C.libplumbr_redact_buffer(
		p.ptr,
		(*C.char)(unsafe.Pointer(&input[0])),
		C.size_t(len(input)),
		&outLen,
	)

	if result == nil {
		return nil, ErrAlloc
	}
	defer C.libplumbr_free_string(result)

	return C.GoBytes(unsafe.Pointer(result), C.int(outLen)), nil
}

// RedactBatch redacts multiple strings.
func (p *Plumbr) RedactBatch(inputs []string) []string {
	results := make([]string, len(inputs))
	for i, input := range inputs {
		results[i] = p.Redact(input)
	}
	return results
}

// Stats returns redaction statistics.
func (p *Plumbr) Stats() Stats {
	s := C.libplumbr_get_stats(p.ptr)
	return Stats{
		LinesProcessed:  uint64(s.lines_processed),
		LinesModified:   uint64(s.lines_modified),
		PatternsMatched:  uint64(s.patterns_matched),
		BytesProcessed:  uint64(s.bytes_processed),
		ElapsedSeconds:  float64(s.elapsed_seconds),
	}
}

// PatternCount returns the number of loaded patterns.
func (p *Plumbr) PatternCount() int {
	return int(C.libplumbr_pattern_count(p.ptr))
}

// Version returns the PlumbrC version string.
func Version() string {
	return C.GoString(C.libplumbr_version())
}
