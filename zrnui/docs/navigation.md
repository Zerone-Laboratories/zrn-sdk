# Navigation: UserControl, Page, Frame, Pivot and Hub

This page covers the controls that make up the screens of an application and the
way the user moves between them: the Windows Phone navigation model (a Frame
that shows Pages and keeps a history), and the two multi-view containers of the
WP8.1 template, Pivot and Hub.

On this page:

- [UserControl](#usercontrol)
- [Page](#page)
- [Frame](#frame)
- [Page transitions and animation](#page-transitions-and-animation)
- [Pivot and PivotItem](#pivot-and-pivotitem)
- [Hub and HubSection](#hub-and-hubsection)

An AppBar or CommandBar is attached to a Page with `Zpage_set_top_app_bar` and
`Zpage_set_bottom_app_bar`; the bars themselves are described in
[app-bars-and-icons.md](app-bars-and-icons.md). To start an application with a
Frame see [getting-started.md](getting-started.md). Layout properties are in
[layout.md](layout.md); the ScrollViewer a hub uses is in [scrolling.md](scrolling.md).

## UserControl

**What it is.** A control with one child, its content (WP8.1 `UserControl`). It
is the base of Page.

**Create.**

```c
Zid Zuser_control_create(void);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zuser_control_set_content(uc, child)` | UserControl.Content: its one child. Replacing it destroys the old one. | empty |
| `Zuser_control_get_content(uc)` | The content, `Z_INVALID_ID` when empty. | |

Like every control it also has the properties of `Zcontrol_*` (foreground,
background, padding, font, tab stop; see `controls.h`).

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_view(void) {
    Zid view = Zuser_control_create();
    Zuser_control_set_content(view, Ztext_block_create("inside a user control"));
    return view;
}
```

## Page

**What it is.** A UserControl that a Frame shows (WP8.1 `Windows.UI.Xaml.Controls.Page`).

**Create.** A page is made inside a page factory (see [Frame](#frame)):

```c
Zid Zpage_create(void);
typedef Zid (*ZPageFactory)(Zid frame, void* parameter);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zpage_set_content(page, child)` / `Zpage_get_content(page)` | Page.Content: its one child, as UserControl. Replacing it destroys the old one. | |
| `Zpage_get_parameter(page)` | The parameter passed to `Zframe_navigate` (NavigationEventArgs.Parameter). NULL until the Frame has shown the page: read it in OnNavigatedTo, not in the factory (which gets it as an argument). | |
| `Zpage_get_frame(page)` | The Frame showing the page, `Z_INVALID_ID` when it is not shown. | |
| `Zpage_get_navigation_mode(page)` | `ZNavigationMode` of the navigation that brings the page or takes it away; valid inside its OnNavigatedTo and OnNavigatedFrom. | |
| `Zpage_set_navigation_cache_mode(page, mode)` / `Zpage_get_navigation_cache_mode(page)` | NavigationCacheMode. Set it before the frame leaves the page. | `Z_NAVIGATION_CACHE_DISABLED` |
| `Zpage_set_top_app_bar(page, bar)` / `Zpage_set_bottom_app_bar(page, bar)` | TopAppBar and BottomAppBar: an AppBar (a CommandBar), owned by the page; replacing it destroys the old one. `Z_INVALID_ID` takes it away. See [app-bars-and-icons.md](app-bars-and-icons.md). | none |
| `Zpage_get_top_app_bar(page)` / `Zpage_get_bottom_app_bar(page)` | Read the bars. | |

`ZNavigationMode`: `Z_NAVIGATION_NEW` (the frame navigated), `Z_NAVIGATION_BACK`,
`Z_NAVIGATION_FORWARD`, `Z_NAVIGATION_REFRESH`.

`ZNavigationCacheMode`: `Z_NAVIGATION_CACHE_DISABLED`,
`Z_NAVIGATION_CACHE_REQUIRED`, `Z_NAVIGATION_CACHE_ENABLED`.

**Events.**

| Function | Event | When |
| --- | --- | --- |
| `Zpage_on_navigated_to(page, callback, user_data)` | `Z_EVENT_NAVIGATED_TO` | OnNavigatedTo: the Frame has shown the page; read the parameter from it. |
| `Zpage_on_navigated_from(page, callback, user_data)` | `Z_EVENT_NAVIGATED_FROM` | OnNavigatedFrom: the Frame is about to replace the page; it is still alive in the callback. |

The callback type is `ZRoutedCallback`: `void (*)(Zid sender, void* user_data)`,
where `sender` is the page. Equivalent: `Zelement_on(page, Z_EVENT_NAVIGATED_TO, ...)`.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_shown(Zid page, void* user_data) {
    (void)user_data;
    const char* name = (const char*)Zpage_get_parameter(page);
    (void)name;
}

static Zid details_page(Zid frame, void* parameter) {
    (void)frame;
    (void)parameter;
    Zid page = Zpage_create();
    Zpage_set_content(page, Ztext_block_create("details"));
    Zpage_on_navigated_to(page, on_shown, NULL);
    return page;
}
```

**Notes.**

- The page starts below the safe area: 32 px at the top for the status bar of the
  shell (a constant for now, to come from the shell later). The Frame keeps this
  room itself, so a page needs no top margin of its own: a 32 px margin on the
  page content, as some samples set, doubles it.
- The page's content gets what a closed AppBar leaves; an open bar grows over the
  content.

## Frame

**What it is.** A ContentControl that navigates between pages and keeps the
history (WP8.1 `Frame`). In a Windows Phone app the Frame is the content of the
window.

C has no `typeof(MainPage)`, so a page is made by a **factory**: a function that
builds the page for a parameter. The Frame keeps the history as (factory,
parameter) and builds the page again on GoBack, unless the page is cached.

**Create.**

```c
Zid Zframe_create(void);
```

**Properties and functions.**

| Functions | Meaning |
| --- | --- |
| `bool Zframe_navigate(frame, ZPageFactory factory, void* parameter)` | Frame.Navigate: builds the page, shows it and puts it on the history; the pages ahead of the current one are dropped. Returns false, with nothing changed, when the factory is NULL, refuses (returns `Z_INVALID_ID`) or does not return a new Page, when there is no memory, or when it is called from a callback of a navigation (see the notes). |
| `Zframe_can_go_back(frame)` / `Zframe_can_go_forward(frame)` | Whether there is a page behind or ahead. |
| `bool Zframe_go_back(frame)` / `bool Zframe_go_forward(frame)` | Shows the previous (next) page: the cached one, else built again from its entry. Returns false when there is none, when the factory refuses, or when it is called from a callback of a navigation. |
| `Zframe_get_back_stack_depth(frame)` | Frame.BackStackDepth: pages behind the current one. |
| `Zframe_get_page(frame)` | The page shown, `Z_INVALID_ID` before the first Navigate. During a transition it is already the new page, though the old one is still on screen. |
| `Zframe_get_current_source_page_type(frame)` | CurrentSourcePageType: the factory of the page shown, NULL before the first Navigate. |
| `Zframe_set_transition(frame, ZPageTransition)` | See [Page transitions](#page-transitions-and-animation). |
| `Zframe_is_transitioning(frame)` | True while a transition runs. |
| `bool Zroot_back(root)` (`host.h`) | The Back button, in this order: closes the top popup, flyout or dialog that Back closes; else closes the open app bar that is not sticky; else, when the visual of the root is a Frame that can go back, goes back. False when none of those happened (the first page, or the visual is not a Frame); the caller then quits, as Back does on Windows Phone. `Zroot_inject_key` does the same for Escape when nothing handles the key; the window of zrnapp quits on it, only when the visual is a Frame. |

**Events.** `Zframe_on_navigated(frame, callback, user_data)`: Frame.Navigated
(`Z_EVENT_NAVIGATED`), raised when the page is shown, after its OnNavigatedTo.

**Order of a navigation.** NavigatedFrom of the old page, the old page destroyed,
NavigatedTo of the new one, then Navigated of the Frame. With a transition the
old page is destroyed later (see [Page transitions](#page-transitions-and-animation)).

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static Zid main_page(Zid frame, void* parameter);

static void open_details(Zid sender, void* user_data) {
    Zid frame = (Zid)(uintptr_t)user_data;
    (void)sender;
    Zframe_navigate(frame, main_page, NULL);
}

static Zid main_page(Zid frame, void* parameter) {
    (void)parameter;
    Zid page = Zpage_create();
    Zid button = Zbutton_create("go");
    Zbutton_on_click(button, open_details, (void*)(uintptr_t)frame);
    Zpage_set_content(page, button);
    return page;
}

void start(Zid root) {
    Zid frame = Zframe_create();
    Zroot_set_visual(root, frame);
    Zframe_navigate(frame, main_page, NULL);
}
```

(`Zbutton_create` and `Zbutton_on_click` are described in [buttons.md](buttons.md).
The example needs `#include <stdint.h>` for `uintptr_t`.)

**Notes.**

- A Click handler may call `Zframe_navigate`: the page, and the button that is
  running, are destroyed after the handler returns to the router, which checks
  for it.
- A page factory must return a new Page (`Zpage_create`) that has no parent, or
  `Z_INVALID_ID` to refuse. Any other result is reported
  (`Z_ERROR_WRONG_CLASS`), the element is destroyed if it has no parent, and the
  navigation fails.
- Navigation from inside a navigation is refused: `Zframe_navigate`,
  `Zframe_go_back` and `Zframe_go_forward` return false when called from a page
  factory or from a NavigatedFrom, NavigatedTo or Navigated callback. A
  navigation started while a *transition* runs is allowed and ends that
  transition at once.
- The `parameter` is a pointer the program owns; the Frame only keeps it, so it
  must outlive the history.
- Cache modes: a page with a NavigationCacheMode other than Disabled is not
  destroyed when the frame leaves it. It is kept for its entry of the history and
  comes back, as it was, on GoBack or GoForward (its factory does not run again).
  It is destroyed with the frame or when Navigate drops its entry. Required and
  Enabled do the same; there is no CacheSize, every cached page is kept.
- Not included: SuspensionManager, GetNavigationState, Navigating (it cannot be
  cancelled here).

## Page transitions and animation

```c
typedef enum ZPageTransition { Z_TRANSITION_NONE = 0, Z_TRANSITION_TURNSTILE } ZPageTransition;
```

With animations running (`Zroot_set_animations`, [getting-started.md](getting-started.md)),
the Frame shows a transition between two pages; with animations off the page is
replaced at once. `Z_TRANSITION_TURNSTILE` is the turnstile of Windows Phone: the
rows of the page that leaves turn away round the left edge one after another, a
moment of nothing, then the rows of the new page swing in; 706 ms in all. NavigatedFrom
comes when it starts, NavigatedTo and Navigated when the new page appears.
`Zframe_set_transition` selects it; it takes effect from the next navigation. The
default of a new Frame is `Z_TRANSITION_TURNSTILE`.
A Navigate or GoBack while a transition runs ends it at once.

The page that leaves is destroyed (unless it is cached) when the new one goes in:
inside `Zframe_navigate` or `Zframe_go_back` when no transition runs, otherwise from
`Zroot_tick`, once it has turned away (about 330 ms). During a transition
`Zframe_get_page` already returns the new page, though the old one is still on
screen.

Tilt and render transforms of single elements (`Zelement_set_tilt`,
`Zelement_animate`) are described in [layout.md](layout.md#render-transforms-animation-and-tilt).

## Pivot and PivotItem

**What it is.** Headers over pages that slide (WP8.1 `Pivot`). A title (18.14 px
SemiBold) sits above the strip of headers in a big light type (57 px SemiLight);
the chosen header is in the foreground and the others in PhoneControlDisabled;
the page of the chosen item is below, with the margin 19, 22.5, 19, 0.

A finger drags the pages and the strip together; it lets go on the next page when
it went a fifth of a page or flicked; a tap on a header goes to its page. With
animations the slide takes 300 ms, without them it is at once.

**Create.**

```c
Zid Zpivot_create(void);
Zid Zpivot_item_create(const char* header);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zpivot_set_title(pivot, title)` / `Zpivot_get_title(pivot)` | Title above the headers (the name of the application). "" or NULL takes it away. | none |
| `int Zpivot_add_item(pivot, item)` | Items.Append: the pivot takes the item. Returns its index, -1 when the item was not added (it has a parent already, a Zid is not a Pivot or a PivotItem, or no memory). | |
| `int Zpivot_insert_item(pivot, index, item)` | Items.Insert; an index out of range (also a negative one) appends. Returns as add. An item inserted before the chosen one keeps the selection on the item that was chosen. | |
| `Zpivot_remove_item_at(pivot, index)` | Items.RemoveAt: the item is destroyed; an index out of range does nothing. When the chosen item goes, the selection stays at its index (the next item, or the last), with no SelectionChanged. | |
| `Zpivot_clear_items(pivot)` | Removes all items. | |
| `Zpivot_get_item_count(pivot)` / `Zpivot_get_item(pivot, index)` | Read the items. | |
| `Zpivot_get_selected_item(pivot)` | SelectedItem, `Z_INVALID_ID` when there are no items. | |
| `Zpivot_set_selected_index(pivot, index)` | SelectedIndex: goes to an item, the shortest way round, sliding when animations run. | |
| `Zpivot_get_selected_index(pivot)` | The index of the chosen item, -1 when there are no items. | |
| `Zpivot_set_is_locked(pivot, is_locked)` / `Zpivot_get_is_locked(pivot)` | IsLocked: stops the swipes and the taps on the headers and hides the headers of the other items; the program can still set SelectedIndex. | false |
| `ZRect Zpivot_get_header_rect(pivot, index)` | Where the header of an item is now in the strip, in the coordinates of the pivot (an empty rectangle when out of view). | |
| `Zpivot_item_set_header(item, header)` / `Zpivot_item_get_header(item)` | Header of a PivotItem. | the string passed to create |
| `Zcontent_control_set_content(item, child)` / `Zcontent_control_set_text(item, text)` | The page of a PivotItem is its Content. | |

**Events.**

| Function | Meaning |
| --- | --- |
| `Zpivot_on_selection_changed(pivot, callback, user_data)` | SelectionChanged (`Z_EVENT_SELECTION_CHANGED`, `ZRoutedCallback`): the chosen item changed. |
| `Zpivot_on_pivot_item_loading(pivot, callback, user_data)` | PivotItemLoading: an item is chosen. |
| `Zpivot_on_pivot_item_loaded(pivot, callback, user_data)` | PivotItemLoaded: the page of the item has arrived (at once without animations). |

```c
typedef void (*ZPivotItemCallback)(Zid sender, Zid item, void* user_data);
```

`sender` is the pivot and `item` the PivotItem (PivotItemEventArgs.Item). The
first item is not announced as it is added.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void on_changed(Zid sender, void* user_data) {
    (void)user_data;
    int index = Zpivot_get_selected_index(sender);
    (void)index;
}

Zid make_settings_pivot(void) {
    Zid pivot = Zpivot_create();
    Zpivot_set_title(pivot, "SETTINGS");

    Zid first = Zpivot_item_create("system");
    Zcontent_control_set_text(first, "System settings");
    Zpivot_add_item(pivot, first);

    Zid second = Zpivot_item_create("theme");
    Zcontent_control_set_content(second, Ztext_block_create("Theme settings"));
    Zpivot_add_item(pivot, second);

    Zpivot_on_selection_changed(pivot, on_changed, NULL);
    return pivot;
}
```

**Notes.**

- The strip is a ring, as the PivotHeaderPanel makes it: with two items or more
  the headers go round (the one that leaves at the left comes back at the end),
  and a swipe past the last page goes to the first.
- A Pivot is not an ItemsControl here: its items are PivotItems, added with
  `Zpivot_add_item`. The pivot owns an item from then on.
- A tall page inside an item needs a ScrollViewer ([scrolling.md](scrolling.md)).

## Hub and HubSection

**What it is.** A big header (78 px Light) over sections that stand side by side
and scroll sideways with a finger, with inertia and no snap points and no scroll
bars (WP8.1 `Hub`). A HubSection is a header (19 px SemiBold) over its content,
with the margin 19, 0, 0, 0. Each section keeps room for the header of the hub
above its own, so that they begin together under it.

**Create.**

```c
Zid Zhub_create(void);
Zid Zhub_section_create(const char* header);
```

**Properties of the Hub.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zhub_set_header(hub, header)` / `Zhub_get_header(hub)` | Header: a string; "" or NULL takes it away. | none |
| `Zhub_set_is_looping(hub, is_looping)` / `Zhub_get_is_looping(hub)` | In a horizontal hub of two sections or more the sections go round: past the last comes the first and before the first the last. The default is the library's; the phone hub does not loop. | true |
| `Zhub_set_orientation(hub, orientation)` / `Zhub_get_orientation(hub)` | `Z_HORIZONTAL` or `Z_VERTICAL`. Setting it scrolls back to the start. A vertical hub stacks the sections in a vertical scroller and the header scrolls away with them. | `Z_HORIZONTAL` |
| `Zhub_get_scroll_viewer(hub)` | The ScrollViewer of the template: its offsets tell where the hub is. | |
| `int Zhub_add_section(hub, section)` | Sections.Append: the hub owns the section. Returns its index, -1 when the section was not added (an invalid handle, or it has a parent already). | |
| `int Zhub_insert_section(hub, index, section)` | Sections.Insert; an index out of range appends. Returns as add. | |
| `Zhub_remove_section_at(hub, index)` | The section is destroyed. | |
| `Zhub_clear_sections(hub)` | Removes all. | |
| `Zhub_get_section_count(hub)` / `Zhub_get_section(hub, index)` | Read the sections, in the order of the API (also in a looping hub, whose ring is turned on screen); `Z_INVALID_ID` when out of range. A section that is destroyed directly (`Zelement_destroy`) leaves the hub without harm. | |
| `Zhub_scroll_to_section(hub, index)` | Puts the section at the start of the view (at once, as far as the extent lets it go). | |
| `Zhub_get_sections_in_view_count(hub)` / `Zhub_get_section_in_view(hub, index)` | SectionsInView: the sections that show at least half a pixel, in order. | |

**Properties of a HubSection.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Zhub_section_set_header(section, header)` / `Zhub_section_get_header(section)` | Header. | the string passed to create |
| `Zcontent_control_set_content(section, child)` | The content of a section (where XAML has a template). | |
| `Zhub_section_set_is_header_interactive(section, is_interactive)` / `Zhub_section_get_is_header_interactive(section)` | The header can be tapped, for SectionHeaderClick. No arrow is drawn (the phone template draws none). | false |
| `Zhub_section_set_width_multiple(section, multiple)` / `Zhub_section_get_width_multiple(section)` | The width of the section is this times 90% of the width of the hub (2 is a wide one). 0 or less sizes it to its content, never wider than the screen less the margin. Only in a horizontal hub. | 1 |
| `Zelement_set_width(section, width)` | A width in pixels; wins over the multiple. | |

**Events.**

| Function | Meaning |
| --- | --- |
| `Zhub_on_sections_in_view_changed(hub, callback, user_data)` | SectionsInViewChanged (`Z_EVENT_SECTIONS_IN_VIEW_CHANGED`, `ZRoutedCallback`), raised from the scroll that makes other sections show (a finger, the inertia, ScrollToSection), from the next `Zroot_tick`, never inside the layout. |
| `Zhub_on_section_header_click(hub, callback, user_data)` | SectionHeaderClick: a tap on the header of a section with IsHeaderInteractive. One handler; NULL removes it. |

```c
typedef void (*ZHubSectionHeaderClickCallback)(Zid sender, Zid section, void* user_data);
```

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void header_clicked(Zid hub, Zid section, void* user_data) {
    (void)user_data;
    const char* title = Zhub_section_get_header(section);
    (void)hub;
    (void)title;
}

Zid make_hub(void) {
    Zid hub = Zhub_create();
    Zhub_set_header(hub, "my app");

    Zid news = Zhub_section_create("NEWS");
    Zhub_section_set_is_header_interactive(news, true);
    Zcontent_control_set_content(news, Ztext_block_create("headlines"));
    Zhub_add_section(hub, news);

    Zid wide = Zhub_section_create("WIDE");
    Zhub_section_set_width_multiple(wide, 2.0f);
    Zhub_add_section(hub, wide);

    Zhub_on_section_header_click(hub, header_clicked, NULL);
    return hub;
}
```

**Notes.**

- A section is 90% of the width of the hub (the margin is outside of it), as in
  the WP8.1 hub.
- The header of the hub goes at half the speed of the sections (parallax; this is
  the library's number).
- With IsLooping, the offsets of `Zhub_get_scroll_viewer` are those of a ring that
  is turned as it goes (not 0 at the first section). A ring too short to fill the
  screen with others shows a gap where a section would have to be twice.
- Not included: the parallax of the Background, SemanticZoomOwner, section
  header templates, DefaultSectionIndex.

Back to the [documentation index](index.md)
