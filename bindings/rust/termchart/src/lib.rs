//! Safe Rust bindings for termchart.
//!
//! Two layers, matching the C++ convenience API:
//! * **One-liners** — [`plot`], [`scatter`], [`bar`] take slices and return ANSI strings.
//! * **Chart builder** — [`Chart`] for multi-series / property / SVG / PNG export.
//!
//! Errors are reported as [`Error`] (code + message read from `tc_error_message`);
//! nothing is silently downgraded. Native library setup is documented in the README.

use std::ffi::{CStr, CString};
use std::fmt;
use std::ptr;

use termchart_sys as sys;

/// Raw FFI constants/types re-exported for advanced callers and integration tests.
pub use termchart_sys as ffi;

/// A C ABI status code plus its human-readable message.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    pub code: i32,
    pub message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "termchart error {}: {}", self.code, self.message)
    }
}
impl std::error::Error for Error {}

/// Initialize process-wide state (idempotent).
pub fn init() {
    unsafe { sys::tc_init() }
}

/// Library version string, e.g. "1.0.0".
pub fn version() -> String {
    unsafe {
        let p = sys::tc_version_string();
        if p.is_null() {
            return String::new();
        }
        CStr::from_ptr(p).to_string_lossy().into_owned()
    }
}

/// All registered chart kind names, space-separated (discovery for [`Chart::new`]).
pub fn kinds() -> Result<String, Error> {
    two_stage(|buf, cap, out_len, err| unsafe {
        sys::tc_chart_kinds(buf as *mut _, cap, out_len, err)
    })
    .map(|bytes| String::from_utf8_lossy(&bytes).into_owned())
}

/// Turn a `tc_status` + error handle into `Result<(), Error>`, always freeing the handle.
unsafe fn check(status: sys::tc_status, err: sys::tc_error_handle) -> Result<(), Error> {
    if status == sys::TC_OK {
        if !err.is_null() {
            sys::tc_error_free(err);
        }
        return Ok(());
    }
    let message = if err.is_null() {
        format!("termchart C ABI error {status}")
    } else {
        let mut buf = [0i8; 512];
        sys::tc_error_message(err, buf.as_mut_ptr(), buf.len() as u64);
        sys::tc_error_free(err);
        CStr::from_ptr(buf.as_ptr()).to_string_lossy().into_owned()
    };
    Err(Error { code: status, message })
}

/// Drive a two-stage C function (cap=0 to query length, then fill) into a Rust Vec.
fn two_stage<F>(mut call: F) -> Result<Vec<u8>, Error>
where
    F: FnMut(*mut i8, u64, *mut u64, *mut sys::tc_error_handle) -> sys::tc_status,
{
    let mut len: u64 = 0;
    let mut err: sys::tc_error_handle = ptr::null_mut();
    let status = call(ptr::null_mut(), 0, &mut len, &mut err);
    // First stage legitimately returns BUFFER_TOO_SMALL with the required length.
    if status != sys::TC_OK && status != sys::TC_ERR_BUFFER_TOO_SMALL {
        return unsafe { check(status, err) }.map(|_| Vec::new());
    }
    if !err.is_null() {
        unsafe { sys::tc_error_free(err) };
    }
    if len == 0 {
        return Ok(Vec::new());
    }
    let mut buf = vec![0i8; len as usize + 1];
    err = ptr::null_mut();
    let status = call(buf.as_mut_ptr(), len, &mut len, &mut err);
    unsafe { check(status, err)? };
    buf.truncate(len as usize);
    Ok(buf.into_iter().map(|b| b as u8).collect())
}

/// Take a C-owned `char*` (freed with `tc_free_string`) into a Rust String.
unsafe fn take_c_string(p: *mut i8) -> Result<String, Error> {
    if p.is_null() {
        return Err(Error { code: sys::TC_ERR_NOMEM, message: "null output string".into() });
    }
    let s = CStr::from_ptr(p).to_string_lossy().into_owned();
    sys::tc_free_string(p);
    Ok(s)
}

/// One-liner line chart: x/y slices of equal length → ANSI string.
pub fn plot(x: &[f64], y: &[f64]) -> Result<String, Error> {
    plot_impl(x, y, false)
}

/// One-liner scatter chart.
pub fn scatter(x: &[f64], y: &[f64]) -> Result<String, Error> {
    plot_impl(x, y, true)
}

fn plot_impl(x: &[f64], y: &[f64], is_scatter: bool) -> Result<String, Error> {
    if x.is_empty() || x.len() != y.len() {
        return Err(Error {
            code: sys::TC_ERR_INVALID_ARG,
            message: "plot expects non-empty x/y of equal length".into(),
        });
    }
    let chart = Chart::new(if is_scatter { "scatter" } else { "line" })?;
    unsafe {
        check(
            sys::tc_chart_add_points(chart.raw, x.as_ptr(), y.as_ptr(), x.len() as u64),
            ptr::null_mut(),
        )?;
    }
    chart.render_ansi()
}

/// One-liner bar chart: values → ANSI string.
pub fn bar(values: &[f64]) -> Result<String, Error> {
    if values.is_empty() {
        return Err(Error { code: sys::TC_ERR_INVALID_ARG, message: "empty values".into() });
    }
    let chart = Chart::new("bar")?;
    unsafe {
        check(
            sys::tc_chart_add_bar(
                chart.raw,
                values.as_ptr(),
                values.len() as u64,
                ptr::null(),
                0,
            ),
            ptr::null_mut(),
        )?;
    }
    chart.render_ansi()
}

/// Render a plain text line to ANSI (no chart).
pub fn render_line(text: &str) -> Result<String, Error> {
    let text = CString::new(text)
        .map_err(|_| Error { code: sys::TC_ERR_INVALID_ARG, message: "interior NUL".into() })?;
    let mut out: *mut i8 = ptr::null_mut();
    let mut err: sys::tc_error_handle = ptr::null_mut();
    let status = unsafe {
        sys::tc_render_line(text.as_ptr(), ptr::null(), ptr::null(), &mut out, &mut err)
    };
    unsafe { check(status, err)? };
    unsafe { take_c_string(out) }
}

/// An owned chart handle (RAII: frees the native chart on drop).
pub struct Chart {
    raw: sys::tc_chart_handle,
}

impl Chart {
    fn from_raw(raw: sys::tc_chart_handle) -> Result<Self, Error> {
        if raw.is_null() {
            Err(Error { code: sys::TC_ERR_INVALID_HANDLE, message: "chart create failed".into() })
        } else {
            Ok(Self { raw })
        }
    }

    /// Create a chart by kind name (see [`kinds`]).
    pub fn new(kind: &str) -> Result<Self, Error> {
        let kind = CString::new(kind)
            .map_err(|_| Error { code: sys::TC_ERR_INVALID_ARG, message: "interior NUL".into() })?;
        let mut err: sys::tc_error_handle = ptr::null_mut();
        let raw = unsafe { sys::tc_chart_create(kind.as_ptr(), &mut err) };
        match Self::from_raw(raw) {
            Ok(chart) => Ok(chart),
            Err(e) => {
                if !err.is_null() {
                    unsafe { sys::tc_error_free(err) };
                }
                Err(e)
            }
        }
    }

    pub fn line() -> Result<Self, Error> {
        Self::new("line")
    }
    pub fn scatter_chart() -> Result<Self, Error> {
        Self::new("scatter")
    }
    pub fn bar_chart() -> Result<Self, Error> {
        Self::new("bar")
    }

    /// Append (x, y) points to a line/scatter chart.
    pub fn add_points(&self, x: &[f64], y: &[f64]) -> Result<(), Error> {
        if x.len() != y.len() {
            return Err(Error { code: sys::TC_ERR_INVALID_ARG, message: "x/y length mismatch".into() });
        }
        unsafe {
            check(
                sys::tc_chart_add_points(self.raw, x.as_ptr(), y.as_ptr(), x.len() as u64),
                ptr::null_mut(),
            )
        }
    }

    /// Add values to a bar chart, with optional labels.
    pub fn add_bar(&self, values: &[f64], labels: &[String]) -> Result<(), Error> {
        let label_c: Vec<CString> = labels
            .iter()
            .map(|s| CString::new(s.as_str()).unwrap_or_default())
            .collect();
        let label_ptrs: Vec<*const i8> = label_c.iter().map(|s| s.as_ptr()).collect();
        unsafe {
            check(
                sys::tc_chart_add_bar(
                    self.raw,
                    values.as_ptr(),
                    values.len() as u64,
                    if label_ptrs.is_empty() { ptr::null() } else { label_ptrs.as_ptr() },
                    label_ptrs.len() as u64,
                ),
                ptr::null_mut(),
            )
        }
    }

    /// Render to an ANSI string (UTF-8 + SGR), no stdout write.
    pub fn render_ansi(&self) -> Result<String, Error> {
        let mut out: *mut i8 = ptr::null_mut();
        let mut err: sys::tc_error_handle = ptr::null_mut();
        let status = unsafe { sys::tc_chart_render_ansi(self.raw, ptr::null(), &mut out, &mut err) };
        unsafe { check(status, err)? };
        unsafe { take_c_string(out) }
    }

    /// Export SVG via the two-stage buffer protocol.
    pub fn to_svg(&self) -> Result<Vec<u8>, Error> {
        two_stage(|buf, cap, out_len, err| unsafe {
            sys::tc_chart_to_svg(self.raw, ptr::null(), buf, cap, out_len, err)
        })
    }

    /// Export PNG bytes via the two-stage buffer protocol.
    pub fn to_png(&self) -> Result<Vec<u8>, Error> {
        two_stage(|buf, cap, out_len, err| unsafe {
            sys::tc_chart_to_png(self.raw, ptr::null(), buf, cap, out_len, err)
        })
    }

    /// Set a model property (single property channel; unsupported → Error).
    pub fn set_double_array_prop(&self, prop: sys::tc_chart_prop, values: &[f64]) -> Result<(), Error> {
        let mut value: sys::tc_value = unsafe { std::mem::zeroed() };
        value.kind = sys::tc_value_kind::TC_VAL_DOUBLE_ARRAY;
        value.p = values.as_ptr();
        value.n = values.len() as u64;
        let mut err: sys::tc_error_handle = ptr::null_mut();
        unsafe { check(sys::tc_chart_set_prop(self.raw, prop, &value, &mut err), err) }
    }
}

impl Drop for Chart {
    fn drop(&mut self) {
        unsafe { sys::tc_chart_free(self.raw) }
    }
}

// Chart contains a raw pointer to native state that is not Send/Sync-shared.
// (The native library is single-threaded by contract; we keep !Send to be safe.)
