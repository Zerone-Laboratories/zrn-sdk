fn main() {
    let app = zrnui::App::new("{{ID}}", "{{NAME}}");
    let page = app.page("{{NAME}}");
    page.text("A new ztOS app. Edit src/main.rs, then run `zrn build && zrn render out.png`.");
    let status = page.section("Status").heading("Not tapped yet");
    let app_ref = &app;
    page.button("Tap me").on_click(move || {
        status.set_text("Tapped");
        app_ref.toast("Hello from {{NAME}}");
    });
    std::process::exit(app.run(page));
}
