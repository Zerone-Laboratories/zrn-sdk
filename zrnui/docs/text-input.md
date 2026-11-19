# Text input

This page covers the controls that take text from the keyboard: TextBox, PasswordBox and AutoSuggestBox. They look and behave like the Windows Phone 8.1 templates (a header over a 34 px box). To only display text, see [text.md](text.md).

Widgets on this page:

- [TextBox](#textbox)
- [PasswordBox](#passwordbox)
- [AutoSuggestBox](#autosuggestbox)

Shared `Control` properties (font, colors, padding) are in [buttons.md](buttons.md#control-and-contentcontrol).

## TextBox

**What it is.** The WP8.1 `TextBox`: an editable text with an optional header and placeholder. It has the margin 0,9.5 and MinWidth 51 of its style, and its box is 34 px high at least.

**Create.**

```c
ZRNUI_API Zid Ztext_box_create(const char* text);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Ztext_box_set_text(box, const char*)` / `Ztext_box_get_text` | Text; the caret goes to the end. The getter returns `""` for an empty box and the string stays valid until the next change of the text | the `text` of create |
| `Ztext_box_set_accepts_return(box, bool)` | AcceptsReturn: Enter makes a new line (multi-line box); without it a new line in typed or pasted text is dropped | false |
| `Ztext_box_set_text_wrapping(box, ZTextWrapping)` | `Z_WRAP` wraps the lines at the width of the box | `Z_NO_WRAP` |
| `Ztext_box_set_is_read_only(box, bool)` / `Ztext_box_get_is_read_only` | IsReadOnly: the user cannot edit the text (it can still be selected); `Ztext_box_set_text` still works | false |
| `Ztext_box_set_max_length(box, int)` / `Ztext_box_get_max_length` | MaxLength in characters (UTF-8 code points, not bytes); 0 or a negative value is no limit. It limits what the user types or pastes (the part that does not fit is cut off); the text already in the box, and `Ztext_box_set_text`, are not cut | 0 |
| `Ztext_box_select(box, int start, int length)` | Selection, in bytes of the UTF-8 text. `start` and `length` are clamped to the text and moved back to the start of a character when they fall inside one; the caret goes to the end of the selection | |
| `Ztext_box_select_all(box)` | Select everything | |
| `Ztext_box_get_selection_start`, `Ztext_box_get_selection_length` | Current selection, in bytes | 0, 0 |
| `Ztext_box_set_placeholder_text(box, const char*)` / `Ztext_box_get_placeholder_text` | Shown while the box is empty and has no focus; `""` or `NULL` removes it | none |
| `Ztext_box_set_header(box, const char*)` / `Ztext_box_get_header` | Small text above the box (16 px, PhoneMid); `""` or `NULL` removes it | none |

Focus: `Zelement_focus(box)` and `Zelement_has_focus(box)`.

**Events.**

| Event | Subscribe | Callback |
|---|---|---|
| TextChanged | `Zelement_on(box, Z_EVENT_TEXT_CHANGED, cb, data)` | `void (*ZRoutedCallback)(Zid sender, void* user_data)` |
| GotFocus, LostFocus | `Zelement_on(box, Z_EVENT_GOT_FOCUS / Z_EVENT_LOST_FOCUS, ...)` | `ZRoutedCallback` |

Read the new text with `Ztext_box_get_text(sender)`.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_text(Zid sender, void* user_data) {
    const char* text = Ztext_box_get_text(sender);
    (void)text;
}

Zid make_text_box(Zid stack) {
    Zid box = Ztext_box_create("");
    Ztext_box_set_header(box, "name");
    Ztext_box_set_placeholder_text(box, "your name");
    Ztext_box_set_max_length(box, 40);
    Zelement_on(box, Z_EVENT_TEXT_CHANGED, on_text, NULL);
    Zpanel_add(stack, box);
    return box;
}
```

**Notes.**

- TextChanged fires when the text changes, not when the program sets the same text.
- Selection positions are bytes of the UTF-8 text, MaxLength counts characters (code points).
- Handlers may destroy the control that raised them (also from TextChanged). The library checks the handle after the callback; do not use the sender afterwards.
- A press that starts on a TextBox is its own drag (selection); a surrounding ScrollViewer does not take the pointer away (see [scrolling.md](scrolling.md)).
- For a multi-line box combine `Ztext_box_set_accepts_return` with `Ztext_box_set_text_wrapping(box, Z_WRAP)`.

## PasswordBox

**What it is.** The WP8.1 `PasswordBox`: a TextBox that shows a bullet for each character.

**Create.**

```c
ZRNUI_API Zid Zpassword_box_create(void);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Zpassword_box_set_password(box, const char*)` / `Zpassword_box_get_password` | Password; the same text `Ztext_box_get_text` returns | empty |
| `Zpassword_box_set_header(box, const char*)` | Header above the box | none |
| `Zpassword_box_set_placeholder_text(box, const char*)` | Placeholder | none |
| `Zpassword_box_set_password_char(box, uint32_t codepoint)` | PasswordChar | the bullet |

The `Ztext_box_*` functions for header, placeholder and max length also work on a PasswordBox.

**Events.** TextChanged through `Zelement_on(box, Z_EVENT_TEXT_CHANGED, ...)`.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_password(Zid stack) {
    Zid box = Zpassword_box_create();
    Zpassword_box_set_header(box, "password");
    Zpassword_box_set_placeholder_text(box, "required");
    Zpanel_add(stack, box);
    return box;
}
```

**Notes.** Header and placeholder work as for TextBox.

## AutoSuggestBox

**What it is.** The WP8.1 `AutoSuggestBox`: a TextBox with PlaceholderText and Header and, under it, a white list with an accent border of suggestions while the list is open.

**Create.**

```c
ZRNUI_API Zid Zauto_suggest_box_create(void);
```

**Properties.**

| Function | Meaning | Default |
|---|---|---|
| `Zauto_suggest_box_set_items(box, const char* const* items, int count)` | The suggestions (ItemsSource as strings), copied; the list opens when there are any and the box has focus, an empty list closes it | none |
| `Zauto_suggest_box_set_text(box, const char*)` / `Zauto_suggest_box_get_text` | Text; setting it raises TextChanged with `Z_SUGGEST_PROGRAMMATIC_CHANGE` and does not open the list | empty |
| `Zauto_suggest_box_set_placeholder_text(box, const char*)` / `_get_placeholder_text` | Placeholder | none |
| `Zauto_suggest_box_set_header(box, const char*)` / `_get_header` | Header | none |
| `Zauto_suggest_box_set_is_suggestion_list_open(box, bool)` / `_get_is_suggestion_list_open` | IsSuggestionListOpen | false |
| `Zauto_suggest_box_set_max_suggestion_list_height(box, float)` | MaxSuggestionListHeight in pixels | 300 |
| `Zauto_suggest_box_set_update_text_on_select(box, bool)` | UpdateTextOnSelect: a chosen suggestion goes into the text | true |

The suggestions are items: `Zitems_control_get_count` and `Zitems_control_get_text` work on the box (see [items.md](items.md)).

**Events.**

```c
typedef void (*ZAutoSuggestTextChangedCallback)(Zid sender, ZAutoSuggestionBoxTextChangeReason reason, void* user_data);
typedef void (*ZAutoSuggestSuggestionChosenCallback)(Zid sender, int index, void* user_data);
typedef void (*ZAutoSuggestQuerySubmittedCallback)(Zid sender, const char* query_text, int chosen_index, void* user_data);

ZRNUI_API void Zauto_suggest_box_on_text_changed(Zid box, ZAutoSuggestTextChangedCallback callback, void* user_data);
ZRNUI_API void Zauto_suggest_box_on_suggestion_chosen(Zid box, ZAutoSuggestSuggestionChosenCallback callback, void* user_data);
ZRNUI_API void Zauto_suggest_box_on_query_submitted(Zid box, ZAutoSuggestQuerySubmittedCallback callback, void* user_data);
```

The reason is `Z_SUGGEST_USER_INPUT`, `Z_SUGGEST_PROGRAMMATIC_CHANGE` or `Z_SUGGEST_SUGGESTION_CHOSEN`. In QuerySubmitted `chosen_index` is -1 when no suggestion was chosen.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static const char* const cities[] = { "Berlin", "Bern", "Bergen", "Paris" };

static void on_text(Zid sender, ZAutoSuggestionBoxTextChangeReason reason, void* user_data) {
    if (reason != Z_SUGGEST_USER_INPUT) {
        return;
    }
    /* filter `cities` for Zauto_suggest_box_get_text(sender) yourself */
    Zauto_suggest_box_set_items(sender, cities, 4);
}

static void on_query(Zid sender, const char* query_text, int chosen_index, void* user_data) {
    /* search for query_text */
}

Zid make_suggest(Zid stack) {
    Zid box = Zauto_suggest_box_create();
    Zauto_suggest_box_set_header(box, "city");
    Zauto_suggest_box_set_placeholder_text(box, "type a city");
    Zauto_suggest_box_on_text_changed(box, on_text, NULL);
    Zauto_suggest_box_on_query_submitted(box, on_query, NULL);
    Zpanel_add(stack, box);
    return box;
}
```

**Notes.**

- Nothing is filtered by the control: the program listens to TextChanged and gives the suggestions for the text with `Zauto_suggest_box_set_items`.
- A tap on a suggestion (or Enter on the one the Up and Down keys are on) puts it in the text, then raises SuggestionChosen, then TextChanged with `Z_SUGGEST_SUGGESTION_CHOSEN`, then QuerySubmitted.
- Enter in the text raises QuerySubmitted with the text. QuerySubmitted and QueryIcon belong to Windows 10, not the 8.1 interface; QuerySubmitted exists here for the Enter key.
- `Zelement_focus` on the box hands the focus to its text part.

Back to the [documentation index](index.md).
