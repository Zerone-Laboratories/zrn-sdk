# Buttons and toggles

This page covers the controls that the user taps: the push buttons and the controls that hold a state. They look and behave like the Windows Phone 8.1 templates. All of them are `ContentControl`s, so they also take the shared `Control` and `ContentControl` properties listed first.

Widgets on this page:

- [Control and ContentControl (shared properties)](#control-and-contentcontrol)
- [Button](#button)
- [HyperlinkButton](#hyperlinkbutton)
- [RepeatButton](#repeatbutton)
- [ToggleButton](#togglebutton)
- [CheckBox](#checkbox)
- [RadioButton](#radiobutton)
- [ToggleSwitch](#toggleswitch)

Every control is an element identified by a `Zid`. Add it to a panel with `Zpanel_add` and set common layout properties (width, margin, alignment, `Zelement_set_enabled`, `Zelement_set_tooltip`) with the `Zelement_*` functions; see [layout.md](layout.md).

## Control and ContentControl

**What it is.** `Control` holds the visual properties of every control (colors, border, padding, font). `ContentControl` adds a `Content` that is a string or an element. Buttons, CheckBox, RadioButton and ToggleButton derive from it.

**Properties (Control).**

| Function | Meaning | Default |
|---|---|---|
| `Zcontrol_set_foreground(control, ZColor)` | Foreground color | From the template and the theme |
| `Zcontrol_set_background(control, ZColor)` | Background color | From the template |
| `Zcontrol_set_border_brush(control, ZColor)` | Border color | From the template |
| `Zcontrol_set_border_thickness(control, ZThickness)` | Border thickness | From the template |
| `Zcontrol_set_padding(control, ZThickness)` / `Zcontrol_get_padding` | Padding | From the template |
| `Zcontrol_set_font_size(control, float)` / `Zcontrol_get_font_size` | Font size | From the template |
| `Zcontrol_set_font_family(control, ZFontFamily)` | Font family | From the template |
| `Zcontrol_set_horizontal_content_alignment`, `Zcontrol_set_vertical_content_alignment` | Alignment of the content | From the template |
| `Zcontrol_set_tab_stop(control, bool)` | IsTabStop; false keeps the control out of the Tab order | true |
| `Zcontrol_set_tab_index(control, int)` | TabIndex | 0 |
| `Zcontrol_bind_foreground`, `_background`, `_border_brush`, `_border_thickness`, `_padding`, `_font_size` `(control, ZThemeResource key)` | Bind a property to a theme resource key; it then follows the theme, the accent and the overrides of the root. The direct setter removes the binding. Return false for a Zid that is not a control or a key of another type. | none |

**Properties (ContentControl).**

| Function | Meaning | Default |
|---|---|---|
| `Zcontent_control_set_content(control, Zid)` | Content as an element. The control takes ownership and destroys the old content (the new element may be a descendant of the old one). An element that has a parent is moved here; one that would become its own ancestor is refused with `Z_ERROR_CYCLE` and the old content stays. `Z_INVALID_ID` empties it. | empty |
| `Zcontent_control_set_text(control, const char*)` | Content as a string; the control makes a TextBlock with its own font, or changes the text of the TextBlock it already has. An element content is destroyed. | empty |
| `Zcontent_control_get_content(control)` | The content element (the TextBlock of a string), `Z_INVALID_ID` when empty. | |
| `Zcontent_control_get_text(control)` | The text of a string content, `""` for an element content or none. | |

**Notes.**

- A color or size that was never set comes from the template and the theme (see host.h for theme and accent). There is no reset function: set a color to take it, create the control again to drop it.
- Colors that were not set restyle when the theme or the accent of the root changes.

## Button

**What it is.** The WP8.1 `Button`: a 2.5 px outline that fills with the accent while pressed.

**Create.**

```c
ZRNUI_API Zid Zbutton_create(const char* content);
```

**Properties.** The functions below are shared by Button, HyperlinkButton, RepeatButton, ToggleButton, CheckBox and RadioButton (ButtonBase).

| Function | Meaning | Default |
|---|---|---|
| `Zbutton_set_click_mode(button, ZClickMode)` | `Z_CLICK_RELEASE`, `Z_CLICK_PRESS` or `Z_CLICK_HOVER`: when Click fires | `Z_CLICK_RELEASE` |
| `Zbutton_is_pressed(button)` | IsPressed | |
| `Zbutton_is_pointer_over(button)` | IsPointerOver | |
| `Zbutton_click(button)` | Fires Click as if the user had pressed it (Automation Invoke). A ToggleButton also changes its state first (a RadioButton is checked). Does nothing when the button is disabled | |

Size: Button, RepeatButton and ToggleButton have MinWidth 109 and MinHeight 57.5, with 9.5 px of touch overhang above and below the outline. The control is left aligned and vertically centered.

**Events.**

| Event | Subscribe | Callback |
|---|---|---|
| Click | `Zbutton_on_click(button, callback, user_data)` or `Zelement_on(button, Z_EVENT_CLICK, ...)` | `void (*ZRoutedCallback)(Zid sender, void* user_data)` |

Click fires on release inside the button, or per ClickMode (a RepeatButton: see below). A disabled button raises none. A key press works as a pointer press: Space or Enter fires Click on release, or on press with `Z_CLICK_PRESS`. With `Z_CLICK_HOVER` the pointer entering an enabled button fires it. There is one handler per event and element; passing `NULL` removes it.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_save(Zid sender, void* user_data) {
    /* ... */
}

Zid make_button(Zid stack) {
    Zid button = Zbutton_create("save");
    Zbutton_on_click(button, on_save, NULL);
    Zpanel_add(stack, button);
    return button;
}
```

**Notes.**

- Disabling a button (`Zelement_set_enabled`) lets go of a pointer capture it holds: a held press, or a RepeatButton repeating, ends.
- A Click handler may call `Zframe_navigate`; the page and the running button are destroyed after the handler returns (see [navigation.md](navigation.md)).
- A drag on a button past about 10 px along a scrollable axis scrolls the page and cancels the press; see [scrolling.md](scrolling.md).
- A `Button` also has a Flyout (see the flyout functions in controls.h).

## HyperlinkButton

**What it is.** The WP8.1 `HyperlinkButton`: a button that looks like a link. It has no minimum size and no touch overhang.

**Create.**

```c
ZRNUI_API Zid Zhyperlink_button_create(const char* content);
```

**Properties.** ButtonBase functions (see Button) plus:

| Function | Meaning | Default |
|---|---|---|
| `Zhyperlink_button_set_uri(button, const char*)` / `Zhyperlink_button_get_uri` | NavigateUri | none |

**Events.** Click, as for Button.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_link(Zid sender, void* user_data) {
    const char* uri = Zhyperlink_button_get_uri(sender);
    /* open uri with your own code */
    (void)uri;
}

Zid make_link(Zid stack) {
    Zid link = Zhyperlink_button_create("privacy statement");
    Zhyperlink_button_set_uri(link, "https://example.com/privacy");
    Zbutton_on_click(link, on_link, NULL);
    Zpanel_add(stack, link);
    return link;
}
```

**Notes.** The URI is only stored. The program opens it in Click.

## RepeatButton

**What it is.** The WP8.1 `RepeatButton`: fires Click repeatedly while it is held.

**Create.**

```c
ZRNUI_API Zid Zrepeat_button_create(const char* content);
```

**Properties.** ButtonBase functions plus:

| Function | Meaning | Default |
|---|---|---|
| `Zrepeat_button_set_delay(button, int delay_ms)` / `Zrepeat_button_get_delay` | Milliseconds from the press to the first repeat; a negative value is 0 | 250 |
| `Zrepeat_button_set_interval(button, int interval_ms)` / `Zrepeat_button_get_interval` | Milliseconds between repeats; a negative value is 0 | 250 |

**Events.** Click, as for Button.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_step(Zid sender, void* user_data) {
    /* called again and again while held */
}

Zid make_repeat(Zid stack) {
    Zid button = Zrepeat_button_create("hold me");
    Zrepeat_button_set_delay(button, 400);
    Zrepeat_button_set_interval(button, 100);
    Zbutton_on_click(button, on_step, NULL);
    Zpanel_add(stack, button);
    return button;
}
```

**Notes.**

- The first Click fires on the press, whatever the ClickMode: a pointer press, or Space or Enter. It does not fire on hover, and not again on release.
- Click then fires again after Delay, and then every Interval, only while the pointer is held and is on the button; moving off pauses the repeat and moving back resumes it. The repeat needs `Zroot_tick`.
- A held key fires one Click, on the press; the keyboard does not repeat it.

## ToggleButton

**What it is.** The WP8.1 `ToggleButton`: a button that keeps a checked state. It is the base of CheckBox and RadioButton.

**Create.**

```c
ZRNUI_API Zid Ztoggle_button_create(const char* content);
```

**Properties.** ButtonBase functions plus (these also work on CheckBox and RadioButton):

| Function | Meaning | Default |
|---|---|---|
| `Ztoggle_button_set_is_checked(toggle, bool)` / `Ztoggle_button_get_is_checked` | IsChecked as a plain bool for two states | unchecked |
| `Ztoggle_button_set_state(toggle, ZToggleState)` / `Ztoggle_button_get_state` | `Z_UNCHECKED`, `Z_CHECKED` or `Z_INDETERMINATE`; use it for the third state | `Z_UNCHECKED` |
| `Ztoggle_button_set_is_three_state(toggle, bool)` | IsThreeState: a click cycles unchecked, checked, indeterminate; without it a click flips between the two states. A state set by the program is not limited by it | false |

**Events.**

| Event | Subscribe | Callback |
|---|---|---|
| Checked | `Zelement_on(toggle, Z_EVENT_CHECKED, cb, data)` | `ZRoutedCallback` |
| Unchecked | `Zelement_on(toggle, Z_EVENT_UNCHECKED, cb, data)` | `ZRoutedCallback` |
| Indeterminate | `Zelement_on(toggle, Z_EVENT_INDETERMINATE, cb, data)` | `ZRoutedCallback` |
| Click | `Zbutton_on_click` | `ZRoutedCallback` |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_checked(Zid sender, void* user_data) { /* ... */ }

Zid make_toggle(Zid stack) {
    Zid toggle = Ztoggle_button_create("wifi");
    Ztoggle_button_set_is_checked(toggle, true);
    Zelement_on(toggle, Z_EVENT_CHECKED, on_checked, NULL);
    Zpanel_add(stack, toggle);
    return toggle;
}
```

**Notes.**

- Checked, Unchecked or Indeterminate is raised whenever the state changes, also when the program sets it, and never for the state it already has.
- There is one handler per event and element.

## CheckBox

**What it is.** The WP8.1 `CheckBox`: a 25.5 px square with a check path, followed by the content.

**Create.**

```c
ZRNUI_API Zid Zcheck_box_create(const char* content);
```

**Properties.** The ToggleButton functions (IsChecked, state, IsThreeState) and ButtonBase functions. Size: MinWidth 168 with 16 px above and below.

**Events.** Z_EVENT_CHECKED, Z_EVENT_UNCHECKED, Z_EVENT_INDETERMINATE through `Zelement_on`, and Click.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_agree(Zid sender, void* user_data) {
    bool checked = Ztoggle_button_get_is_checked(sender);
    (void)checked;
}

Zid make_check(Zid stack) {
    Zid box = Zcheck_box_create("I agree");
    Zbutton_on_click(box, on_agree, NULL);
    Zpanel_add(stack, box);
    return box;
}
```

**Notes.** Set `Ztoggle_button_set_is_three_state` to allow the indeterminate state.

## RadioButton

**What it is.** The WP8.1 `RadioButton`: one choice out of a group.

**Create.**

```c
ZRNUI_API Zid Zradio_button_create(const char* content, const char* group_name);
```

**Properties.** The ToggleButton and ButtonBase functions, plus:

| Function | Meaning | Default |
|---|---|---|
| `Zradio_button_set_group_name(radio_button, const char*)` | GroupName | the `group_name` of create |

Size: MinWidth 168 with 16 px above and below.

**Events.** Z_EVENT_CHECKED, Z_EVENT_UNCHECKED through `Zelement_on`, and Click.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void make_radios(Zid stack) {
    Zid a = Zradio_button_create("small", "size");
    Zid b = Zradio_button_create("large", "size");
    Zradio_button_set_group_name(b, "size");
    Ztoggle_button_set_is_checked(a, true);
    Zpanel_add(stack, a);
    Zpanel_add(stack, b);
}
```

**Notes.**

- Radio buttons with the same group name exclude each other wherever they are in the tree of the root. A radio button with a `NULL` or `""` group name is grouped with the other unnamed ones that have the same parent.
- A click only checks a radio button; it never unchecks it. Checking one unchecks the others of its group; their Unchecked events come after its Checked.
- Changing the group name of a checked radio button leaves it checked.

## ToggleSwitch

**What it is.** The WP8.1 `ToggleSwitch`: a header, the content of the state and a 77 x 36 switch at the right.

**Create.**

```c
ZRNUI_API Zid Ztoggle_switch_create(void);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Ztoggle_switch_set_is_on(sw, bool)` / `Ztoggle_switch_get_is_on` | IsOn | off |
| `Ztoggle_switch_set_header(sw, const char*)` / `Ztoggle_switch_get_header` | Small text above the content; `""` or `NULL` removes it | none |
| `Ztoggle_switch_set_on_content(sw, const char*)` / `Ztoggle_switch_get_on_content` | Text shown while on | `"On"` |
| `Ztoggle_switch_set_off_content(sw, const char*)` / `Ztoggle_switch_get_off_content` | Text shown while off | `"Off"` |

**Events.**

| Event | Subscribe | Callback |
|---|---|---|
| Toggled | `Ztoggle_switch_on_toggled(sw, cb, data)` or `Zelement_on(sw, Z_EVENT_TOGGLED, ...)` | `ZRoutedCallback` |

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_toggled(Zid sender, void* user_data) {
    bool on = Ztoggle_switch_get_is_on(sender);
    (void)on;
}

Zid make_switch(Zid stack) {
    Zid sw = Ztoggle_switch_create();
    Ztoggle_switch_set_header(sw, "Automatically adjust");
    Ztoggle_switch_on_toggled(sw, on_toggled, NULL);
    Zpanel_add(stack, sw);
    return sw;
}
```

**Notes.**

- A tap on the switch box (77 x 36, at the right) or Space flips it; the header, the On/Off text and the empty space do not take the tap and pass it on. A drag that starts on the knob carries it. The knob slides for 180 ms when animations run.
- Toggled fires whenever IsOn changes, also when the program sets it, never for the same value. Read the state from the sender.
- Header and contents are strings (no templates).

Back to the [documentation index](index.md).
