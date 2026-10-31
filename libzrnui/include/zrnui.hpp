// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * libzrnui for C++: the same library as zrnui.h, with lambdas for callbacks
 * and an App that frees everything when it goes out of scope. Header only.
 *
 *	#include <zrnui.hpp>
 *	int main() {
 *		zrn::ui::App app("zrn-hello-cpp", "Hello");
 *		auto page = app.page("Hello");
 *		auto label = page.text("Not tapped yet");
 *		int taps = 0;
 *		page.button("Tap me", [=]() mutable {
 *			label.set_text("Tapped " + std::to_string(++taps) + " times");
 *		});
 *		return app.run(page);
 *	}
 *
 * A Node is a handle (copy it freely); the App owns what it points at.
 */
#ifndef ZRNUI_HPP
#define ZRNUI_HPP

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "zrnui.h"

namespace zrn::ui {

enum class Variant { Primary = ZUI_PRIMARY, Tonal = ZUI_TONAL, Plain = ZUI_PLAIN, Danger = ZUI_DANGER };

namespace detail {
using Handler = std::function<void(zui_node *)>;
inline void call(zui_node *n, void *data) { (*static_cast<Handler *>(data))(n); }
inline void drop(void *data) { delete static_cast<Handler *>(data); }
inline void attach(zui_node *n, Handler h) {
	if (!h) return;
	zui_set_handler(n, call, new Handler(std::move(h)), drop);
}
}  // namespace detail

class Node {
public:
	Node() = default;
	explicit Node(zui_node *n) : n_(n) {}
	zui_node *get() const { return n_; }
	explicit operator bool() const { return n_ != nullptr; }

	/* what goes in it */
	Node column() { return Node(zui_column(n_)); }
	Node row() { return Node(zui_row(n_)); }
	Node card() { return Node(zui_card(n_)); }
	Node section(const std::string &title) { return Node(zui_section(n_, title.c_str())); }
	Node space(double points) { return Node(zui_space(n_, points)); }
	Node text(const std::string &s) { return Node(zui_text(n_, s.c_str())); }
	Node heading(const std::string &s) { return Node(zui_heading(n_, s.c_str())); }
	Node caption(const std::string &s) { return Node(zui_caption(n_, s.c_str())); }

	Node button(const std::string &label, std::function<void()> on_click = {}) {
		Node b(zui_button(n_, label.c_str(), nullptr, nullptr));
		if (on_click) detail::attach(b.n_, [f = std::move(on_click)](zui_node *) { f(); });
		return b;
	}
	Node item(const std::string &title, const std::string &subtitle = {}, std::function<void()> on_click = {}) {
		Node i(zui_item(n_, title.c_str(), subtitle.empty() ? nullptr : subtitle.c_str(), nullptr, nullptr));
		if (on_click) detail::attach(i.n_, [f = std::move(on_click)](zui_node *) { f(); });
		return i;
	}
	/* a switch ("switch" is taken in C++) */
	Node toggle(const std::string &title, bool on, std::function<void(bool)> on_change = {}) {
		Node s(zui_switch(n_, title.c_str(), on, nullptr, nullptr));
		if (on_change) detail::attach(s.n_, [f = std::move(on_change)](zui_node *n) { f(zui_get_on(n)); });
		return s;
	}
	/* on_change(value, done): done when the finger lets go */
	Node slider(const std::string &title, double value, std::function<void(double, bool)> on_change = {}) {
		Node s(zui_slider(n_, title.c_str(), value, nullptr, nullptr));
		if (on_change)
			detail::attach(s.n_, [f = std::move(on_change)](zui_node *n) {
				f(zui_get_value(n), zui_event(n) == ZUI_EVENT_SUBMIT);
			});
		return s;
	}
	Node progress(double value) { return Node(zui_progress(n_, value)); }
	/* on_change(text, submitted): submitted on Enter */
	Node field(const std::string &placeholder, std::function<void(const std::string &, bool)> on_change = {}) {
		Node f(zui_field(n_, placeholder.c_str(), nullptr, nullptr));
		if (on_change)
			detail::attach(f.n_, [cb = std::move(on_change)](zui_node *n) {
				cb(zui_get_text(n), zui_event(n) == ZUI_EVENT_SUBMIT);
			});
		return f;
	}

	/* changing it; each returns the node, so they chain */
	Node &set_text(const std::string &s) { zui_set_text(n_, s.c_str()); return *this; }
	Node &set_detail(const std::string &s) { zui_set_detail(n_, s.c_str()); return *this; }
	Node &set_subtitle(const std::string &s) { zui_set_subtitle(n_, s.c_str()); return *this; }
	Node &set_value(double v) { zui_set_value(n_, v); return *this; }
	Node &set_on(bool on) { zui_set_on(n_, on); return *this; }
	Node &set_enabled(bool e) { zui_set_enabled(n_, e); return *this; }
	Node &set_visible(bool v) { zui_set_visible(n_, v); return *this; }
	Node &set_variant(Variant v) { zui_set_variant(n_, static_cast<int>(v)); return *this; }
	Node &set_flex(double f) { zui_set_flex(n_, f); return *this; }
	Node &set_icon(const std::string &name) { zui_set_icon(n_, name.c_str()); return *this; }
	Node &set_tint(uint32_t rgb) { zui_set_tint(n_, rgb); return *this; }
	/* any callback, with the node and why it ran (zui_event) */
	Node &on(std::function<void(Node, int)> f) {
		detail::attach(n_, [f = std::move(f)](zui_node *n) { f(Node(n), zui_event(n)); });
		return *this;
	}

	std::string text_value() const { return zui_get_text(n_); }
	double value() const { return zui_get_value(n_); }
	bool is_on() const { return zui_get_on(n_); }
	void clear() { zui_clear(n_); }
	void destroy() { zui_destroy(n_); n_ = nullptr; }
	void push() { zui_push(n_); }

private:
	zui_node *n_ = nullptr;
};

class App {
public:
	App(const std::string &id, const std::string &title) : app_(zui_app_new(id.c_str(), title.c_str())) {}
	~App() {
		zui_app_free(app_);
		for (auto &t : timers_) delete t.second;
	}
	App(const App &) = delete;
	App &operator=(const App &) = delete;

	zui_app *get() const { return app_; }
	Node page(const std::string &title) { return Node(zui_page(app_, title.c_str())); }
	int run(Node first) { return zui_run(app_, first.get()); }
	void push(Node page) { zui_push(page.get()); }
	void back() { zui_back(app_); }
	void quit() { zui_quit(app_); }
	void busy(bool b) { zui_busy(app_, b); }
	void toast(const std::string &s) { zui_toast(app_, s.c_str()); }
	void set_style(const std::string &s) { zui_app_set_style(app_, s.c_str()); }
	void set_theme(const std::string &s) { zui_app_set_theme(app_, s.c_str()); }
	void set_accent(uint32_t rgb) { zui_app_set_accent(app_, rgb); }
	std::string style() const { return zui_app_style(app_); }
	int render_png(Node page, const std::string &path, int w = 360, int h = 740, int scale = 2) {
		return zui_render_png(app_, page.get(), path.c_str(), w, h, scale);
	}

	/* f() after ms (and every ms if repeat); returns an id for cancel_timer */
	int timer(int ms, bool repeat, std::function<void()> f) {
		auto *slot = new Slot{ this, 0, repeat, std::move(f) };
		int id = zui_timer(app_, ms, repeat, &App::fire, slot);
		if (!id) { delete slot; return 0; }
		slot->id = id;
		timers_[id] = slot;
		return id;
	}
	void cancel_timer(int id) {
		auto it = timers_.find(id);
		if (it == timers_.end()) return;
		zui_timer_cancel(app_, id);
		delete it->second;
		timers_.erase(it);
	}
	/* from any thread: f() on the app's thread */
	void post(std::function<void()> f) {
		zui_post(app_, [](void *d) {
			std::unique_ptr<std::function<void()>> g(static_cast<std::function<void()> *>(d));
			(*g)();
		}, new std::function<void()>(std::move(f)));
	}

private:
	struct Slot { App *app; int id; bool repeat; std::function<void()> f; };
	static void fire(void *d) {
		auto *s = static_cast<Slot *>(d);
		auto f = s->f;	/* the slot may go if f cancels it */
		if (!s->repeat) {
			s->app->timers_.erase(s->id);
			delete s;
		}
		f();
	}
	zui_app *app_;
	std::map<int, Slot *> timers_;
};

}  // namespace zrn::ui

#endif
