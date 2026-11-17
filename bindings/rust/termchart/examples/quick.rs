//! One-liner plotting from Rust — mirrors examples/59_quick_plot.cpp.
//!
//! Run (after building the native capi lib, see README):
//!   TERMCHART_CAPI_DIR=/path/to/lib cargo run -p termchart --example quick

fn main() -> Result<(), Box<dyn std::error::Error>> {
    termchart::init();
    println!("termchart {}", termchart::version());

    let x: Vec<f64> = (0..20).map(|i| i as f64 * 0.3).collect();
    let y: Vec<f64> = x.iter().map(|v| v.sin()).collect();
    let ansi = termchart::plot(&x, &y)?;
    print!("{ansi}");

    let bars = termchart::bar(&[3.0, 7.0, 5.0, 9.0])?;
    print!("{bars}");

    let chart = termchart::Chart::line()?;
    chart.add_points(&x, &y)?;
    let svg = chart.to_svg()?;
    println!("svg bytes: {}", svg.len());
    Ok(())
}
