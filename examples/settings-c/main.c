// SPDX-License-Identifier: MIT
/*
 * A settings app: grouped lists, switches, sliders, a page that opens from a
 * row, a field to type into, and work that takes a while (the busy bar and a
 * toast when it is done). The kind of app most of ztOS is made of.
 */
#include <stdio.h>
#include <string.h>
#include <zrnui.h>

static zui_app *app;
static zui_node *wifi_page, *wifi_row, *join_field, *networks, *update_row;

static void open_wifi(zui_node *row, void *data) { zui_push(wifi_page); }

static void wifi_switched(zui_node *sw, void *data) {
	bool on = zui_get_on(sw);
	zui_set_detail(wifi_row, on ? "Home-5G" : "Off");
	zui_set_visible(networks, on);
}

static void brightness(zui_node *slider, void *data) {
	if (zui_event(slider) == ZUI_EVENT_SUBMIT)
		fprintf(stderr, "brightness set to %.0f%%\n", zui_get_value(slider) * 100);
}

static void dark_theme(zui_node *sw, void *data) {
	zui_app_set_theme(app, zui_get_on(sw) ? "dark" : "light");
}

static void checked(void *data) {
	zui_busy(app, false);
	zui_set_detail(update_row, "Up to date");
	zui_toast(app, "ztOS is up to date");
}

static void check_updates(zui_node *button, void *data) {
	zui_busy(app, true);
	zui_set_detail(update_row, "Checking...");
	zui_timer(app, 1800, false, checked, NULL);
}

static void join(zui_node *field, void *data) {
	if (zui_event(field) != ZUI_EVENT_SUBMIT && field == join_field) return;
	const char *name = zui_get_text(join_field);
	if (!*name) return;
	char line[160];
	snprintf(line, sizeof(line), "Joining %s", name);
	zui_toast(app, line);
}

static void network(zui_node *card, const char *name, const char *how, bool secured) {
	zui_node *row = zui_item(card, name, how, open_wifi, NULL);
	zui_set_icon(row, secured ? "lock" : "wifi");
	zui_set_tint(row, secured ? 0x3b82f6 : 0x10b981);
}

static void build_wifi_page(void) {
	wifi_page = zui_page(app, "Wi-Fi");
	zui_node *top = zui_card(wifi_page);
	zui_node *sw = zui_switch(top, "Wi-Fi", true, wifi_switched, NULL);
	zui_set_subtitle(sw, "Connected to Home-5G");
	zui_set_icon(sw, "wifi");
	networks = zui_column(wifi_page);
	zui_node *list = zui_section(networks, "Networks nearby");
	network(list, "Home-5G", "Connected, 5 GHz", true);
	network(list, "Zerone Lab", "Secured, 2.4 GHz", true);
	network(list, "Cafe Guest", "Open", false);
	zui_node *other = zui_section(networks, "Another network");
	join_field = zui_field(other, "Network name", join, NULL);
	zui_node *row = zui_row(other);
	zui_node *b = zui_button(row, "Join", join, NULL);
	zui_set_flex(b, 1);
}

int main(void) {
	app = zui_app_new("zrn-example-settings", "Settings");
	build_wifi_page();
	zui_node *page = zui_page(app, "Settings");

	zui_node *conn = zui_section(page, "Connections");
	wifi_row = zui_item(conn, "Wi-Fi", NULL, open_wifi, NULL);
	zui_set_icon(wifi_row, "wifi");
	zui_set_detail(wifi_row, "Home-5G");
	zui_node *bt = zui_switch(conn, "Bluetooth", false, NULL, NULL);
	zui_set_icon(bt, "bluetooth");
	zui_set_tint(bt, 0x6366f1);
	zui_node *dnd = zui_switch(conn, "Do not disturb", true, NULL, NULL);
	zui_set_subtitle(dnd, "Until 7:00");
	zui_set_icon(dnd, "moon");
	zui_set_tint(dnd, 0x8b5cf6);

	zui_node *disp = zui_section(page, "Display and sound");
	zui_slider(disp, "Brightness", 0.62, brightness, NULL);
	zui_slider(disp, "Volume", 0.35, NULL, NULL);
	zui_node *dark = zui_switch(disp, "Dark theme", true, dark_theme, NULL);
	zui_set_icon(dark, "sun");
	zui_set_tint(dark, 0xf59e0b);

	zui_node *about = zui_section(page, "This phone");
	update_row = zui_item(about, "Software", "ztOS BETA-1", NULL, NULL);
	zui_set_icon(update_row, "info");
	zui_set_tint(update_row, 0x64748b);
	zui_set_detail(update_row, "Up to date");
	zui_node *bat = zui_item(about, "Battery", "Not charging", NULL, NULL);
	zui_set_icon(bat, "battery");
	zui_set_tint(bat, 0x22c55e);
	zui_set_detail(bat, "84%");
	zui_node *storage = zui_column(about);
	zui_caption(storage, "Storage: 41.2 GB of 64 GB used");
	zui_progress(storage, 0.64);

	zui_node *b = zui_button(page, "Check for updates", check_updates, NULL);
	zui_set_variant(b, ZUI_TONAL);
	return zui_run(app, page);
}
