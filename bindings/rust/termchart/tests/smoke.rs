//! 集成冒烟：one-liner 出图 + 错误通道 + 两段式导出。
//! 需要 native `termchart_capi` 库在链接/运行路径上（见 bindings/rust/README.md）。

use termchart as tc;

#[test]
fn version_and_kinds() {
    assert!(!tc::version().is_empty());
    let kinds = tc::kinds().expect("kinds");
    assert!(kinds.contains("line"), "kinds missing line: {kinds}");
}

#[test]
fn one_liners_produce_ansi() {
    let x: Vec<f64> = (0..8).map(|i| i as f64).collect();
    let y: Vec<f64> = x.iter().map(|v| v * v).collect();
    let ansi = tc::plot(&x, &y).expect("plot");
    assert!(ansi.contains("\u{1b}["), "plot has no ANSI SGR");

    let bars = tc::bar(&[1.0, 2.0, 3.0]).expect("bar");
    assert!(!bars.is_empty());
}

#[test]
fn empty_input_fails_loud() {
    let err = tc::plot(&[], &[]).expect_err("empty plot must fail");
    assert_eq!(err.code, tc::ffi::TC_ERR_INVALID_ARG);
}

#[test]
fn two_stage_export() {
    let chart = tc::Chart::line().expect("chart");
    chart.add_points(&[0.0, 1.0, 2.0], &[1.0, 3.0, 2.0]).expect("points");
    let svg = chart.to_svg().expect("svg");
    assert!(!svg.is_empty());
    let png = chart.to_png().expect("png");
    assert!(png.len() > 8, "png too small: {}", png.len());
}
