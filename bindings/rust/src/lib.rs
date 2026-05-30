//! # PlumbrC — Rust Bindings
//!
//! High-performance log redaction powered by PlumbrC.
//!
//! ```rust
//! use plumbr::Plumbr;
//!
//! let p = Plumbr::new().unwrap();
//! let safe = p.redact("api_key=AKIAIOSFODNN7EXAMPLE");
//! assert!(!safe.contains("AKIAIOSFODNN7EXAMPLE"));
//! ```

mod ffi;

use std::ffi::{CStr, CString};
use std::fmt;

/// Error type for PlumbrC operations.
#[derive(Debug, Clone)]
pub enum PlumbrError {
    /// Memory allocation failed inside the C library.
    Alloc,
    /// Invalid pattern or pattern file path.
    Pattern,
    /// Input exceeds the maximum line size (64KB).
    InputTooLarge,
    /// Output buffer is too small for the redacted result.
    BufferTooSmall,
    /// A required argument was null (internal error).
    NullInput,
    /// Failed to create the PlumbrC instance.
    InitFailed,
    /// Unknown error from the C library.
    Unknown(i32),
}

impl fmt::Display for PlumbrError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PlumbrError::Alloc => write!(f, "memory allocation failed"),
            PlumbrError::Pattern => write!(f, "invalid pattern"),
            PlumbrError::InputTooLarge => write!(f, "input exceeds max line size"),
            PlumbrError::BufferTooSmall => write!(f, "output buffer too small"),
            PlumbrError::NullInput => write!(f, "null input"),
            PlumbrError::InitFailed => write!(f, "failed to create plumbr instance"),
            PlumbrError::Unknown(code) => write!(f, "unknown error (code {})", code),
        }
    }
}

impl std::error::Error for PlumbrError {}

impl From<isize> for PlumbrError {
    fn from(code: isize) -> Self {
        match code {
            -1 => PlumbrError::Alloc,
            -2 => PlumbrError::Pattern,
            -3 => PlumbrError::InputTooLarge,
            -4 => PlumbrError::BufferTooSmall,
            -5 => PlumbrError::NullInput,
            other => PlumbrError::Unknown(other as i32),
        }
    }
}

/// Redaction statistics.
#[derive(Debug, Clone, Copy, Default)]
pub struct Stats {
    pub lines_processed: usize,
    pub lines_modified: usize,
    pub patterns_matched: usize,
    pub bytes_processed: usize,
    pub elapsed_seconds: f64,
}

/// Configuration for creating a Plumbr instance.
#[derive(Debug, Clone, Default)]
pub struct PlumbrConfig {
    /// Path to a custom pattern file. `None` uses built-in defaults.
    pub pattern_file: Option<String>,
    /// Path to a directory of pattern files.
    pub pattern_dir: Option<String>,
    /// Compliance profiles (e.g., "hipaa,pci,gdpr").
    pub compliance: Option<String>,
    /// Number of worker threads. 0 = auto-detect.
    pub num_threads: i32,
    /// Suppress stats output to stderr.
    pub quiet: bool,
}

/// A PlumbrC redaction engine.
///
/// Each instance is single-threaded. Create multiple instances for
/// concurrent use.
pub struct Plumbr {
    ptr: *mut ffi::libplumbr_t,
}

// SAFETY: The C library documents that each instance is safe to use from
// one thread. We enforce single-thread usage via &self (not &mut self)
// and mark Send but not Sync.
unsafe impl Send for Plumbr {}

impl Plumbr {
    /// Create a new Plumbr instance with default patterns.
    pub fn new() -> Result<Self, PlumbrError> {
        let ptr = unsafe { ffi::libplumbr_new(std::ptr::null()) };
        if ptr.is_null() {
            return Err(PlumbrError::InitFailed);
        }
        Ok(Plumbr { ptr })
    }

    /// Create a new Plumbr instance with custom configuration.
    pub fn with_config(config: PlumbrConfig) -> Result<Self, PlumbrError> {
        let pattern_file_c = config.pattern_file.map(|s| CString::new(s).unwrap());
        let pattern_dir_c = config.pattern_dir.map(|s| CString::new(s).unwrap());
        let compliance_c = config.compliance.map(|s| CString::new(s).unwrap());

        let c_config = ffi::libplumbr_config_t {
            pattern_file: pattern_file_c.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            pattern_dir: pattern_dir_c.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            compliance: compliance_c.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            num_threads: config.num_threads,
            quiet: config.quiet as i32,
        };

        let ptr = unsafe { ffi::libplumbr_new(&c_config) };
        if ptr.is_null() {
            return Err(PlumbrError::InitFailed);
        }
        Ok(Plumbr { ptr })
    }

    /// Redact a string, returning the redacted result.
    pub fn redact(&self, input: &str) -> String {
        self.redact_bytes(input.as_bytes())
            .map(|b| String::from_utf8_lossy(&b).into_owned())
            .unwrap_or_else(|_| input.to_string())
    }

    /// Redact raw bytes, returning the redacted result.
    pub fn redact_bytes(&self, input: &[u8]) -> Result<Vec<u8>, PlumbrError> {
        let mut buf = vec![0u8; input.len() * 2 + 64];
        let n = self.redact_into(input, &mut buf)?;
        buf.truncate(n);
        Ok(buf)
    }

    /// Redact into a caller-owned buffer (zero-allocation).
    ///
    /// Returns the number of bytes written.
    pub fn redact_into(&self, input: &[u8], output: &mut [u8]) -> Result<usize, PlumbrError> {
        let n = unsafe {
            ffi::libplumbr_redact_into(
                self.ptr,
                input.as_ptr() as *const _,
                input.len(),
                output.as_mut_ptr() as *mut _,
                output.len(),
            )
        };
        if n < 0 {
            Err(PlumbrError::from(n))
        } else {
            Ok(n as usize)
        }
    }

    /// Redact a newline-separated buffer in one call.
    pub fn redact_buffer(&self, input: &[u8]) -> Result<Vec<u8>, PlumbrError> {
        let mut out_len: usize = 0;
        let ptr = unsafe {
            ffi::libplumbr_redact_buffer(
                self.ptr,
                input.as_ptr() as *const _,
                input.len(),
                &mut out_len,
            )
        };
        if ptr.is_null() {
            return Err(PlumbrError::Alloc);
        }
        let result = unsafe { std::slice::from_raw_parts(ptr as *const u8, out_len) }.to_vec();
        unsafe { ffi::libplumbr_free_string(ptr) };
        Ok(result)
    }

    /// Redact multiple lines in batch.
    pub fn redact_batch(&self, inputs: &[&str]) -> Vec<String> {
        inputs.iter().map(|s| self.redact(s)).collect()
    }

    /// Get redaction statistics.
    pub fn stats(&self) -> Stats {
        let s = unsafe { ffi::libplumbr_get_stats(self.ptr) };
        Stats {
            lines_processed: s.lines_processed,
            lines_modified: s.lines_modified,
            patterns_matched: s.patterns_matched,
            bytes_processed: s.bytes_processed,
            elapsed_seconds: s.elapsed_seconds,
        }
    }

    /// Get the number of loaded patterns.
    pub fn pattern_count(&self) -> usize {
        unsafe { ffi::libplumbr_pattern_count(self.ptr) }
    }

    /// Get the library version string.
    pub fn version() -> &'static str {
        unsafe {
            let ptr = ffi::libplumbr_version();
            CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
        }
    }
}

impl Drop for Plumbr {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { ffi::libplumbr_free(self.ptr) };
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_new_default() {
        let p = Plumbr::new().unwrap();
        assert!(p.pattern_count() >= 10);
    }

    #[test]
    fn test_redact_aws_key() {
        let p = Plumbr::new().unwrap();
        let result = p.redact("key=AKIAIOSFODNN7EXAMPLE");
        assert!(!result.contains("AKIAIOSFODNN7EXAMPLE"));
    }

    #[test]
    fn test_redact_no_match() {
        let p = Plumbr::new().unwrap();
        let input = "This is a normal log line";
        let result = p.redact(input);
        assert_eq!(result, input);
    }

    #[test]
    fn test_redact_into_zero_alloc() {
        let p = Plumbr::new().unwrap();
        let input = b"normal log line";
        let mut buf = [0u8; 256];
        let n = p.redact_into(input, &mut buf).unwrap();
        assert_eq!(&buf[..n], input);
    }

    #[test]
    fn test_redact_into_buffer_too_small() {
        let p = Plumbr::new().unwrap();
        let input = b"normal log line";
        let mut buf = [0u8; 2]; // Too small
        let result = p.redact_into(input, &mut buf);
        assert!(result.is_err());
    }

    #[test]
    fn test_redact_batch() {
        let p = Plumbr::new().unwrap();
        let results = p.redact_batch(&[
            "normal line",
            "key=AKIAIOSFODNN7EXAMPLE",
            "another normal",
        ]);
        assert_eq!(results[0], "normal line");
        assert!(!results[1].contains("AKIAIOSFODNN7EXAMPLE"));
        assert_eq!(results[2], "another normal");
    }

    #[test]
    fn test_stats() {
        let p = Plumbr::new().unwrap();
        p.redact("line1");
        p.redact("AKIAIOSFODNN7EXAMPLE");
        let stats = p.stats();
        assert_eq!(stats.lines_processed, 2);
    }

    #[test]
    fn test_version() {
        let ver = Plumbr::version();
        assert!(ver.contains('.'));
    }

    #[test]
    fn test_redact_buffer() {
        let p = Plumbr::new().unwrap();
        let input = b"normal line\nkey=AKIAIOSFODNN7EXAMPLE\nanother\n";
        let result = p.redact_buffer(input).unwrap();
        let text = String::from_utf8_lossy(&result);
        assert!(text.contains("normal line"));
        assert!(!text.contains("AKIAIOSFODNN7EXAMPLE"));
    }
}
