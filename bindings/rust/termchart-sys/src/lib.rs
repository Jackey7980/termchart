//! Raw FFI bindings to `termchart_capi.h`.
//!
//! Hand-written rather than bindgen-generated: the header is a **frozen C ABI**
//! (POD structs + opaque handles + `tc_status`), so a checked-in binding is stable
//! and adds no build-time dependency on libclang. Every declaration below mirrors
//! `libs/capi/include/termchart_capi.h` exactly; keep the two in sync when the ABI
//! grows (only additions are allowed by the C ABI contract).
//!
//! Linking: this crate emits `cargo:rustc-link-lib=termchart_capi`. The native
//! library must be built with `-DTERMCHART_BUILD_CAPI=ON`; point
//! `TERMCHART_CAPI_DIR` (or `LIBRARY_PATH`) at the directory containing
//! `libtermchart_capi.{so,dll,lib}` before `cargo build`. See the crate README.

#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

use std::ffi::{c_char, c_int, c_void};

/// `tc_status`: 0 = OK, non-zero = error class (see `tc_status_code` equivalents below).
pub type tc_status = c_int;
pub type tc_error_handle = *mut c_void;
pub type tc_chart_handle = *mut c_void;

/// Error codes from `enum tc_status_code` — append-only, values are ABI.
pub const TC_OK: tc_status = 0;
pub const TC_ERR_INVALID_HANDLE: tc_status = 1;
pub const TC_ERR_INVALID_ARG: tc_status = 2;
pub const TC_ERR_RENDER: tc_status = 3;
pub const TC_ERR_NOMEM: tc_status = 4;
pub const TC_ERR_IO: tc_status = 5;
pub const TC_ERR_BUFFER_TOO_SMALL: tc_status = 6;
pub const TC_ERR_UNSUPPORTED: tc_status = 7;

/// `tc_color_depth` — mirrors `core::ColorDepth` exactly.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum tc_color_depth {
    TC_DEPTH_NONE = 0,
    TC_DEPTH_16 = 1,
    TC_DEPTH_256 = 2,
    TC_DEPTH_TRUECOLOR = 3,
}

/// `tc_color_kind`.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum tc_color_kind {
    TC_COLOR_NONE = 0,
    TC_COLOR_ANSI16 = 1,
    TC_COLOR_INDEXED = 2,
    TC_COLOR_RGB = 3,
}

/// `tc_color` POD.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_color {
    pub kind: tc_color_kind,
    pub index: u8,
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

/// `tc_style` POD (attrs bitmask: 1=Bold 2=Dim 4=Italic 8=Underline 16=Reverse).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_style {
    pub fg: tc_color,
    pub bg: tc_color,
    pub attrs: u16,
}

/// `tc_api_opts` POD (text entry options).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_api_opts {
    pub width: u32,
    pub height: u32,
    pub depth: tc_color_depth,
    pub has_depth: u8,
    pub unicode: u8,
    pub newline: u8,
    pub reset_at_end: u8,
}

/// `tc_plot_opts` POD (chart display options).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_plot_opts {
    pub width: u32,
    pub height: u32,
    pub title: *const c_char,
    pub depth: tc_color_depth,
    pub show_legend: u8,
}

/// `tc_value_kind` — variant tag for the generic property value carrier.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum tc_value_kind {
    TC_VAL_NONE = 0,
    TC_VAL_BOOL = 1,
    TC_VAL_INT = 2,
    TC_VAL_DOUBLE = 3,
    TC_VAL_STRING = 4,
    TC_VAL_DOUBLE_ARRAY = 5,
}

/// `tc_value` POD — zero-init (or kind=NONE) means "clear property".
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_value {
    pub kind: tc_value_kind,
    pub i: i64,
    pub d: f64,
    pub s: *const c_char,
    pub n: u64,
    pub p: *const f64,
}

/// `tc_series` POD — one series, bulk point arrays (zero-copy on the C++ side).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct tc_series {
    pub name: *const c_char,
    pub color: tc_color,
    pub x: *const f64,
    pub y: *const f64,
    pub n: u64,
}

/// `tc_chart_prop` — model properties (append-only; new prop = new enum + one switch arm).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum tc_chart_prop {
    TC_PROP_Y_DOMAIN = 1,
    TC_PROP_AUTO_DOMAIN = 2,
    TC_PROP_INCLUDE_ZERO = 3,
    TC_PROP_EMPHASIZE_ZERO = 4,
    TC_PROP_SHADE_NEGATIVE = 5,
    TC_PROP_X_TICKS = 6,
    TC_PROP_X_LABEL = 7,
    TC_PROP_Y_LABEL = 8,
}

#[link(name = "termchart_capi")]
extern "C" {
    pub fn tc_version_string() -> *const c_char;

    pub fn tc_init();
    pub fn tc_core_initialized() -> u8;

    pub fn tc_api_opts_default() -> tc_api_opts;
    pub fn tc_style_default() -> tc_style;

    pub fn tc_render_line(
        text: *const c_char,
        style: *const tc_style,
        opts: *const tc_api_opts,
        out_ansi: *mut *mut c_char,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_print_line(
        text: *const c_char,
        style: *const tc_style,
        opts: *const tc_api_opts,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_hello(opts: *const tc_api_opts, err: *mut tc_error_handle) -> tc_status;

    pub fn tc_chart_create(kind: *const c_char, err: *mut tc_error_handle) -> tc_chart_handle;
    pub fn tc_chart_kinds(
        buf: *mut c_char,
        cap: u64,
        out_len: *mut u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_chart_free(chart: tc_chart_handle);

    pub fn tc_chart_add_points(
        chart: tc_chart_handle,
        x: *const f64,
        y: *const f64,
        n: u64,
    ) -> tc_status;
    pub fn tc_chart_add_bar(
        chart: tc_chart_handle,
        values: *const f64,
        n: u64,
        labels: *const *const c_char,
        n_labels: u64,
    ) -> tc_status;
    pub fn tc_chart_set_values(
        chart: tc_chart_handle,
        values: *const f64,
        n: u64,
        labels: *const *const c_char,
        n_labels: u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_chart_add_series(
        chart: tc_chart_handle,
        series: *const tc_series,
        count: u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_chart_set_matrix(
        chart: tc_chart_handle,
        rows: *const *const f64,
        row_count: u64,
        col_count: u64,
        row_labels: *const *const c_char,
        n_row_labels: u64,
        col_labels: *const *const c_char,
        n_col_labels: u64,
        err: *mut tc_error_handle,
    ) -> tc_status;

    pub fn tc_chart_render_ansi(
        chart: tc_chart_handle,
        opts: *const tc_plot_opts,
        out_ansi: *mut *mut c_char,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_chart_print(
        chart: tc_chart_handle,
        opts: *const tc_plot_opts,
        err: *mut tc_error_handle,
    ) -> tc_status;

    pub fn tc_chart_set_prop(
        chart: tc_chart_handle,
        prop: tc_chart_prop,
        value: *const tc_value,
        err: *mut tc_error_handle,
    ) -> tc_status;

    pub fn tc_chart_to_svg(
        chart: tc_chart_handle,
        opts: *const tc_plot_opts,
        buf: *mut c_char,
        cap: u64,
        out_len: *mut u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_chart_to_png(
        chart: tc_chart_handle,
        opts: *const tc_plot_opts,
        buf: *mut c_char,
        cap: u64,
        out_len: *mut u64,
        err: *mut tc_error_handle,
    ) -> tc_status;

    pub fn tc_write_stdout(
        bytes: *const c_char,
        n: u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_write_stderr(
        bytes: *const c_char,
        n: u64,
        err: *mut tc_error_handle,
    ) -> tc_status;
    pub fn tc_flush_stdout(err: *mut tc_error_handle) -> tc_status;
    pub fn tc_auto_plot(filepath: *const c_char, err: *mut tc_error_handle) -> tc_status;

    pub fn tc_free_string(s: *mut c_char);
    pub fn tc_error_message(err: tc_error_handle, buf: *mut c_char, cap: u64);
    pub fn tc_error_free(err: tc_error_handle);
}
