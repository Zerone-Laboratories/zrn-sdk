#include <stdio.h>
#include <zrnui.h>

static zui_node *status;

static void tapped(zui_node *button, void *data) {
	zui_set_text(status, "Tapped");
	zui_toast(zui_node_app(button), "Hello from {{NAME}}");
}

int main(void) {
	zui_app *app = zui_app_new("{{ID}}", "{{NAME}}");
	zui_node *page = zui_page(app, "{{NAME}}");
	zui_text(page, "A new ztOS app. Edit main.c, then run `zrn build && zrn render out.png`.");
	zui_node *card = zui_section(page, "Status");
	status = zui_heading(card, "Not tapped yet");
	zui_button(page, "Tap me", tapped, NULL);
	return zui_run(app, page);
}
