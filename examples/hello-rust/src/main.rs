// SPDX-License-Identifier: MIT
//! Hello in Rust: the same library as C, closures for callbacks.

fn main() {
    let app = zrnui::App::new("zrn-example-hello-rust", "Hello Rust");
    let page = app.page("Hello, Rust");
    page.text("Closures for callbacks; nodes cannot outlive the app that owns them.");
    let card = page.section("Counter");
    let label = card.heading("Not tapped yet");
    let double = card.switch("Count by two", false).set_icon("plus");
    let mut taps = 0;
    page.button("Tap me").on_click(move || {
        taps += if double.is_on() { 2 } else { 1 };
        label.set_text(&format!("Tapped {taps} {}", if taps == 1 { "time" } else { "times" }));
    });
    let brightness = page.section("A slider");
    brightness.slider("Volume", 0.4).on_slide(|v, done| {
        if done {
            eprintln!("volume {:.0}%", v * 100.0);
        }
    });
    std::process::exit(app.run(page));
}
