#include <zrnui.hpp>

int main() {
	zrn::ui::App app("{{ID}}", "{{NAME}}");
	auto page = app.page("{{NAME}}");
	page.text("A new ztOS app. Edit main.cpp, then run `zrn build && zrn render out.png`.");
	auto status = page.section("Status").heading("Not tapped yet");
	page.button("Tap me", [&]() {
		status.set_text("Tapped");
		app.toast("Hello from {{NAME}}");
	});
	return app.run(page);
}
