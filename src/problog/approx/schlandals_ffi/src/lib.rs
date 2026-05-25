use std::ffi::c_char;
use std::io::Write;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::slice;

use schlandals::args::Args;

#[cfg(unix)]
struct StdoutSilencer {
    saved_stdout: i32,
    dev_null: i32,
}

#[cfg(unix)]
impl StdoutSilencer {
    fn new() -> Option<Self> {
        unsafe {
            let _ = std::io::stdout().flush();
            let saved_stdout = dup(STDOUT_FILENO);
            if saved_stdout < 0 {
                return None;
            }
            let dev_null = open(c"/dev/null".as_ptr(), O_WRONLY);
            if dev_null < 0 {
                close(saved_stdout);
                return None;
            }
            if dup2(dev_null, STDOUT_FILENO) < 0 {
                close(dev_null);
                close(saved_stdout);
                return None;
            }
            Some(Self {
                saved_stdout,
                dev_null,
            })
        }
    }
}

#[cfg(unix)]
impl Drop for StdoutSilencer {
    fn drop(&mut self) {
        unsafe {
            let _ = std::io::stdout().flush();
            dup2(self.saved_stdout, STDOUT_FILENO);
            close(self.dev_null);
            close(self.saved_stdout);
        }
    }
}

#[cfg(unix)]
const STDOUT_FILENO: i32 = 1;
#[cfg(unix)]
const O_WRONLY: i32 = 1;

#[cfg(unix)]
unsafe extern "C" {
    fn close(fd: i32) -> i32;
    fn dup(fd: i32) -> i32;
    fn dup2(oldfd: i32, newfd: i32) -> i32;
    fn open(pathname: *const c_char, flags: i32) -> i32;
}

fn write_error(error_buf: *mut c_char, error_buf_len: usize, message: &str) {
    if error_buf.is_null() || error_buf_len == 0 {
        return;
    }
    let bytes = message.as_bytes();
    let copy_len = bytes.len().min(error_buf_len.saturating_sub(1));
    unsafe {
        std::ptr::copy_nonoverlapping(bytes.as_ptr().cast::<c_char>(), error_buf, copy_len);
        *error_buf.add(copy_len) = 0;
    }
}

fn clear_error(error_buf: *mut c_char, error_buf_len: usize) {
    if error_buf.is_null() || error_buf_len == 0 {
        return;
    }
    unsafe {
        *error_buf = 0;
    }
}

fn rebuild_distributions(
    flat_ptr: *const f64,
    offsets_ptr: *const u32,
    num_distributions: usize,
) -> Result<Vec<Vec<f64>>, String> {
    if num_distributions == 0 {
        return Ok(Vec::new());
    }
    if flat_ptr.is_null() || offsets_ptr.is_null() {
        return Err("null distribution buffer passed to schlandals ffi".to_string());
    }
    let offsets = unsafe { slice::from_raw_parts(offsets_ptr, num_distributions + 1) };
    let total_len = *offsets.last().unwrap() as usize;
    let flat = unsafe { slice::from_raw_parts(flat_ptr, total_len) };

    let mut out = Vec::with_capacity(num_distributions);
    for window in offsets.windows(2) {
        let start = window[0] as usize;
        let end = window[1] as usize;
        if start > end || end > flat.len() {
            return Err("invalid distribution offsets passed to schlandals ffi".to_string());
        }
        out.push(flat[start..end].to_vec());
    }
    Ok(out)
}

fn rebuild_clauses(
    flat_ptr: *const i32,
    offsets_ptr: *const u32,
    num_clauses: usize,
) -> Result<Vec<Vec<isize>>, String> {
    if num_clauses == 0 {
        return Ok(Vec::new());
    }
    if flat_ptr.is_null() || offsets_ptr.is_null() {
        return Err("null clause buffer passed to schlandals ffi".to_string());
    }
    let offsets = unsafe { slice::from_raw_parts(offsets_ptr, num_clauses + 1) };
    let total_len = *offsets.last().unwrap() as usize;
    let flat = unsafe { slice::from_raw_parts(flat_ptr, total_len) };

    let mut out = Vec::with_capacity(num_clauses);
    for window in offsets.windows(2) {
        let start = window[0] as usize;
        let end = window[1] as usize;
        if start > end || end > flat.len() {
            return Err("invalid clause offsets passed to schlandals ffi".to_string());
        }
        out.push(flat[start..end].iter().map(|lit| *lit as isize).collect());
    }
    Ok(out)
}

#[no_mangle]
pub extern "C" fn souffle_schlandals_solve(
    distributions_flat_ptr: *const f64,
    distribution_offsets_ptr: *const u32,
    num_distributions: u32,
    clauses_flat_ptr: *const i32,
    clause_offsets_ptr: *const u32,
    num_clauses: u32,
    epsilon: f64,
    lds: bool,
    out_estimate: *mut f64,
    out_lower: *mut f64,
    out_upper: *mut f64,
    error_buf: *mut c_char,
    error_buf_len: usize,
) -> i32 {
    clear_error(error_buf, error_buf_len);
    let result = catch_unwind(AssertUnwindSafe(|| -> Result<(f64, f64, f64), String> {
        if out_estimate.is_null() || out_lower.is_null() || out_upper.is_null() {
            return Err("null output buffer passed to schlandals ffi".to_string());
        }

        let distributions = rebuild_distributions(
            distributions_flat_ptr,
            distribution_offsets_ptr,
            num_distributions as usize,
        )?;
        let clauses = rebuild_clauses(clauses_flat_ptr, clause_offsets_ptr, num_clauses as usize)?;

        let mut args = Args::default();
        args.set_epsilon(epsilon);
        args.set_lds(lds);
        args.set_statistics(true);

        #[cfg(unix)]
        let _silencer = StdoutSilencer::new();
        let (lower, upper) = schlandals::pysearch(args, &distributions, &clauses);
        let estimate = (lower * upper).sqrt();
        Ok((estimate, lower, upper))
    }));

    match result {
        Ok(Ok((estimate, lower, upper))) => {
            unsafe {
                *out_estimate = estimate;
                *out_lower = lower;
                *out_upper = upper;
            }
            0
        }
        Ok(Err(message)) => {
            write_error(error_buf, error_buf_len, &message);
            1
        }
        Err(_) => {
            write_error(error_buf, error_buf_len, "schlandals ffi panicked");
            2
        }
    }
}
