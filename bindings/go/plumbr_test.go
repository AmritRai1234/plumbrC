package plumbr

import (
	"strings"
	"testing"
)

func TestNew(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatalf("New() failed: %v", err)
	}
	defer p.Close()

	if p.PatternCount() < 10 {
		t.Errorf("Expected >= 10 patterns, got %d", p.PatternCount())
	}
}

func TestRedact(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	result := p.Redact("key=AKIAIOSFODNN7EXAMPLE")
	if strings.Contains(result, "AKIAIOSFODNN7EXAMPLE") {
		t.Errorf("Expected AWS key to be redacted, got: %s", result)
	}
}

func TestRedactNoMatch(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	input := "This is a normal log line"
	result := p.Redact(input)
	if result != input {
		t.Errorf("Expected '%s', got '%s'", input, result)
	}
}

func TestRedactInto(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	input := []byte("normal log line")
	output := make([]byte, 256)

	n, err := p.RedactInto(input, output)
	if err != nil {
		t.Fatalf("RedactInto failed: %v", err)
	}
	if string(output[:n]) != "normal log line" {
		t.Errorf("Expected 'normal log line', got '%s'", output[:n])
	}

	// Buffer too small
	tiny := make([]byte, 2)
	_, err = p.RedactInto(input, tiny)
	if err == nil {
		t.Error("Expected error for tiny buffer")
	}
}

func TestRedactBytes(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	result, err := p.RedactBytes([]byte("key=AKIAIOSFODNN7EXAMPLE"))
	if err != nil {
		t.Fatal(err)
	}
	if strings.Contains(string(result), "AKIAIOSFODNN7EXAMPLE") {
		t.Error("Expected redaction")
	}
}

func TestRedactBatch(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	results := p.RedactBatch([]string{
		"normal line",
		"key=AKIAIOSFODNN7EXAMPLE",
		"another normal",
	})

	if results[0] != "normal line" {
		t.Errorf("Expected 'normal line', got '%s'", results[0])
	}
	if strings.Contains(results[1], "AKIAIOSFODNN7EXAMPLE") {
		t.Error("Expected redaction in second line")
	}
	if results[2] != "another normal" {
		t.Errorf("Expected 'another normal', got '%s'", results[2])
	}
}

func TestRedactBuffer(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	input := []byte("normal line\nkey=AKIAIOSFODNN7EXAMPLE\nanother\n")
	result, err := p.RedactBuffer(input)
	if err != nil {
		t.Fatal(err)
	}
	text := string(result)
	if !strings.Contains(text, "normal line") {
		t.Error("Normal line should pass through")
	}
	if strings.Contains(text, "AKIAIOSFODNN7EXAMPLE") {
		t.Error("AWS key should be redacted")
	}
}

func TestStats(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	defer p.Close()

	p.Redact("line1")
	p.Redact("AKIAIOSFODNN7EXAMPLE")

	stats := p.Stats()
	if stats.LinesProcessed != 2 {
		t.Errorf("Expected 2 lines processed, got %d", stats.LinesProcessed)
	}
}

func TestVersion(t *testing.T) {
	ver := Version()
	if !strings.Contains(ver, ".") {
		t.Errorf("Expected version with '.', got '%s'", ver)
	}
}

func TestClose(t *testing.T) {
	p, err := New(nil)
	if err != nil {
		t.Fatal(err)
	}
	// Double close should not crash
	p.Close()
	p.Close()
}

func BenchmarkRedact(b *testing.B) {
	p, err := New(nil)
	if err != nil {
		b.Fatal(err)
	}
	defer p.Close()

	input := "2024-01-15T10:30:00Z INFO key=AKIAIOSFODNN7EXAMPLE region=us-east-1"
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		p.Redact(input)
	}
}

func BenchmarkRedactInto(b *testing.B) {
	p, err := New(nil)
	if err != nil {
		b.Fatal(err)
	}
	defer p.Close()

	input := []byte("2024-01-15T10:30:00Z INFO key=AKIAIOSFODNN7EXAMPLE region=us-east-1")
	output := make([]byte, 256)

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		p.RedactInto(input, output)
	}
}
