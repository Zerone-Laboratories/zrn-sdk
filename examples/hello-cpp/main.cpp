// SPDX-License-Identifier: MIT
// Hello in C++: the same library as C, with lambdas.
#include <string>
#include <zrnui.hpp>

int main() {
	zrn::ui::App app("zrn-example-hello-cpp", "Hello C++");
	auto page = app.page("Hello, C++");
	page.text("Callbacks are lambdas; the App frees everything when it goes.");
	auto card = page.card();
	auto label = card.heading("Not tapped yet");
	auto toggle = card.toggle("Count by two", false);
	toggle.set_icon("plus");
	int taps = 0;
	page.button("Tap me", [&]() {
		taps += toggle.is_on() ? 2 : 1;
		label.set_text("Tapped " + std::to_string(taps) + (taps == 1 ? " time" : " times"));
	});
	page.button("Reset", [&]() {
		taps = 0;
		label.set_text("Not tapped yet");
		app.toast("Counter reset");
	}).set_variant(zrn::ui::Variant::Plain);
	return app.run(page);
}
