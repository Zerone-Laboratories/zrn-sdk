# Flyouts, dialogs and tooltips

Transient UI shown above the page: flyouts anchored to an element, menus, the modal ContentDialog, tooltips, and the Popup they are built on. Pickers are flyouts too, see [pickers.md](pickers.md); the buttons that attach flyouts are in [buttons.md](buttons.md).

Widgets on this page:

- [FlyoutBase (shared)](#flyoutbase-shared)
- [Flyout](#flyout)
- [MenuFlyout](#menuflyout)
- [Button.Flyout](#buttonflyout)
- [ContentDialog](#contentdialog)
- [ToolTip](#tooltip)
- [Popup and light dismiss](#popup-and-light-dismiss)

## FlyoutBase (shared)

**What it is.** `FlyoutBase` is not an element: it is the base of Flyout, MenuFlyout and the picker flyouts. A flyout shows above the page, in a Popup, next to the element it is shown at. A tap outside it, Back, a tap on an item of a menu, or Hide closes it. On the phone it is as wide as the screen.

**Create.** Use the concrete creators below.

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zflyout_base_set_placement(f, placement)` / `Zflyout_base_get_placement(f)` | `ZFlyoutPlacementMode`: `Z_FLYOUT_PLACEMENT_TOP` above the element, `Z_FLYOUT_PLACEMENT_BOTTOM` under it (each flips to the other side when it does not fit), `_LEFT` and `_RIGHT` act as Bottom, `Z_FLYOUT_PLACEMENT_FULL` fills the screen | Top |
| `Zflyout_base_show_at(f, target)` | `ShowAt`; `target` must be in a tree that belongs to a root (otherwise `Z_ERROR_INVALID_ARGUMENT` is reported and nothing shows). Showing it again at the same target does nothing; at another target it closes (Closed comes) and shows there | |
| `Zflyout_base_hide(f)` | Closes it | |
| `Zflyout_base_get_is_open(f)` | Whether it shows now | |

**Events**

`Zflyout_base_on_opening(f, cb, ud)` (before it shows), `Zflyout_base_on_opened` (after), `Zflyout_base_on_closed` (when it has gone). Callback type `ZRoutedCallback`: `void (*)(Zid sender, void* user_data)`. The same events exist as `Z_EVENT_OPENING`, `Z_EVENT_OPENED`, `Z_EVENT_CLOSED` for `Zelement_on`.

**Notes**

- A flyout is owned by the program (`Zelement_destroy`) until `Zbutton_set_flyout` gives it to a button.

## Flyout

**What it is.** The Windows Phone 8.1 `Flyout`: a FlyoutPresenter in the chrome color, at least 43 px tall, whose content sits 25.5 px under its top edge.

**Create**

```c
Zid Zflyout_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zflyout_set_content(f, content)` | `Flyout.Content`: any element, owned by the flyout | none |
| `Zflyout_get_content(f)` | | |

**Events.** FlyoutBase events (above).

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void show_hint(Zid anchor) {
	Zid body = Zcontent_control_create();
	Zcontent_control_set_text(body, "Saved.");
	Zid fly = Zflyout_create();
	Zflyout_set_content(fly, body);
	Zflyout_base_set_placement(fly, Z_FLYOUT_PLACEMENT_BOTTOM);
	Zflyout_base_show_at(fly, anchor);
}
```

**Notes**

- `FlyoutPresenterStyle` is not supported.

## MenuFlyout

**What it is.** The 8.1 `MenuFlyout`: the page background with a 1.5 px line above and below, items 25.5 px SemiLight with 8.5 px padding above and below, 19 px from the left.

**Create**

```c
Zid Zmenu_flyout_create(void);
Zid Zmenu_flyout_item_create(const char* text);
Zid Ztoggle_menu_flyout_item_create(const char* text, bool is_checked);
Zid Zmenu_flyout_separator_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zmenu_flyout_add_item(m, item)` | `Items.Append` of an item, a toggle item or a separator; the menu owns it; returns the index, -1 on failure (an invalid handle, another kind of element, or an item that has a parent: `Z_ERROR_HAS_PARENT`) | |
| `Zmenu_flyout_get_item_count(m)` / `Zmenu_flyout_get_item(m, i)` | | |
| `Zmenu_flyout_clear_items(m)` | | |
| `Zmenu_flyout_item_set_text(item, text)` / `Zmenu_flyout_item_get_text(item)` | Text of an item | |
| `Ztoggle_menu_flyout_item_set_is_checked(item, on)` / `Ztoggle_menu_flyout_item_get_is_checked(item)` | `IsChecked` | the value given at create |

**Events.** `Zmenu_flyout_item_on_click(item, ZRoutedCallback, user_data)`, callback `void (*)(Zid sender, void* user_data)`; plus the FlyoutBase events on the menu.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_rename(Zid sender, void* user_data) { (void)sender; (void)user_data; }

void show_menu(Zid anchor) {
	Zid menu = Zmenu_flyout_create();
	Zid rename = Zmenu_flyout_item_create("rename");
	Zmenu_flyout_item_on_click(rename, on_rename, NULL);
	Zmenu_flyout_add_item(menu, rename);
	Zmenu_flyout_add_item(menu, Zmenu_flyout_separator_create());
	Zmenu_flyout_add_item(menu, Ztoggle_menu_flyout_item_create("pinned", true));
	Zflyout_base_show_at(menu, anchor);
}
```

**Notes**

- A tap on an item runs Click and the menu closes.
- A ToggleMenuFlyoutItem flips IsChecked before Click runs; a check is drawn at the right edge when it is on (the template has none).

## Button.Flyout

**What it is.** `Button.Flyout`: a flyout attached to a button, shown when the button is tapped (after its Click). Works on any Button, including an [AppBarButton](app-bars-and-icons.md#appbarbutton).

**Functions**

```c
void Zbutton_set_flyout(Zid button, Zid flyout);
Zid  Zbutton_get_flyout(Zid button);
```

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_more_button(Zid menu) {   // menu: a MenuFlyout, made as in the example above
	Zid btn = Zbutton_create("more");
	Zbutton_set_flyout(btn, menu);   // the button now owns it
	return btn;
}
```

**Notes**

- The button takes ownership. A flyout that is attached to a button already is refused (`Z_ERROR_HAS_PARENT` is reported and nothing changes). The flyout the button had is destroyed when it is replaced; `Z_INVALID_ID` takes the flyout away that way, so `Zbutton_get_flyout` returns `Z_INVALID_ID` after it.
- A flyout shown at a button by hand with `Zflyout_base_show_at` stays yours.

## ContentDialog

**What it is.** The 8.1 `ContentDialog`: a dialog at the top of the screen, as wide as it, in the chrome color: title (25.5 px SemiBold), content (18.14 px), and the buttons under them in two halves (primary at the left; a button without text is not there).

**Create**

```c
Zid Zcontent_dialog_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zcontent_dialog_set_title(d, text)` / `Zcontent_dialog_get_title(d)` | Title as a string; `""`/`NULL` removes it | none |
| `Zcontent_control_set_content(d, element)` / `Zcontent_control_set_text(d, text)` | The Content ([ContentControl](buttons.md)) | |
| `Zcontent_dialog_set_primary_button_text(d, text)` / `_get_primary_button_text` | | none |
| `Zcontent_dialog_set_secondary_button_text(d, text)` / `_get_secondary_button_text` | | none |
| `Zcontent_dialog_set_is_primary_button_enabled(d, on)` / `_get_is_primary_button_enabled` | | |
| `Zcontent_dialog_set_is_secondary_button_enabled(d, on)` / `_get_is_secondary_button_enabled` | | |
| `Zcontent_dialog_show(d, root, callback, user_data)` | `ShowAsync`: shows it over `root`. `callback` may be NULL. Returns false when it already shows, when the dialog is in a tree (`Z_ERROR_HAS_PARENT`), when `root` is not a root (`Z_ERROR_INVALID_ID`) or on failure such as no memory; the dialog is then not shown and `callback` is not kept | |
| `Zcontent_dialog_hide(d)` | Closes with the result None | |

**Events**

- `Zcontent_dialog_on_primary_button_click`, `Zcontent_dialog_on_secondary_button_click`, `Zcontent_dialog_on_opened`, `Zcontent_dialog_on_closed`: `(Zid dialog, ZRoutedCallback, void* user_data)`, callback `void (*)(Zid sender, void* user_data)`. Also `Z_EVENT_PRIMARY_BUTTON_CLICK`, `Z_EVENT_SECONDARY_BUTTON_CLICK`, `Z_EVENT_OPENED`, `Z_EVENT_CLOSED` for `Zelement_on`.
- Result callback `ZContentDialogResultCallback`: `void (*)(Zid dialog, ZContentDialogResult result, void* user_data)`; result is `Z_CONTENT_DIALOG_RESULT_NONE`, `_PRIMARY` or `_SECONDARY`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void done(Zid dialog, ZContentDialogResult result, void* user_data) {
	(void)dialog; (void)user_data;
	if (result == Z_CONTENT_DIALOG_RESULT_PRIMARY) { /* delete it */ }
}

void ask(Zid root) {
	Zid dialog = Zcontent_dialog_create();
	Zcontent_dialog_set_title(dialog, "Delete?");
	Zcontent_control_set_text(dialog, "The file will be gone.");
	Zcontent_dialog_set_primary_button_text(dialog, "delete");
	Zcontent_dialog_set_secondary_button_text(dialog, "cancel");
	Zcontent_dialog_show(dialog, root, done, NULL);
}
```

**Notes**

- It shows in a modal Popup: what is under it is dimmed (black 60% in Dark, white 60% in Light) and gets no input; Tab stays in the dialog; Back or `Zcontent_dialog_hide` closes it with None.
- The program owns the dialog (it is not put in a tree). There is no await in C: the callback gets the result, and Closed comes before it.
- Keyboard focus goes to the primary button and comes back when the dialog closes.
- Not included: Closing (it cannot be cancelled), FullSizeDesired, the buttons' Command, TitleTemplate.

## ToolTip

**What it is.** `ToolTip`, a ContentControl shown when the pointer rests on an element. The simple way is plain text through `ToolTipService.ToolTip`.

**Create**

```c
Zid Ztool_tip_create(const char* text);
void Zelement_set_tooltip(Zid element, const char* text);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zelement_set_tooltip(element, text)` | Plain-text tooltip, shown when the pointer rests on the element; `NULL` removes it | none |
| `Ztool_tip_create(text)` | A ToolTip control with a text content | |

**Events.** None in the headers.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_save_button(void) {
	Zid b = Zbutton_create("save");
	Zelement_set_tooltip(b, "Save the file");
	return b;
}
```

**Notes**

- The delay is driven by `Zroot_tick` (host.h).
- No further ToolTip-specific functions (placement, open state) exist in the headers; use `Zelement_set_tooltip`.

## Popup and light dismiss

A Popup shows its child above the rest of the page, at an offset from the top-left corner of its root, or next to a placement target. It takes no room in the layout. A ComboBox drops down in one, and the flyouts and the ContentDialog are shown in one.

| Function | Meaning |
|---|---|
| `Zpopup_create()` | New Popup |
| `Zpopup_set_child(p, child)` | Child element |
| `Zpopup_set_offset(p, x, y)` | Offset from the root's top-left corner |
| `Zpopup_set_placement_target(p, target, mode)` | Opens next to `target`; `mode` is a `ZPlacementMode` (`Z_PLACE_BOTTOM`, `Z_PLACE_TOP`, `Z_PLACE_LEFT`, `Z_PLACE_RIGHT`, `Z_PLACE_MOUSE`) |
| `Zpopup_set_open(p, on)` / `Zpopup_is_open(p)` | `IsOpen`; opens only once the popup is in a tree that belongs to a root |
| `Zpopup_set_stays_open(p, on)` | `StaysOpen`: false (the default of ComboBox) closes the popup on a click outside (light dismiss) |

Events: `Z_EVENT_POPUP_OPENED` and `Z_EVENT_POPUP_CLOSED` via `Zelement_on`.

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void show_popup(Zid anchor) {   // anchor must be in a tree that belongs to a root
	Zid popup = Zpopup_create();
	Zpopup_set_child(popup, Zbutton_create("hello"));
	Zpopup_set_placement_target(popup, anchor, Z_PLACE_BOTTOM);
	Zpopup_set_stays_open(popup, false);
	Zpopup_set_open(popup, true);
}
```

Back to the [documentation index](index.md)
