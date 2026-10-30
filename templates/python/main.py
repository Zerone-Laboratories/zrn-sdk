import zrnui

app = zrnui.App("{{ID}}", "{{NAME}}")
page = app.page("{{NAME}}")
page.text("A new ztOS app. Edit main.py, then run `zrn render out.png`.")
status = page.section("Status").heading("Not tapped yet")


def tapped():
    status.set_text("Tapped")
    app.toast("Hello from {{NAME}}")


page.button("Tap me", tapped)
app.run(page)
