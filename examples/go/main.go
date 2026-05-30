// PlumbrC + Go HTTP Server Integration Example
//
// Runs PlumbrC as an HTTP sidecar and provides middleware for log redaction.
// Start PlumbrC sidecar: ./build/bin/plumbr-server --port 8081
// Then run: go run main.go

package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"time"
)

var plumbrURL = getEnv("PLUMBR_URL", "http://localhost:8081")

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

// RedactingWriter wraps an io.Writer and redacts via PlumbrC sidecar.
type RedactingWriter struct {
	client *http.Client
}

func (w *RedactingWriter) Write(p []byte) (n int, err error) {
	redacted := w.redact(string(p))
	fmt.Print(redacted)
	return len(p), nil
}

func (w *RedactingWriter) redact(text string) string {
	body, _ := json.Marshal(map[string]string{"text": text})
	resp, err := w.client.Post(plumbrURL+"/api/redact", "application/json", bytes.NewReader(body))
	if err != nil {
		return text // Fail open
	}
	defer resp.Body.Close()

	var result struct {
		Redacted string `json:"redacted"`
	}
	if json.NewDecoder(resp.Body).Decode(&result) == nil && result.Redacted != "" {
		return result.Redacted
	}
	return text
}

// LoggingMiddleware logs requests with redacted output.
func LoggingMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		start := time.Now()
		next.ServeHTTP(w, r)
		log.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(start))
	})
}

func main() {
	// Set up redacting logger
	writer := &RedactingWriter{
		client: &http.Client{Timeout: 2 * time.Second},
	}
	log.SetOutput(writer)

	mux := http.NewServeMux()

	mux.HandleFunc("/login", func(w http.ResponseWriter, r *http.Request) {
		var creds struct {
			Email    string `json:"email"`
			Password string `json:"password"`
		}
		json.NewDecoder(r.Body).Decode(&creds)
		// This log line would leak the password without PlumbrC!
		log.Printf("Login attempt: email=%s password=%s", creds.Email, creds.Password)
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "ok"})
	})

	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(map[string]string{"status": "healthy", "redactor": plumbrURL})
	})

	fmt.Println("Go server running on http://localhost:8080")
	fmt.Printf("PlumbrC sidecar: %s\n", plumbrURL)
	http.ListenAndServe(":8080", LoggingMiddleware(mux))
}
