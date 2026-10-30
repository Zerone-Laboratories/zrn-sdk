# SPDX-License-Identifier: MIT
"""Hello in Python: the same library as C, no Qt."""
import zrnui

app = zrnui.App("zrn-example-hello-py", "Hello Python")
page = app.page("Hello, Python")
page.text("This page is drawn by libzrnui on the GPU; Python only says what is on it.")
card = page.section("Counter")
label = card.heading("Not tapped yet")
taps = 0


def tapped():
    global taps
    taps += 1
    label.set_text(f"Tapped {taps} time" + ("s" if taps > 1 else ""))


page.button("Tap me", tapped)
name = page.section("Your name").field("Type a name", on_submit=lambda t: app.toast(f"Hello, {t}"))
app.run(page)
