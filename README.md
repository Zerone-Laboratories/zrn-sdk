# zrn-sdk

Make apps for **ztOS** in C, C++, Rust or Python. They are drawn by
**libzrnui** on the phone's GPU in **zrnCoolStyle** (or Metro, if the user
picks ztRetrofit), ship as one signed **`.zexec`** file, and reach people
through the ztOS store, which builds them from your source.

![zrnCoolStyle, light, and Metro](docs/renders/compare.png)

## Start

```
export PATH="$PWD/tools:$PATH"     # zrn and zrn-exec
zrn doctor                         # what is missing, and the command that fixes it
zrn new "Notes" --lang c           # or cpp, python, rust
cd notes
zrn build
zrn render out.png                 # the first page, as a PNG: no phone, no display needed
zrn run                            # a phone-sized window (nested Wayland compositor)
zrn pack                           # dist/<id>-0.1.0-<arch>.zexec + .minisig
zrn install dist/*.zexec           # into your app list
```

You need Python 3.11+, a C compiler, `make`, `pkg-config` and the development
files for Wayland, Mesa (EGL, GLES2, GBM), FreeType, HarfBuzz, fontconfig,
libpng and xkbcommon. Rust apps need `cargo`. `zrn build` builds libzrnui from
this tree the first time if it is not installed.

## A page

```c
#include <zrnui.h>

static void open_wifi(zui_node *row, void *data) { zui_push(data); }

int main(void) {
	zui_app *app = zui_app_new("io.github.you.settings", "Settings");
	zui_node *wifi = zui_page(app, "Wi-Fi");
	zui_switch(zui_card(wifi), "Wi-Fi", true, NULL, NULL);

	zui_node *page = zui_page(app, "Settings");
	zui_node *conn = zui_section(page, "Connections");     /* a heading and a card */
	zui_node *row = zui_item(conn, "Wi-Fi", NULL, open_wifi, wifi);
	zui_set_icon(row, "wifi");
	zui_set_detail(row, "Home-5G");
	zui_slider(zui_section(page, "Display"), "Brightness", 0.6, NULL, NULL);
	zui_button(page, "Check for updates", NULL, NULL);
	return zui_run(app, page);
}
```

You say what is on a page; libzrnui lays it out, scrolls it, presses it,
animates between pages and draws it on the GPU. The same in the other
languages:

```cpp
zrn::ui::App app("io.github.you.hello", "Hello");            // C++ (zrnui.hpp)
auto page = app.page("Hello");
auto label = page.text("Not tapped yet");
page.button("Tap me", [&] { label.set_text("Tapped"); });
return app.run(page);
```

```rust
let app = zrnui::App::new("io.github.you.hello", "Hello");   // Rust (crate zrnui)
let page = app.page("Hello");
let label = page.text("Not tapped yet");
page.button("Tap me").on_click(move || { label.set_text("Tapped"); });
std::process::exit(app.run(page));
```

```python
import zrnui                                                  # Python (no Qt)
app = zrnui.App("io.github.you.hello", "Hello")
page = app.page("Hello")
label = page.text("Not tapped yet")
page.button("Tap me", lambda: label.set_text("Tapped"))
app.run(page)
```

The full API, the style rules and the package format are in
[DESIGN.md](DESIGN.md); the skill coding agents get is
[templates/agents/SKILL.md](templates/agents/SKILL.md).

## Looking at your app without a phone

```
zrn render out.png                          # first page, 360x740 points at scale 2
zrn render --page "Wi-Fi" --scroll 300 out.png
zrn render --style metro --theme light out.png
zrn render --scale 4 out.png                # the S9's real 1440x2960
zrn run --headless --script "wait 500; tap 180 170; wait 600; snap after.png; quit"
```

`zrn render` draws through the same GLES2 path as the phone (Mesa's
surfaceless EGL). `zrn run --headless` starts a virtual Wayland compositor
(kwin_wayland or weston), runs the app in it and lets a script tap, type, go
back and take pictures.

## Coding agents

```
zrn init-agents            # pick: Claude Code, Codex, Gemini CLI, Cursor, Copilot, OpenCode, Antigravity, Windsurf
zrn init-agents --all      # or --agent claude,codex
```

writes each agent's instruction file and installs the `zrnui` skill in the
project. Every `zrn` command takes `--json`. See [docs/llms.txt](docs/llms.txt).

## Publishing

Push the app to a public GitHub repository, then `zrn submit` shows the entry
for your current commit and `zrn submit --send` sends it. Moderators review it
on the ztOS Mobile Discord; the store builds that exact commit, signs it and
lists it. See the store's CONTRIBUTING.md.

## This repository

| | |
|---|---|
| `libzrnui/` | the C library (`make -C libzrnui`, `make install PREFIX=/usr`) |
| `python/zrnui/` | the Python binding |
| `rust/` | the Rust crate |
| `tools/` | `zrn`, `zrn-exec` |
| `templates/` | what `zrn new` and `zrn init-agents` write |
| `examples/` | hello in C, C++, Rust and Python; a settings app in C |
| `tests/` | `make check` |
| `docs/renders/` | pictures of every example in both styles; `docs/evidence/` packages |

`make check` runs the C tests (layout, touch, typing, an offscreen frame
checked pixel by pixel), the tool tests (signatures, packages, the CLI's
JSON contract, the Python binding) and the Rust tests.
