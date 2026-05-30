//! Raw FFI bindings to libplumbr.
//!
//! These are the direct C function signatures. Use the safe wrapper
//! in `lib.rs` instead.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int};

/// Opaque handle to a PlumbrC instance.
#[repr(C)]
pub struct libplumbr_t {
    _opaque: [u8; 0],
}

/// Error codes returned by PlumbrC functions.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum libplumbr_error_t {
    PLUMBR_OK = 0,
    PLUMBR_ERR_ALLOC = -1,
    PLUMBR_ERR_PATTERN = -2,
    PLUMBR_ERR_INPUT_TOO_LARGE = -3,
    PLUMBR_ERR_BUFFER_TOO_SMALL = -4,
    PLUMBR_ERR_NULL_INPUT = -5,
}

/// Configuration for creating a PlumbrC instance.
#[repr(C)]
pub struct libplumbr_config_t {
    pub pattern_file: *const c_char,
    pub pattern_dir: *const c_char,
    pub compliance: *const c_char,
    pub num_threads: c_int,
    pub quiet: c_int,
}

/// Statistics from a PlumbrC instance.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default)]
pub struct libplumbr_stats_t {
    pub lines_processed: usize,
    pub lines_modified: usize,
    pub patterns_matched: usize,
    pub bytes_processed: usize,
    pub elapsed_seconds: f64,
}

extern "C" {
    pub fn libplumbr_new(config: *const libplumbr_config_t) -> *mut libplumbr_t;
    pub fn libplumbr_free(p: *mut libplumbr_t);

    pub fn libplumbr_redact(
        p: *mut libplumbr_t,
        input: *const c_char,
        input_len: usize,
        output_len: *mut usize,
    ) -> *mut c_char;

    pub fn libplumbr_redact_into(
        p: *mut libplumbr_t,
        input: *const c_char,
        in_len: usize,
        output: *mut c_char,
        out_cap: usize,
    ) -> isize;

    pub fn libplumbr_redact_buffer(
        p: *mut libplumbr_t,
        input: *const c_char,
        input_len: usize,
        output_len: *mut usize,
    ) -> *mut c_char;

    pub fn libplumbr_free_string(str: *mut c_char);

    pub fn libplumbr_get_stats(p: *const libplumbr_t) -> libplumbr_stats_t;
    pub fn libplumbr_reset_stats(p: *mut libplumbr_t);
    pub fn libplumbr_pattern_count(p: *const libplumbr_t) -> usize;
    pub fn libplumbr_version() -> *const c_char;
    pub fn libplumbr_last_error() -> libplumbr_error_t;
    pub fn libplumbr_error_string(err: libplumbr_error_t) -> *const c_char;
}
