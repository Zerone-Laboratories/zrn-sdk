# App bars and icons

The bar a Page shows over its top or bottom (AppBar, CommandBar), its buttons and separator, and the icons that go into them. Page placement of bars is in [navigation.md](navigation.md); buttons in general in [buttons.md](buttons.md).

Widgets on this page:

- [AppBar](#appbar)
- [CommandBar](#commandbar)
- [AppBarButton](#appbarbutton)
- [AppBarToggleButton](#appbartogglebutton)
- [AppBarSeparator](#appbarseparator)
- [SymbolIcon](#symbolicon)
- [FontIcon](#fonticon)
- [PathIcon](#pathicon)
- [BitmapIcon](#bitmapicon)

An `IconElement` is not a control. An icon draws in the foreground of the control it is in (the white outline of an AppBarButton, the background color while it is pressed) unless `Zicon_element_set_foreground(icon, color)` gives it one. In the circle of an AppBarButton an icon is 20 x 20.

## AppBar

**What it is.** The Windows Phone 8.1 `AppBar`. Closed it is 57.5 px tall, or 35 px as Minimal; the page gives its content what is left. Three dots at the right edge (all of a Minimal bar) open it: a tap, or `IsOpen`. Open, it grows over the page in 167 ms and the buttons show their labels.

**Create**

```c
Zid Zapp_bar_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zapp_bar_set_is_open(bar, on)` / `Zapp_bar_get_is_open(bar)` | `IsOpen`. Only one bar is open per root: opening a bar that is in a tree closes the one that is open (its Closed comes first) | false |
| `Zapp_bar_set_is_sticky(bar, on)` / `Zapp_bar_get_is_sticky(bar)` | true keeps an open bar open through a press outside it and Back | false |
| `Zapp_bar_set_closed_display_mode(bar, mode)` / `Zapp_bar_get_closed_display_mode(bar)` | `Z_APP_BAR_CLOSED_COMPACT` or `Z_APP_BAR_CLOSED_MINIMAL` | Compact |

**Events.** `Zapp_bar_on_opened(bar, cb, ud)`, `Zapp_bar_on_closed(bar, cb, ud)`; callback `void (*)(Zid sender, void* user_data)`. Also `Z_EVENT_OPENED` / `Z_EVENT_CLOSED` via `Zelement_on`. They come when `IsOpen` changes, not when the animation ends.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_minimal_bar(void) {
	Zid bar = Zapp_bar_create();
	Zapp_bar_set_closed_display_mode(bar, Z_APP_BAR_CLOSED_MINIMAL);
	Zapp_bar_set_is_sticky(bar, true);
	return bar;
}
```

**Notes**

- A press outside an open bar that is not sticky closes it and is not given to what is under it; so does Back.
- A tapped secondary command (a toggle button too) closes the bar.
- A root has one open bar at a time: opening one bar closes the other, so the light dismiss and Back have one bar to close.
- The phone template of CommandBar is a stub (the shell draws the real bar): the dots, the Minimal size, the secondary rows and the separator are the library's own. The Compact bar is 57.5 px, not the 68 of other sources.

## CommandBar

**What it is.** The 8.1 `CommandBar`, an AppBar with primary commands (buttons at the right) and secondary commands (rows of text when open). Its Content (`Zcontent_control_set_content`) is at the left.

**Create**

```c
Zid Zcommand_bar_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zcommand_bar_add_primary_command(bar, cmd)` | Appends an AppBarButton, AppBarToggleButton or AppBarSeparator; the bar takes ownership; returns the index, -1 on failure (an invalid handle, a command of another class, or one that has a parent) | |
| `Zcommand_bar_add_secondary_command(bar, cmd)` | Same for the secondary commands | |
| `Zcommand_bar_get_primary_command_count(bar)` / `Zcommand_bar_get_secondary_command_count(bar)` | | |
| `Zcommand_bar_get_primary_command(bar, i)` / `Zcommand_bar_get_secondary_command(bar, i)` | | |
| `Zcommand_bar_clear_primary_commands(bar)` / `Zcommand_bar_clear_secondary_commands(bar)` | The commands are destroyed | |

**Events.** The AppBar events (`Zapp_bar_on_opened`, `Zapp_bar_on_closed`) work on it; commands raise their own Click.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_add(Zid sender, void* user_data) { (void)sender; (void)user_data; }

Zid make_bar(void) {
	Zid bar = Zcommand_bar_create();
	Zid add = Zapp_bar_button_create("add", Zsymbol_icon_create(Z_SYMBOL_ADD));
	Zbutton_on_click(add, on_add, NULL);
	Zcommand_bar_add_primary_command(bar, add);
	Zcommand_bar_add_primary_command(bar, Zapp_bar_toggle_button_create("pin", Zsymbol_icon_create(Z_SYMBOL_PIN)));
	Zcommand_bar_add_secondary_command(bar, Zapp_bar_button_create("settings", Z_INVALID_ID));
	return bar;
}
```

**Notes**

- A CommandBar sets IsCompact on its buttons while it is closed.

## AppBarButton

**What it is.** The 8.1 `AppBarButton`: a 38.5 px circle with a 2.5 px outline, the icon in it and its label under it (9.5 px type; the item is 80 px wide and the label 70.5 px inside it). Pressed, it fills with the foreground and the icon takes the background color.

**Create**

```c
Zid Zapp_bar_button_create(const char* label, Zid icon);
```

`icon` is an IconElement the button takes, or `Z_INVALID_ID`.

**Properties** (the same functions work on an AppBarToggleButton)

| Function | Meaning | Default |
|---|---|---|
| `Zapp_bar_button_set_label(b, text)` / `Zapp_bar_button_get_label(b)` | Label | |
| `Zapp_bar_button_set_icon(b, icon)` / `Zapp_bar_button_get_icon(b)` | Icon (the Content of the button) | |
| `Zapp_bar_button_set_is_compact(b, on)` / `Zapp_bar_button_get_is_compact(b)` | Hides the label and makes the button 48 wide | false |

**Events.** It is a Button: `Zbutton_on_click(b, ZRoutedCallback, user_data)` (or `Z_EVENT_CLICK`), and `Zbutton_set_flyout` works on it.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_refresh(Zid sender, void* user_data) { (void)sender; (void)user_data; }

Zid make_refresh(void) {
	Zid refresh = Zapp_bar_button_create("refresh", Zsymbol_icon_create(Z_SYMBOL_REFRESH));
	Zbutton_on_click(refresh, on_refresh, NULL);
	return refresh;
}
```

## AppBarToggleButton

**What it is.** An AppBarButton that keeps a checked state (a `ToggleButton`); drawn 11 px under the top (12 for the plain button). A checked one looks pressed.

**Create**

```c
Zid Zapp_bar_toggle_button_create(const char* label, Zid icon);
```

**Properties.** Label, Icon, IsCompact via the `Zapp_bar_button_*` functions above; the checked state via `Ztoggle_button_set_is_checked(toggle, is_checked)` and `Ztoggle_button_set_is_three_state`.

**Events.** `Z_EVENT_CHECKED`, `Z_EVENT_UNCHECKED`, `Z_EVENT_INDETERMINATE`, `Z_EVENT_CLICK` via `Zelement_on`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_pin(void) {
	Zid pin = Zapp_bar_toggle_button_create("pin", Zsymbol_icon_create(Z_SYMBOL_PIN));
	Ztoggle_button_set_is_checked(pin, true);
	return pin;
}
```

## AppBarSeparator

**What it is.** `AppBarSeparator`, a divider between commands.

**Create**

```c
Zid Zapp_bar_separator_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zapp_bar_separator_set_is_compact(s, on)` / `Zapp_bar_separator_get_is_compact(s)` | `IsCompact` | false |

**Events.** None.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void add_separator(Zid bar) {   // bar: a CommandBar
	Zcommand_bar_add_primary_command(bar, Zapp_bar_separator_create());
}
```

## SymbolIcon

**What it is.** `SymbolIcon`: a `Symbol` drawn as vector shapes.

**Create**

```c
Zid Zsymbol_icon_create(ZSymbol symbol);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zsymbol_icon_set_symbol(icon, symbol)` / `Zsymbol_icon_get_symbol(icon)` | The `ZSymbol` (`Z_SYMBOL_ADD`, `Z_SYMBOL_ACCEPT`, `Z_SYMBOL_CANCEL`, `Z_SYMBOL_DELETE`, `Z_SYMBOL_EDIT`, `Z_SYMBOL_SAVE`, `Z_SYMBOL_FIND`, `Z_SYMBOL_SETTING`, ...) | as created |
| `Zicon_element_set_foreground(icon, color)` | `IconElement.Foreground` | the control's foreground |

**Events.** None.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_search(void) {
	Zid icon = Zsymbol_icon_create(Z_SYMBOL_FIND);
	return Zapp_bar_button_create("search", icon);
}
```

**Notes**

- Vector drawings exist for Accept, Add, Back, Cancel, Clear, Delete, Download, Edit, Favorite, Find, Forward, Home, Mail, MailFilled, More, Next, OutlineStar, Pause, Pin, Play, Previous, Refresh, Remove, Save, SaveLocal, Send, Setting, SolidStar, Stop, Sync, Up and Upload; any other Symbol is an empty box.
- The 8.1 Symbol has Find and Setting, no Search and Settings, and no Share and no Down.

## FontIcon

**What it is.** `FontIcon`: a glyph drawn with the text engine.

**Create**

```c
Zid Zfont_icon_create(const char* glyph);   // UTF-8
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zfont_icon_set_glyph(icon, glyph)` / `Zfont_icon_get_glyph(icon)` | The glyph (UTF-8) | as created |
| `Zfont_icon_set_font_size(icon, size)` | Font size | 20 |
| `Zfont_icon_set_font_family(icon, family)` | A `ZFontFamily` | Selawik |

**Events.** None.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_letter(void) {
	Zid icon = Zfont_icon_create("A");
	Zfont_icon_set_font_size(icon, 18);
	return Zapp_bar_button_create("letter", icon);
}
```

## PathIcon

**What it is.** `PathIcon`: a geometry, drawn as filled polygons that fit the icon uniformly.

**Create**

```c
Zid Zpath_icon_create(const char* data);
```

`data` is `Path.Data` in the mini language "M x,y L x,y H x V y Z" (also lower-case m, l, h, v, z). `NULL` makes an empty icon. Returns `Z_INVALID_ID` when the data is not understood (`Z_ERROR_INVALID_ARGUMENT` is reported); `""`, or data without a point, is not understood.

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `bool Zpath_icon_set_data(icon, data)` | New path; returns false, icon unchanged, when `data` is NULL or not understood | |

**Events.** None.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_triangle(void) {
	Zid icon = Zpath_icon_create("M 0,0 L 10,0 L 5,10 Z");
	if (icon == Z_INVALID_ID) return Z_INVALID_ID;
	return Zapp_bar_button_create("tri", icon);
}
```

**Notes**

- Every sub-path is a filled polygon (no curves, no strokes).

## BitmapIcon

**What it is.** `BitmapIcon`: a PNG file drawn as it is.

**Create**

```c
Zid Zbitmap_icon_create(const char* png_path);
```

`NULL` makes an empty icon. Returns `Z_INVALID_ID` when the file cannot be read as a PNG (`Z_ERROR_IO` is reported).

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `bool Zbitmap_icon_set_uri_source(icon, png_path)` | New PNG; returns false, icon unchanged, when `png_path` is NULL or cannot be read as a PNG | |

**Events.** None.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_logo(void) {
	Zid icon = Zbitmap_icon_create("assets/logo.png");
	if (icon == Z_INVALID_ID) return Z_INVALID_ID;
	return Zapp_bar_button_create("logo", icon);
}
```

**Notes**

- It is not tinted with the foreground.

Back to the [documentation index](index.md)
