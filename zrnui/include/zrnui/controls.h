/**
 * zrnui
 * controls.h : Control and everything that derives from it.
 *
 *   Control                              Foreground, Background, BorderBrush,
 *                                        BorderThickness, Padding, Font*
 *     UserControl
 *     ContentControl                     Content: a string or an element
 *       ToolTip
 *       ButtonBase                       ClickMode, IsPressed, Click
 *         Button                         Flyout
 *           AppBarButton                 Label, Icon, IsCompact
 *         HyperlinkButton, RepeatButton
 *         ToggleButton                   IsChecked, IsThreeState
 *           CheckBox, RadioButton        GroupName
 *           AppBarToggleButton           Label, Icon, IsCompact
 *       ContentDialog                    Title, PrimaryButtonText, SecondaryButtonText, Show, Hide
 *       PivotItem                        Header: the page of a Pivot
 *       HubSection                       Header, IsHeaderInteractive: a section of a Hub
 *       SelectorItem                     IsSelected
 *         ListBoxItem, ComboBoxItem, ListViewItem, GridViewItem, FlipViewItem
 *       ListViewBaseHeaderItem           the header of a group
 *         ListViewHeaderItem, GridViewHeaderItem
 *     AppBar                             IsOpen, IsSticky, ClosedDisplayMode, Opened, Closed
 *       CommandBar                       PrimaryCommands, SecondaryCommands
 *     AppBarSeparator
 *     MenuFlyoutItemBase                 (an item of a MenuFlyout)
 *       MenuFlyoutItem, MenuFlyoutSeparator
 *         ToggleMenuFlyoutItem
 *     ToggleSwitch                       IsOn, Header, OnContent, OffContent, Toggled
 *     ProgressRing                       IsActive
 *     RangeBase                          Minimum, Maximum, Value
 *       Slider, ProgressBar, ScrollBar
 *     Thumb                              dragging
 *     Separator
 *     ScrollViewer
 *     TextBox, PasswordBox
 *     ItemsControl                       Items, ItemsPanel
 *       Selector                         SelectedIndex, SelectionChanged
 *         ListBox                        SelectionMode
 *         ComboBox                       Header, PlaceholderText, IsDropDownOpen
 *         FlipView                       a page at a time
 *         ListViewBase                   SelectionMode, IsItemClickEnabled, Header, Footer
 *           ListView, GridView
 *       AutoSuggestBox                   Text, PlaceholderText, the suggestions
 *     SemanticZoom                       two views of the same data
 *     Pivot                              Title, SelectedIndex, IsLocked: PivotItems under a strip of headers
 *     Hub                                Header, Orientation, ScrollToSection: HubSections side by side
 *     LoopingSelector                    Items, SelectedIndex, ShouldLoop
 *     DatePicker, TimePicker             Header, Date / Time: a button that shows the flyout below
 *   IconElement (not a control)          SymbolIcon, FontIcon, PathIcon, BitmapIcon
 *   FlyoutBase (not an element)          Placement, ShowAt, Hide, Opening, Opened, Closed
 *     Flyout                             Content
 *     MenuFlyout                         Items
 *     PickerFlyoutBase                   Title, with the selectors and Done / Cancel
 *       DatePickerFlyout, TimePickerFlyout
 *
 * Button, HyperlinkButton, RepeatButton, ToggleButton, CheckBox, RadioButton,
 * Slider, ProgressBar, ProgressRing, ToggleSwitch, TextBox, PasswordBox, ListBox,
 * ComboBox, ListView, GridView, FlipView, AutoSuggestBox, Pivot, Hub, LoopingSelector, DatePicker and TimePicker look and behave like the controls of the Windows Phone 8.1 templates (generic.xaml
 * of the phone SDK), with the numbers of the 8.1 XAML as they are: a Button is a
 * 2.5 px outline, at least 109 x 57.5, that fills with the accent while pressed;
 * a CheckBox is a 25.5 px square with a check path; a TextBox a header over a
 * 34 px box; a ListView item is a row with no padding of 12 px text. The other
 * controls here are the WP8 ones still. The templates are
 * not exposed: a control draws itself, and the properties below are the ones
 * the templates bind to.
 *
 * Colors that were not set follow the theme (host.h): changing the
 * theme or the accent of the root restyles every control that has none.
 */

#pragma once

#include "element.h"


/** ValueChanged of a RangeBase: runs once the value has changed (never for the same value), also when the program set it. */
typedef void (*ZValueChangedCallback)(Zid sender, double old_value, double new_value, void* user_data);
/**
 * Scroll of a ScrollBar or a ScrollViewer. A ScrollBar reports what the user did (`type`) and its value; a ScrollViewer
 * always reports Z_SCROLL_THUMB_POSITION and its offset along the axis that scrolls: the horizontal one when only the
 * horizontal axis can scroll (a Hub), otherwise the vertical one.
 */
typedef void (*ZScrollCallback)(Zid sender, ZScrollEventType type, double new_value, void* user_data);
/**
 * DragStarted / DragDelta / DragCompleted of a Thumb. Started: the position of the press inside the thumb. Delta: how far
 * the pointer moved since the last call. Completed: the total distance of the drag, also when the capture was lost.
 */
typedef void (*ZDragCallback)(Zid sender, float horizontal_change, float vertical_change, void* user_data);
/** ItemClick of a ListView or a GridView: `index` is the index of the item that was tapped (ItemClickEventArgs.ClickedItem). */
typedef void (*ZItemClickCallback)(Zid sender, int index, void* user_data);
/** TextChanged of an AutoSuggestBox: why the text changed. A suggestion that was chosen comes as Z_SUGGEST_SUGGESTION_CHOSEN. */
typedef void (*ZAutoSuggestTextChangedCallback)(Zid sender, ZAutoSuggestionBoxTextChangeReason reason, void* user_data);
/** SuggestionChosen of an AutoSuggestBox: `index` of the suggestion in the list (SelectedItem). */
typedef void (*ZAutoSuggestSuggestionChosenCallback)(Zid sender, int index, void* user_data);
/** QuerySubmitted of an AutoSuggestBox (Enter, or a tap on a suggestion): the text and the suggestion chosen, -1 when none. */
typedef void (*ZAutoSuggestQuerySubmittedCallback)(Zid sender, const char* query_text, int chosen_index, void* user_data);
/** ContentDialog.ShowAsync has finished: what closed the dialog. */
typedef void (*ZContentDialogResultCallback)(Zid dialog, ZContentDialogResult result, void* user_data);
/** ViewChangeStarted / ViewChangeCompleted of a SemanticZoom: whether the view that is being left (was left) is the zoomed in one. */
typedef void (*ZSemanticZoomCallback)(Zid sender, bool is_source_zoomed_in_view, void* user_data);


/** A calendar date, Gregorian: `month` 1 to 12, `day` 1 to the days of the month. */
typedef struct ZDate {
	int year, month, day;
} ZDate;

/** A time of day, 24 hour clock: `hour` 0 to 23, `minute` 0 to 59. */
typedef struct ZTimeOfDay {
	int hour, minute;
} ZTimeOfDay;

/** ClockIdentifier of a TimePicker. */
typedef enum ZClockIdentifier {
	Z_CLOCK_12_HOUR = 0x0700,
	Z_CLOCK_24_HOUR
} ZClockIdentifier;

/** DateChanged of a DatePicker (DatePickerValueChangedEventArgs): the old and the new date. */
typedef void (*ZDateChangedCallback)(Zid sender, ZDate old_date, ZDate new_date, void* user_data);
/** DatePicked of a DatePickerFlyout (DatePickedEventArgs). */
typedef void (*ZDatePickedCallback)(Zid sender, ZDate old_date, ZDate new_date, void* user_data);
/** TimeChanged of a TimePicker. */
typedef void (*ZTimeChangedCallback)(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data);
/** TimePicked of a TimePickerFlyout. */
typedef void (*ZTimePickedCallback)(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data);


#ifdef __cplusplus
extern "C" {
#endif

	/* ============================================================================
	 *  CONTROL
	 * ============================================================================ */

	/**
	 * Visual properties of every control. A color or a size that was never
	 * set comes from the template: Foreground white in Dark, the accent for
	 * a ProgressBar... Zcontrol_reset_* is not needed: set a color to take
	 * it, create the control again to drop it.
	 */
	ZRNUI_API void Zcontrol_set_foreground(Zid control, ZColor color);
	ZRNUI_API void Zcontrol_set_background(Zid control, ZColor color);
	ZRNUI_API void Zcontrol_set_border_brush(Zid control, ZColor color);
	ZRNUI_API void Zcontrol_set_border_thickness(Zid control, ZThickness thickness);
	ZRNUI_API void Zcontrol_set_padding(Zid control, ZThickness padding);
	/**
	 * @brief {ThemeResource key} for a property of a control: Foreground, Background and BorderBrush take a color key,
	 * Padding and BorderThickness a Thickness key, FontSize a Double key. Follows the theme, the accent and the overrides
	 * of the root; the direct setter removes the binding. @return false for a Zid that is not a control or a key of another type.
	 */
	ZRNUI_API bool Zcontrol_bind_foreground(Zid control, ZThemeResource key);
	ZRNUI_API bool Zcontrol_bind_background(Zid control, ZThemeResource key);
	ZRNUI_API bool Zcontrol_bind_border_brush(Zid control, ZThemeResource key);
	ZRNUI_API bool Zcontrol_bind_border_thickness(Zid control, ZThemeResource key);
	ZRNUI_API bool Zcontrol_bind_padding(Zid control, ZThemeResource key);
	ZRNUI_API bool Zcontrol_bind_font_size(Zid control, ZThemeResource key);
	ZRNUI_API ZThickness Zcontrol_get_padding(Zid control);
	/** @brief FontSize in px; also the size of the text the control makes for a string content. The getter returns 0 for a Zid that is not a control. */
	ZRNUI_API void Zcontrol_set_font_size(Zid control, float size);
	ZRNUI_API float Zcontrol_get_font_size(Zid control);
	/** @brief FontFamily of the text of the control and of the content it makes from a string. */
	ZRNUI_API void Zcontrol_set_font_family(Zid control, ZFontFamily family);
	/** @brief Where the content sits inside the room of the control (HorizontalContentAlignment, VerticalContentAlignment). */
	ZRNUI_API void Zcontrol_set_horizontal_content_alignment(Zid control, ZHorizontalAlignment alignment);
	ZRNUI_API void Zcontrol_set_vertical_content_alignment(Zid control, ZVerticalAlignment alignment);
	/** @brief IsTabStop: false keeps the control out of the Tab order. */
	ZRNUI_API void Zcontrol_set_tab_stop(Zid control, bool is_tab_stop);
	/** @brief TabIndex: the order of the Tab key among the controls that are tab stops. */
	ZRNUI_API void Zcontrol_set_tab_index(Zid control, int tab_index);

	/** @brief A UserControl with no content, stretched, that takes no focus. */
	ZRNUI_API Zid Zuser_control_create(void);
	/**
	 * @brief UserControl.Content: its one child, which the control owns. Replacing it, or Z_INVALID_ID, destroys the old one.
	 * A child that has a parent is moved here; one that would become its own ancestor is refused (Z_ERROR_CYCLE) and the old child stays.
	 */
	ZRNUI_API void Zuser_control_set_content(Zid user_control, Zid child);
	/** @return The content, Z_INVALID_ID when empty. */
	ZRNUI_API Zid Zuser_control_get_content(Zid user_control);


	/* ============================================================================
	 *  FRAME and PAGE
	 *
	 *  The navigation of a Windows Phone app: Window.Content is a Frame, and
	 *  Frame.Navigate(typeof(MainPage), parameter) shows a Page. C has no
	 *  typeof, so a page is made by a factory: a function that builds the
	 *  page for a parameter. The Frame keeps the history as (factory,
	 *  parameter) and builds the page again on GoBack (CacheSize 0).
	 *
	 *    static Zid main_page(Zid frame, void* parameter) {
	 *        Zid page = Zpage_create();
	 *        Zpage_set_content(page, ...);
	 *        Zpage_on_navigated_to(page, loaded, NULL);   // LoadState
	 *        return page;
	 *    }
	 *    Zid frame = Zframe_create();
	 *    Zroot_set_visual(root, frame);
	 *    Zframe_navigate(frame, main_page, NULL);
	 *
	 *  A Click handler may call Zframe_navigate: the page, and the button
	 *  that is running, are destroyed after the handler returns to the
	 *  router, which checks for it. The `parameter` is a pointer the program
	 *  owns; the Frame only keeps it, so it must outlive the history.
	 *  The page starts below the safe area, 32 px at the top for the status bar of the shell
	 *  (a constant for now, to come from the shell later).
	 *
	 *  Windows.UI.Xaml.Navigation.NavigationMode comes with the callbacks: Zpage_get_navigation_mode
	 *  tells, inside OnNavigatedTo and OnNavigatedFrom, whether the frame navigated (Z_NAVIGATION_NEW),
	 *  went back or went forward; the page that leaves is told the mode of the navigation that leaves it.
	 *  A page with a NavigationCacheMode other than Disabled is not destroyed when the frame leaves it:
	 *  it is kept for its entry of the history and comes back, as it was, on GoBack or GoForward (its
	 *  factory does not run again); it is destroyed with the frame or when Navigate drops its entry.
	 *  Required and Enabled do the same (there is no CacheSize: every cached page is kept).
	 *  The page that leaves is destroyed (unless it is cached) when the new one goes in: inside Zframe_navigate
	 *  or Zframe_go_back when no transition runs, otherwise from Zroot_tick, once it has turned away (about 330 ms).
	 *  A navigation started while another transition runs ends that one at once; one started from a callback
	 *  of a navigation (NavigatedFrom, NavigatedTo, Navigated, a page factory) is refused: the call returns false.
	 *  A page may show an AppBar over its top or its bottom (Zpage_set_top_app_bar); the content
	 *  gets what the closed bar leaves, and an open bar grows over the content.
	 *  Not included: SuspensionManager, GetNavigationState, Navigating (it cannot be cancelled here).
	 * ============================================================================ */

	/**
	 * @brief Builds the page of a navigation (Zframe_navigate, and again on a GoBack or GoForward to a page that is not cached).
	 * Returns a new Page (Zpage_create) with no parent, or Z_INVALID_ID to refuse the navigation. Any other result is
	 * reported (Z_ERROR_WRONG_CLASS), the element is destroyed if it has no parent, and the navigation fails.
	 * It cannot start another navigation.
	 */
	typedef Zid (*ZPageFactory)(Zid frame, void* parameter);

	/** @brief A Page with no content: stretched, takes no focus. Normally returned by a ZPageFactory. */
	ZRNUI_API Zid Zpage_create(void);
	/** @brief Page.Content: its one child, as UserControl. Replacing it destroys the old one. */
	ZRNUI_API void Zpage_set_content(Zid page, Zid child);
	/** @return The content, Z_INVALID_ID when empty. */
	ZRNUI_API Zid Zpage_get_content(Zid page);
	/** @brief The parameter passed to Zframe_navigate for this page (NavigationEventArgs.Parameter). NULL until the frame has shown the page, so not inside the factory (it gets the parameter as an argument). */
	ZRNUI_API void* Zpage_get_parameter(Zid page);
	/** @brief The Frame the page is in, Z_INVALID_ID when it is not in one (still being built, cached, or waiting for a transition). */
	ZRNUI_API Zid Zpage_get_frame(Zid page);
	/** @brief The NavigationMode of the navigation that brings the page, or takes it away, valid in its OnNavigatedTo and OnNavigatedFrom. */
	ZRNUI_API ZNavigationMode Zpage_get_navigation_mode(Zid page);
	/** @brief NavigationCacheMode, Z_NAVIGATION_CACHE_DISABLED by default. Set it before the frame leaves the page. */
	ZRNUI_API void Zpage_set_navigation_cache_mode(Zid page, ZNavigationCacheMode mode);
	ZRNUI_API ZNavigationCacheMode Zpage_get_navigation_cache_mode(Zid page);
	/**
	 * @brief TopAppBar and BottomAppBar: an AppBar (a CommandBar), owned by the page; replacing it destroys the old one.
	 * Z_INVALID_ID takes it away; a Zid that is not an AppBar is reported and changes nothing.
	 */
	ZRNUI_API void Zpage_set_top_app_bar(Zid page, Zid app_bar);
	ZRNUI_API void Zpage_set_bottom_app_bar(Zid page, Zid app_bar);
	/** @return The bar, Z_INVALID_ID when the page has none. */
	ZRNUI_API Zid Zpage_get_top_app_bar(Zid page);
	ZRNUI_API Zid Zpage_get_bottom_app_bar(Zid page);
	/** @brief OnNavigatedTo: the page is in the Frame (Zframe_navigate has put it there); read the parameter from it. One handler, a new one replaces it. */
	ZRNUI_API void Zpage_on_navigated_to(Zid page, ZRoutedCallback callback, void* user_data);
	/** @brief OnNavigatedFrom: the Frame is about to replace the page; it is still alive in the callback. One handler. */
	ZRNUI_API void Zpage_on_navigated_from(Zid page, ZRoutedCallback callback, void* user_data);

	/**
	 * What shows between two pages when animations run (Zroot_set_animations; with them off the page is
	 * replaced at once). The turnstile of Windows Phone: the rows of the page that leaves turn away round
	 * the left edge one after another, a moment of nothing, then the rows of the new page swing in;
	 * 706 ms. NavigatedFrom comes when it starts, NavigatedTo and Navigated when the new page appears.
	 */
	typedef enum ZPageTransition {
		Z_TRANSITION_NONE = 0,
		Z_TRANSITION_TURNSTILE
	} ZPageTransition;

	/** @brief A Frame with no page and no history; it shows the first page after Zframe_navigate. The transition is Z_TRANSITION_TURNSTILE at first. */
	ZRNUI_API Zid Zframe_create(void);


	/* ============================================================================
	 *  PIVOT, PIVOTITEM
	 *
	 *  The Pivot of the Windows Phone 8.1 template: a title (18.14 px SemiBold at 19,38), under it the
	 *  strip of the headers in a big light type (57 px SemiLight) with the chosen one in the foreground and
	 *  the others in PhoneControlDisabled, and the page of the chosen item below, with the margin 19,22.5,19,0.
	 *  A finger drags the pages and the strip together; it lets go on the next page when it went a fifth of
	 *  a page or flicked; a tap on a header goes to its page. With animations the slide takes 300 ms, without
	 *  them it is at once. SelectionChanged (Z_EVENT_SELECTION_CHANGED, Zpivot_on_selection_changed) comes when
	 *  the chosen item changes, PivotItemLoading when an item is chosen and PivotItemLoaded when its page has
	 *  arrived (at once without animations). The first item is not announced as it is added.
	 *
	 *    Zid pivot = Zpivot_create();
	 *    Zpivot_set_title(pivot, "SETTINGS");
	 *    Zid item = Zpivot_item_create("system");
	 *    Zcontent_control_set_content(item, system_list);
	 *    Zpivot_add_item(pivot, item);
	 *
	 *  The strip is a ring, as the PivotHeaderPanel makes it: with two items or more the headers go round (the
	 *  one that leaves at the left comes back at the end), and a swipe past the last page goes to the first. The
	 *  template does not say it, the native panel does. IsLocked stops the swipes and the taps on the headers and
	 *  hides the headers of the other items; the program can still set SelectedIndex. A Pivot is not an ItemsControl
	 *  here: its items are PivotItems, added with Zpivot_add_item.
	 * ============================================================================ */

	/** PivotItemLoading and PivotItemLoaded: the pivot and the PivotItem (PivotItemEventArgs.Item). */
	typedef void (*ZPivotItemCallback)(Zid sender, Zid item, void* user_data);

	/** @brief A Pivot with no items and no title. */
	ZRNUI_API Zid Zpivot_create(void);
	/** @brief Title above the headers (the name of the application); "" or NULL takes it away. */
	ZRNUI_API void Zpivot_set_title(Zid pivot, const char* title);
	/** @return The title, "" when there is none. */
	ZRNUI_API const char* Zpivot_get_title(Zid pivot);
	/**
	 * @brief A PivotItem with its Header. Its Content is the page: Zcontent_control_set_content or set_text.
	 * The item is given to a pivot with Zpivot_add_item, which owns it from then on.
	 */
	ZRNUI_API Zid Zpivot_item_create(const char* header);
	/** @brief The header of the strip; NULL is "". The getter returns "" for a Zid that is not a PivotItem. */
	ZRNUI_API void Zpivot_item_set_header(Zid pivot_item, const char* header);
	ZRNUI_API const char* Zpivot_item_get_header(Zid pivot_item);
	/**
	 * @brief Items.Append: the pivot takes the item and owns it. @return its index, -1 when the item was not added: it is
	 * in a tree already (Z_ERROR_HAS_PARENT), a Zid is not a Pivot or a PivotItem, or no memory.
	 */
	ZRNUI_API int Zpivot_add_item(Zid pivot, Zid pivot_item);
	/**
	 * @brief Items.Insert; an index out of range (also a negative one) appends. Returns and fails as Zpivot_add_item.
	 * An item inserted before the chosen one keeps the selection on the item that was chosen.
	 */
	ZRNUI_API int Zpivot_insert_item(Zid pivot, int index, Zid pivot_item);
	/**
	 * @brief Items.RemoveAt: the item is destroyed; an index out of range does nothing. When the chosen item goes the
	 * selection stays at its index (the next item, or the last), with no SelectionChanged.
	 */
	ZRNUI_API void Zpivot_remove_item_at(Zid pivot, int index);
	/** @brief Destroys every item. */
	ZRNUI_API void Zpivot_clear_items(Zid pivot);
	ZRNUI_API int Zpivot_get_item_count(Zid pivot);
	/** @return The PivotItem at an index, Z_INVALID_ID when out of range. */
	ZRNUI_API Zid Zpivot_get_item(Zid pivot, int index);
	/** @brief SelectedItem, Z_INVALID_ID when there are no items. */
	ZRNUI_API Zid Zpivot_get_selected_item(Zid pivot);
	/**
	 * @brief SelectedIndex: goes to an item, the shortest way round, sliding when animations run. An index out of range
	 * is clamped to the first or the last item; with no items nothing happens. Loading, SelectionChanged and Loaded
	 * run only when the item changes.
	 */
	ZRNUI_API void Zpivot_set_selected_index(Zid pivot, int index);
	/** @return The index of the chosen item, -1 when there are no items. */
	ZRNUI_API int Zpivot_get_selected_index(Zid pivot);
	/** @brief IsLocked (default false). */
	ZRNUI_API void Zpivot_set_is_locked(Zid pivot, bool is_locked);
	ZRNUI_API bool Zpivot_get_is_locked(Zid pivot);
	/** @brief Where the header of an item is now in the strip, in the coordinates of the pivot (an empty rectangle when it is out of view). */
	ZRNUI_API ZRect Zpivot_get_header_rect(Zid pivot, int index);
	/** @brief SelectionChanged: the chosen item changed, by the user or by the program (not for the first item added, nor when the chosen item was removed). */
	ZRNUI_API void Zpivot_on_selection_changed(Zid pivot, ZRoutedCallback callback, void* user_data);
	/**
	 * @brief PivotItemLoading: an item has been chosen, before SelectionChanged. The handler may add or remove items;
	 * the selection is then the one the pivot settled on. One handler per pivot, NULL removes it.
	 */
	ZRNUI_API void Zpivot_on_pivot_item_loading(Zid pivot, ZPivotItemCallback callback, void* user_data);
	/** @brief PivotItemLoaded: the chosen item has arrived (after the slide, at once without animations). One handler per pivot, NULL removes it. */
	ZRNUI_API void Zpivot_on_pivot_item_loaded(Zid pivot, ZPivotItemCallback callback, void* user_data);

	/** @brief The transition between pages; it runs only while animations are on (Zroot_set_animations). Takes effect from the next navigation. */
	ZRNUI_API void Zframe_set_transition(Zid frame, ZPageTransition transition);
	/** @brief true while a transition runs. A Navigate or GoBack in that time ends it at once. */
	ZRNUI_API bool Zframe_is_transitioning(Zid frame);
	/**
	 * @brief Frame.Navigate: builds the page with `factory`, shows it and puts it on the history; the entries ahead of the current one are dropped.
	 * Order: NavigatedFrom of the old page, the old page destroyed (after the transition, when one runs), NavigatedTo of the new one,
	 * then Navigated of the Frame. The Frame keeps `parameter` as it is, it must stay valid while the history holds the entry.
	 * @return false, with the page and the history as they were, when the factory is NULL (reported) or refuses or does not
	 * return a new Page, when no memory, or when a callback of a navigation calls it. A handler may destroy the frame: do not use it after.
	 */
	ZRNUI_API bool Zframe_navigate(Zid frame, ZPageFactory factory, void* parameter);
	ZRNUI_API bool Zframe_can_go_back(Zid frame);
	ZRNUI_API bool Zframe_can_go_forward(Zid frame);
	/**
	 * @brief Frame.GoBack / GoForward: shows the previous (next) page: the one that was cached (NavigationCacheMode), else built
	 * again by the factory of its entry. @return false when there is no such entry, the factory refuses, or a callback of a navigation calls it.
	 */
	ZRNUI_API bool Zframe_go_back(Zid frame);
	ZRNUI_API bool Zframe_go_forward(Zid frame);
	/** @brief Frame.BackStackDepth: the pages behind the current one. */
	ZRNUI_API int Zframe_get_back_stack_depth(Zid frame);
	/** @brief The page shown, Z_INVALID_ID before the first Navigate. During a transition it is already the new page, though the old one is still on screen. */
	ZRNUI_API Zid Zframe_get_page(Zid frame);
	/** @brief Frame.CurrentSourcePageType: the factory of the page shown, NULL before the first Navigate. */
	ZRNUI_API ZPageFactory Zframe_get_current_source_page_type(Zid frame);
	/** @brief Frame.Navigated: the page is shown (after its OnNavigatedTo). One handler, NULL removes it. */
	ZRNUI_API void Zframe_on_navigated(Zid frame, ZRoutedCallback callback, void* user_data);


	/* ============================================================================
	 *  CONTENTCONTROL
	 * ============================================================================ */

	/** @brief A ContentControl with no content: it only lays out its content. */
	ZRNUI_API Zid Zcontent_control_create(void);

	/**
	 * @brief Content as an element. The control takes ownership; the old
	 * content is destroyed. NULL (Z_INVALID_ID) empties it. An element that has a parent is moved here;
	 * one that would become its own ancestor is refused (Z_ERROR_CYCLE) and the old content stays.
	 */
	ZRNUI_API void Zcontent_control_set_content(Zid control, Zid content);
	/** @brief Content as a string: the control makes a TextBlock that shows it with the font of the control. */
	ZRNUI_API void Zcontent_control_set_text(Zid control, const char* text);
	/** @return The element that is the content (the TextBlock of a string), Z_INVALID_ID when empty. */
	ZRNUI_API Zid Zcontent_control_get_content(Zid control);
	/** @return The text of a string content, "" for an element content or none. */
	ZRNUI_API const char* Zcontent_control_get_text(Zid control);

	/**
	 * @brief A ToolTip: a ContentControl with `text` as content, a 1 px border and 8,4 padding. It is only the box:
	 * nothing shows it by itself (Zelement_set_tooltip is what shows a text when the pointer rests on an element).
	 */
	ZRNUI_API Zid Ztool_tip_create(const char* text);


	/* ============================================================================
	 *  BUTTONS
	 * ============================================================================ */

	/** @brief Button with a string content (NULL: no content). The same for the other buttons below, Z_INVALID_ID when the control could not be made. */
	ZRNUI_API Zid Zbutton_create(const char* content);
	ZRNUI_API Zid Zhyperlink_button_create(const char* content);
	ZRNUI_API Zid Zrepeat_button_create(const char* content);
	/**
	 * Default sizes of the 8.1 styles (the control is left aligned, vertically centered): Button, RepeatButton
	 * and ToggleButton MinWidth 109, MinHeight 57.5 with 9.5 px of touch overhang above and below the outline;
	 * CheckBox and RadioButton MinWidth 168 with 16 px above and below; HyperlinkButton has none of it.
	 */

	/**
	 * @brief ButtonBase.Click, for every kind of button (also a ToggleButton, CheckBox...). Fired on release inside the button,
	 * or per ClickMode; a RepeatButton fires it on press and then repeats. A disabled button raises none. One handler, NULL removes it.
	 * The handler may destroy the button or its ancestors (a tap that opens another page): do not use the handle after.
	 */
	ZRNUI_API void Zbutton_on_click(Zid button, ZRoutedCallback callback, void* user_data);
	/** @brief ButtonBase.ClickMode, Z_CLICK_RELEASE by default. A RepeatButton clicks on a pointer press or a Space/Enter press whatever the mode (the keyboard does not repeat it). */
	ZRNUI_API void Zbutton_set_click_mode(Zid button, ZClickMode mode);
	/** @brief IsPressed: the button is held (pointer or Space/Enter) and the pointer is on it. */
	ZRNUI_API bool Zbutton_is_pressed(Zid button);
	ZRNUI_API bool Zbutton_is_pointer_over(Zid button);
	/**
	 * @brief Raises Click now, as if the user had pressed it (Automation Invoke): a ToggleButton also changes its state.
	 * Does nothing for a disabled button.
	 */
	ZRNUI_API void Zbutton_click(Zid button);

	/** @brief HyperlinkButton.NavigateUri: only stored; the program opens it in Click. The getter returns "" when none was set. */
	ZRNUI_API void Zhyperlink_button_set_uri(Zid button, const char* uri);
	ZRNUI_API const char* Zhyperlink_button_get_uri(Zid button);

	/**
	 * @brief RepeatButton.Delay: milliseconds from the press to the first repeat (default 250; a negative value is 0).
	 * Click fires on the press, then after Delay and every Interval while the button is held with the pointer on it
	 * (needs Zroot_tick). A Space or Enter press fires one Click, on the press, and does not repeat.
	 */
	ZRNUI_API void Zrepeat_button_set_delay(Zid button, int delay_ms);
	ZRNUI_API int Zrepeat_button_get_delay(Zid button);
	/** @brief RepeatButton.Interval: milliseconds between repeats (default 250; a negative value is 0). */
	ZRNUI_API void Zrepeat_button_set_interval(Zid button, int interval_ms);
	ZRNUI_API int Zrepeat_button_get_interval(Zid button);

	/** @brief A ToggleButton, unchecked: it fills with the accent while checked. A CheckBox is a ToggleButton with a box before its content. */
	ZRNUI_API Zid Ztoggle_button_create(const char* content);
	ZRNUI_API Zid Zcheck_box_create(const char* content);
	/**
	 * @brief A RadioButton, unchecked. Checking one unchecks the others of its group (their Unchecked events come after its Checked).
	 * @param group_name RadioButton.GroupName: the radio buttons with the same name exclude each other wherever they are in the tree of
	 * the root. NULL or "" groups by parent: those with no name that have the same parent.
	 */
	ZRNUI_API Zid Zradio_button_create(const char* content, const char* group_name);
	/** @brief Changes the group; NULL or "" groups by parent. A radio button that is checked stays checked. */
	ZRNUI_API void Zradio_button_set_group_name(Zid radio_button, const char* group_name);

	/**
	 * @brief IsChecked. A plain bool for two states; Ztoggle_button_set_state for the third. The getter is false for
	 * Z_INDETERMINATE. Checked, Unchecked or Indeterminate is raised when the state changes, also when the program sets it,
	 * never for the state it already has.
	 */
	ZRNUI_API void Ztoggle_button_set_is_checked(Zid toggle, bool is_checked);
	ZRNUI_API bool Ztoggle_button_get_is_checked(Zid toggle);
	ZRNUI_API void Ztoggle_button_set_state(Zid toggle, ZToggleState state);
	ZRNUI_API ZToggleState Ztoggle_button_get_state(Zid toggle);
	/**
	 * @brief IsThreeState: a click cycles unchecked, checked, indeterminate. Without it a click flips between the two states;
	 * a click on a RadioButton only checks it. Setting the state from the program is not limited by it.
	 */
	ZRNUI_API void Ztoggle_button_set_is_three_state(Zid toggle, bool is_three_state);
	/** @brief Checked, Unchecked and Indeterminate events: Zelement_on(Z_EVENT_CHECKED...). */


	/* ============================================================================
	 *  TOGGLESWITCH
	 *
	 *  A header, the content of the state ("Off" / "On") and a 77 x 36 switch at the right
	 *  that is off at first: a tap on the switch or Space flips it (the header, the text and the
	 *  empty space take no tap), a drag that starts on the knob carries it. Toggled comes
	 *  whenever IsOn changes (also when the program sets it, never for the same value);
	 *  read the state from the sender. The knob slides for 180 ms when animations run.
	 *  Header and contents are strings here (no templates).
	 * ============================================================================ */

	/** @brief A ToggleSwitch that is off. */
	ZRNUI_API Zid Ztoggle_switch_create(void);
	/** @brief IsOn. Raises Toggled when the value changes; the knob slides to its place. */
	ZRNUI_API void Ztoggle_switch_set_is_on(Zid toggle_switch, bool is_on);
	ZRNUI_API bool Ztoggle_switch_get_is_on(Zid toggle_switch);
	/** @brief ToggleSwitch.Header: a small text above the content; "" or NULL takes it away. The getter returns "" when there is none. */
	ZRNUI_API void Ztoggle_switch_set_header(Zid toggle_switch, const char* header);
	ZRNUI_API const char* Ztoggle_switch_get_header(Zid toggle_switch);
	/** @brief OnContent and OffContent (default "On" and "Off"): shown while IsOn is true / false. "" or NULL shows nothing, and the getters return "". */
	ZRNUI_API void Ztoggle_switch_set_on_content(Zid toggle_switch, const char* content);
	ZRNUI_API const char* Ztoggle_switch_get_on_content(Zid toggle_switch);
	ZRNUI_API void Ztoggle_switch_set_off_content(Zid toggle_switch, const char* content);
	ZRNUI_API const char* Ztoggle_switch_get_off_content(Zid toggle_switch);
	/** @brief ToggleSwitch.Toggled: IsOn changed; read it with Ztoggle_switch_get_is_on. One handler. The handler may destroy the switch. */
	ZRNUI_API void Ztoggle_switch_on_toggled(Zid toggle_switch, ZRoutedCallback callback, void* user_data);


	/* ============================================================================
	 *  PROGRESSRING
	 *
	 *  Five dots run round a 57.5 px ring while IsActive (needs Zroot_tick), over the
	 *  Background of the page at 0.8 opacity that covers the control. Nothing shows
	 *  while it is not active. It takes no input.
	 * ============================================================================ */

	/** @brief A ProgressRing that is not active (nothing shows). */
	ZRNUI_API Zid Zprogress_ring_create(void);
	/** @brief IsActive: starts or stops the dots; the animation restarts each time it is turned on. */
	ZRNUI_API void Zprogress_ring_set_is_active(Zid progress_ring, bool is_active);
	ZRNUI_API bool Zprogress_ring_get_is_active(Zid progress_ring);


	/* ============================================================================
	 *  RANGEBASE: SLIDER, PROGRESSBAR, SCROLLBAR
	 * ============================================================================ */

	/**
	 * @brief Slider with Minimum 0, Maximum 100, Value 0, StepFrequency 1, SmallChange 1 and LargeChange 10
	 * (the 8.1 defaults, as far as they are known). It is a 57 px band (plus the header) as wide as its room.
	 */
	ZRNUI_API Zid Zslider_create(ZOrientation orientation);
	/** @brief ProgressBar with Minimum 0, Maximum 100, Value 0: 3.5 px high, takes no input and no focus. */
	ZRNUI_API Zid Zprogress_bar_create(void);
	/** @brief ScrollBar, vertical or horizontal, with Minimum 0, Maximum 1, Value 0 and ViewportSize 0.1. */
	ZRNUI_API Zid Zscroll_bar_create(ZOrientation orientation);

	/**
	 * @brief Value is coerced into [Minimum, Maximum]; NaN is ignored. ValueChanged fires only when the value changes,
	 * also for a value the program sets. The getters return 0 for a Zid that is not a RangeBase.
	 */
	ZRNUI_API void Zrange_base_set_value(Zid range, double value);
	ZRNUI_API double Zrange_base_get_value(Zid range);
	/**
	 * @brief Minimum and Maximum; the value is coerced again (ValueChanged fires when that changes it). NaN changes nothing;
	 * a maximum below the minimum becomes the minimum.
	 */
	ZRNUI_API void Zrange_base_set_range(Zid range, double minimum, double maximum);
	ZRNUI_API double Zrange_base_get_minimum(Zid range);
	ZRNUI_API double Zrange_base_get_maximum(Zid range);
	/** @brief SmallChange (arrows, wheel) and LargeChange (clicks in the track). */
	ZRNUI_API void Zrange_base_set_changes(Zid range, double small_change, double large_change);
	/** @brief ValueChanged: one handler per control, NULL removes it. The handler may destroy the control. */
	ZRNUI_API void Zrange_base_on_value_changed(Zid range, ZValueChangedCallback callback, void* user_data);

	/** @brief Slider.Orientation, set by Zslider_create at first; the slider is laid out again. */
	ZRNUI_API void Zslider_set_orientation(Zid slider, ZOrientation orientation);
	/** @brief Slider.Header: a text above the slider; "" or NULL takes it away. */
	ZRNUI_API void Zslider_set_header(Zid slider, const char* header);
	ZRNUI_API const char* Zslider_get_header(Zid slider);
	/** @brief StepFrequency (default 1): what a touch or a key snaps the value to; 0 or a negative value for no snapping. A value the program sets is not snapped. */
	ZRNUI_API void Zslider_set_step_frequency(Zid slider, double step_frequency);
	ZRNUI_API double Zslider_get_step_frequency(Zid slider);
	/** @brief TickFrequency: the distance between tick marks; 0 (the default) draws none. */
	ZRNUI_API void Zslider_set_tick_frequency(Zid slider, double tick_frequency);
	ZRNUI_API double Zslider_get_tick_frequency(Zid slider);
	/** @brief TickPlacement, Z_TICK_OUTSIDE by default. */
	ZRNUI_API void Zslider_set_tick_placement(Zid slider, ZTickPlacement placement);
	/** @brief SnapsTo: with Z_SNAPS_TO_TICKS a touch or a key lands on the tick marks instead of the steps. */
	ZRNUI_API void Zslider_set_snaps_to(Zid slider, ZSliderSnapsTo snaps_to);
	/** @brief ProgressBar.IsIndeterminate: five squares run along the bar and the track is hidden (needs Zroot_tick). */
	ZRNUI_API void Zprogress_bar_set_is_indeterminate(Zid progress_bar, bool is_indeterminate);
	ZRNUI_API bool Zprogress_bar_get_is_indeterminate(Zid progress_bar);
	/** @brief ShowError: the indicator disappears. ShowPaused: the indicator is half transparent. Both are false at first. */
	ZRNUI_API void Zprogress_bar_set_show_error(Zid progress_bar, bool show_error);
	ZRNUI_API void Zprogress_bar_set_show_paused(Zid progress_bar, bool show_paused);

	/** @brief ScrollBar.ViewportSize: the size of the thumb relative to the range (a negative value is 0). */
	ZRNUI_API void Zscroll_bar_set_viewport_size(Zid scrollbar, double viewport_size);
	/**
	 * @brief ScrollBar.Scroll: raised after ValueChanged for each move the user makes (also when the value did not change),
	 * with what was done and the value. One handler. The handler may destroy the bar.
	 */
	ZRNUI_API void Zscroll_bar_on_scroll(Zid scrollbar, ZScrollCallback callback, void* user_data);

	/** @brief A Thumb: a block that reports a drag, it moves nothing itself. It takes no focus. */
	ZRNUI_API Zid Zthumb_create(void);
	/**
	 * @brief DragStarted, DragDelta and DragCompleted: one handler each, NULL removes it. A disabled thumb ignores presses;
	 * disabling it during a drag ends it with DragCompleted, as does any other loss of the pointer capture.
	 */
	ZRNUI_API void Zthumb_on_drag_started(Zid thumb, ZDragCallback callback, void* user_data);
	ZRNUI_API void Zthumb_on_drag_delta(Zid thumb, ZDragCallback callback, void* user_data);
	ZRNUI_API void Zthumb_on_drag_completed(Zid thumb, ZDragCallback callback, void* user_data);
	/** @brief IsDragging: between DragStarted and DragCompleted. */
	ZRNUI_API bool Zthumb_is_dragging(Zid thumb);

	/** @brief A thin line between items: horizontal in a vertical stack and the other way. */
	ZRNUI_API Zid Zseparator_create(void);


	/* ============================================================================
	 *  SCROLLVIEWER
	 * ============================================================================ */

	/** @brief A ScrollViewer with no content: it scrolls vertically (bar Z_SCROLLBAR_AUTO) and not horizontally (Z_SCROLLBAR_DISABLED). */
	ZRNUI_API Zid Zscroll_viewer_create(void);
	/**
	 * @brief ScrollViewer.Content: any element, which the viewer owns; it is measured with unlimited room along the
	 * scrolling axes. Replacing it, or Z_INVALID_ID, destroys the old one.
	 */
	ZRNUI_API void Zscroll_viewer_set_content(Zid scroll_viewer, Zid content);
	/**
	 * @brief Whether an axis scrolls and shows its bar. Z_SCROLLBAR_DISABLED: the axis does not scroll and the content gets the
	 * size of the viewer; AUTO: scrolls, the thin bar shows while it moves; HIDDEN: scrolls, no bar; VISIBLE: scrolls, the bar always shows.
	 */
	ZRNUI_API void Zscroll_viewer_set_vertical_scrollbar_visibility(Zid scroll_viewer, ZScrollBarVisibility visibility);
	ZRNUI_API void Zscroll_viewer_set_horizontal_scrollbar_visibility(Zid scroll_viewer, ZScrollBarVisibility visibility);
	/**
	 * @brief Jumps to an offset in px, stopping any inertia. It is clamped to [0, extent - viewport] as known from the last layout
	 * (so before the first layout it ends up 0). Raises Scroll, also when the offset did not change.
	 */
	ZRNUI_API void Zscroll_viewer_scroll_to_vertical_offset(Zid scroll_viewer, double offset);
	ZRNUI_API void Zscroll_viewer_scroll_to_horizontal_offset(Zid scroll_viewer, double offset);
	/** @brief The offsets, the size of the content (extent) and of the view (viewport) as of the last layout; 0 for a Zid that is not a ScrollViewer. */
	ZRNUI_API double Zscroll_viewer_get_vertical_offset(Zid scroll_viewer);
	ZRNUI_API double Zscroll_viewer_get_horizontal_offset(Zid scroll_viewer);
	ZRNUI_API double Zscroll_viewer_get_extent_height(Zid scroll_viewer);
	ZRNUI_API double Zscroll_viewer_get_extent_width(Zid scroll_viewer);
	ZRNUI_API double Zscroll_viewer_get_viewport_height(Zid scroll_viewer);
	ZRNUI_API double Zscroll_viewer_get_viewport_width(Zid scroll_viewer);
	/**
	 * @brief Raised when the offset moves: a touch drag, each step of the inertia (needs Zroot_tick), the wheel, or the program.
	 * See ZScrollCallback for what is reported. One handler, NULL removes it.
	 */
	ZRNUI_API void Zscroll_viewer_on_scroll(Zid scroll_viewer, ZScrollCallback callback, void* user_data);


	/* ============================================================================
	 *  TEXTBOX, PASSWORDBOX
	 * ============================================================================ */

	/** @brief A TextBox with `text` (NULL: empty) and no TextChanged for it. */
	ZRNUI_API Zid Ztext_box_create(const char* text);
	/** @brief A PasswordBox: the text shows as bullets (see Zpassword_box_set_password_char). It is a TextBox: the Ztext_box_* functions work on it too. */
	ZRNUI_API Zid Zpassword_box_create(void);

	/**
	 * A TextBox has the margin 0,9.5 and MinWidth 51 of its style, and its box is 34 px high at least.
	 * @brief TextBox.Text / PasswordBox.Password, UTF-8. TextChanged fires when it changes, not when the program sets the same text.
	 * The caret goes to the end. The getter returns "" for an empty box (or a Zid that is not a TextBox) and the string
	 * stays valid until the next change of the text. MaxLength does not apply to a text the program sets.
	 */
	ZRNUI_API void Ztext_box_set_text(Zid text_box, const char* text);
	ZRNUI_API const char* Ztext_box_get_text(Zid text_box);
	/** @brief AcceptsReturn: Enter makes a new line (multi-line box); without it a new line in typed or pasted text is dropped. Wraps with Ztext_box_set_text_wrapping. */
	ZRNUI_API void Ztext_box_set_accepts_return(Zid text_box, bool accepts_return);
	/** @brief TextWrapping: Z_WRAP wraps the lines at the width of the box (the default is Z_NO_WRAP). */
	ZRNUI_API void Ztext_box_set_text_wrapping(Zid text_box, ZTextWrapping wrapping);
	/** @brief IsReadOnly: the user cannot edit the text (it can still be selected); Ztext_box_set_text still works. */
	ZRNUI_API void Ztext_box_set_is_read_only(Zid text_box, bool is_read_only);
	ZRNUI_API bool Ztext_box_get_is_read_only(Zid text_box);
	/**
	 * @brief MaxLength in characters (UTF-8 code points, not bytes); 0 or a negative value is no limit. It limits what the user
	 * types or pastes (the part that does not fit is cut off); the text already in the box, and Ztext_box_set_text, are not cut.
	 */
	ZRNUI_API void Ztext_box_set_max_length(Zid text_box, int max_length);
	ZRNUI_API int Ztext_box_get_max_length(Zid text_box);
	/**
	 * @brief Selection in BYTE offsets of the UTF-8 text. `start` and `length` are clamped to the text, and moved back to the
	 * start of a character when they fall inside one; the caret goes to the end of the selection. The getters return byte
	 * offsets too (0 for a Zid that is not a TextBox).
	 */
	ZRNUI_API void Ztext_box_select(Zid text_box, int start, int length);
	ZRNUI_API void Ztext_box_select_all(Zid text_box);
	ZRNUI_API int Ztext_box_get_selection_start(Zid text_box);
	ZRNUI_API int Ztext_box_get_selection_length(Zid text_box);
	/** @brief PlaceholderText: shown in the box while it is empty and has no focus; "" or NULL takes it away. Also for a PasswordBox. */
	ZRNUI_API void Ztext_box_set_placeholder_text(Zid text_box, const char* placeholder_text);
	ZRNUI_API const char* Ztext_box_get_placeholder_text(Zid text_box);
	/** @brief Header: a small text above the box (16 px, PhoneMid); "" or NULL takes it away. Also for a PasswordBox. */
	ZRNUI_API void Ztext_box_set_header(Zid text_box, const char* header);
	ZRNUI_API const char* Ztext_box_get_header(Zid text_box);
	/** @brief PasswordBox.Password: the same text Ztext_box_get_text returns. These work only on a PasswordBox (a TextBox is reported and left alone). */
	ZRNUI_API void Zpassword_box_set_password(Zid password_box, const char* password);
	ZRNUI_API const char* Zpassword_box_get_password(Zid password_box);
	/** @brief As Ztext_box_set_header and Ztext_box_set_placeholder_text, for a PasswordBox only. */
	ZRNUI_API void Zpassword_box_set_header(Zid password_box, const char* header);
	ZRNUI_API void Zpassword_box_set_placeholder_text(Zid password_box, const char* placeholder_text);
	/** @brief PasswordBox.PasswordChar: the Unicode code point shown for each character; U+25CF (a bullet) by default. */
	ZRNUI_API void Zpassword_box_set_password_char(Zid password_box, uint32_t codepoint);


	/* ============================================================================
	 *  ITEMSCONTROL, SELECTOR
	 *
	 *  An item is a container (ListBoxItem, ListViewItem...) in the items panel; an item that is
	 *  a string gets a TextBlock for content. A ListBox, a ListView and a GridView show the panel
	 *  in a ScrollViewer. Items are not virtualized: every container exists (thousands are fine),
	 *  but only what is in view is painted.
	 *
	 *    Zid list = Zlist_view_create();
	 *    Zlist_view_set_selection_mode(list, Z_LIST_VIEW_SELECTION_NONE);
	 *    Zlist_view_set_is_item_click_enabled(list, true);
	 *    Zlist_view_on_item_click(list, opened, NULL);
	 *    Zitems_control_add_text(list, "Wi-Fi");
	 *
	 *  The text of a string item is 12 px, the ControlContentThemeFontSize of the ListBox and
	 *  ListView styles; set Zcontrol_set_font_size on the control or on the container to change it.
	 * ============================================================================ */

	/** @brief A plain ItemsControl: its items are the elements themselves, in a StackPanel, with no container around them. */
	ZRNUI_API Zid Zitems_control_create(void);

	/** @brief Adds an item that is a string (NULL is ""): the control makes its container. @return The container (the TextBlock in a plain ItemsControl), Z_INVALID_ID when not added. */
	ZRNUI_API Zid Zitems_control_add_text(Zid items_control, const char* text);
	/**
	 * @brief Adds an element as an item (Items.Add). Wrapped in the container of the control (ListBoxItem,
	 * ListViewItem, GridViewItem, ComboBoxItem, FlipViewItem) unless it already is a SelectorItem; the control owns it from now on.
	 * @return The container, Z_INVALID_ID when nothing was added: an element that is wrapped must have no parent (Z_ERROR_HAS_PARENT).
	 */
	ZRNUI_API Zid Zitems_control_add(Zid items_control, Zid item);
	/**
	 * @brief Items.RemoveAt: the container (and the item in it) is destroyed; an index out of range does nothing.
	 * Removing a selected item raises SelectionChanged.
	 */
	ZRNUI_API void Zitems_control_remove_at(Zid items_control, int index);
	/** @brief Items.Clear: destroys every item and group header. SelectionChanged is raised when a selected item was among them. */
	ZRNUI_API void Zitems_control_clear(Zid items_control);
	/** @return The number of items; the header items of groups are not counted. */
	ZRNUI_API int Zitems_control_get_count(Zid items_control);
	/** @return The container (ListBoxItem...) at an index, Z_INVALID_ID when out of range; the header items of groups are not counted. */
	ZRNUI_API Zid Zitems_control_get_container(Zid items_control, int index);
	/** @brief ItemContainerGenerator.IndexFromContainer: the index of a container, -1 when it is not one of the items. */
	ZRNUI_API int Zitems_control_index_from_container(Zid items_control, Zid container);
	/** @brief The text of an item that was added as a string, or the text content of its container; "" for an index out of range or an item with no text. */
	ZRNUI_API const char* Zitems_control_get_text(Zid items_control, int index);
	/**
	 * @brief ItemsPanel: the panel the containers are laid out by, Zitems_stack_panel_create or Zitems_wrap_grid_create
	 * (or a StackPanel, a VirtualizingStackPanel, a WrapPanel). The control takes ownership and moves the containers
	 * to it. A ListView has an ItemsStackPanel and a GridView an ItemsWrapGrid at first. The view scrolls along the
	 * orientation of the panel: sideways for a horizontal stack and for a vertical wrap grid.
	 * A panel that already has a parent, or a Zid that is not a panel, is refused and nothing changes.
	 */
	ZRNUI_API void Zitems_control_set_items_panel(Zid items_control, Zid panel);
	/** @return The panel, Z_INVALID_ID for a Zid that is not an ItemsControl. */
	ZRNUI_API Zid Zitems_control_get_items_panel(Zid items_control);
	/**
	 * @return The ScrollViewer of a ListBox, ListView or GridView (the template part named ScrollViewer), also the one of the
	 * drop-down of a ComboBox or an AutoSuggestBox; Z_INVALID_ID for the others (a plain ItemsControl, a FlipView).
	 */
	ZRNUI_API Zid Zitems_control_get_scroll_viewer(Zid items_control);

	/**
	 * @brief SelectedIndex, -1 for none; with several items selected the first. Selects that item alone. An index out of range
	 * deselects (as -1). With SelectionMode None nothing happens, and a FlipView cannot be left with no item. SelectionChanged
	 * (Z_EVENT_SELECTION_CHANGED) fires only when the selection changes. The getter returns -1 for none or for a Zid that is not a Selector.
	 */
	ZRNUI_API void Zselector_set_selected_index(Zid selector, int index);
	ZRNUI_API int Zselector_get_selected_index(Zid selector);
	/** @brief The container of SelectedItem, Z_INVALID_ID for none; its content is the item. */
	ZRNUI_API Zid Zselector_get_selected_container(Zid selector);

	/**
	 * @brief SelectorItem.IsSelected: selecting a container that is in a selector selects it the way SelectionMode lets it
	 * (Single deselects the others; None changes nothing) and raises SelectionChanged. A container that is not in a selector just stores the flag.
	 */
	ZRNUI_API void Zselector_item_set_is_selected(Zid selector_item, bool is_selected);
	ZRNUI_API bool Zselector_item_get_is_selected(Zid selector_item);


	/* ============================================================================
	 *  LISTBOX
	 *
	 *  The 8.1 ListBox: a PhoneBaseMediumHigh box (white at 80%) with black text; a row is
	 *  6.5,8 padded, leans while pressed, and fills with the accent when it is selected.
	 *  Single: a tap selects. Multiple: a tap toggles. Extended: a tap selects that one only.
	 *  In the Light theme the 8.1 dictionary gives the box black 75% and the text PhoneBaseBlack, black on black: here the
	 *  text of a ListBox row is PhoneBaseWhite in Light (the one departure from themeresources.xaml).
	 * ============================================================================ */

	/** @return The new ListBox, Z_INVALID_ID on failure. Every Z*_create below returns Z_INVALID_ID on failure too. */
	ZRNUI_API Zid Zlist_box_create(void);
	/**
	 * @brief An item container by itself, to give to Zitems_control_add. `content` is its text (NULL for none).
	 * Until it is added the program owns it (Zelement_destroy).
	 */
	ZRNUI_API Zid Zlist_box_item_create(const char* content);
	/** @brief ListBox.SelectionMode, Z_SELECTION_SINGLE by default. Going to Single keeps only the first selected item (SelectionChanged if that changes the selection). */
	ZRNUI_API void Zlist_box_set_selection_mode(Zid list_box, ZSelectionMode mode);
	ZRNUI_API ZSelectionMode Zlist_box_get_selection_mode(Zid list_box);
	/**
	 * @brief SelectedItems: writes up to `max` indices, ascending, into `indices` (which may be NULL to only count).
	 * @return How many items are selected (also when more than `max`), 0 for an invalid handle.
	 */
	ZRNUI_API int Zlist_box_get_selected_indices(Zid list_box, int* indices, int max);
	/** @brief ListBox.SelectAll (Multiple and Extended; does nothing in Single). Raises SelectionChanged when the selection changes. */
	ZRNUI_API void Zlist_box_select_all(Zid list_box);
	/** @brief ListBox.ScrollIntoView: scrolls the least that brings the item into view. Does nothing for an index out of range or when the box is not in a tree with a root. */
	ZRNUI_API void Zlist_box_scroll_into_view(Zid list_box, int index);


	/* ============================================================================
	 *  LISTVIEW, GRIDVIEW (ListViewBase)
	 *
	 *  The 8.1 containers of the phone. A ListView stacks its items; a GridView wraps them in an
	 *  ItemsWrapGrid, rows left to right. Selection looks as in the templates: Single shows nothing
	 *  (the program reads SelectedIndex), Multiple and Extended show a check box 40.5 px wide at the
	 *  left of a list item, and an accent border with a check in the corner of a grid item; a
	 *  pressed item leans and a disabled one is at 40%.
	 *
	 *  A tap: with SelectionMode Single it selects, Multiple toggles, Extended selects that one
	 *  alone, None does nothing; then, when IsItemClickEnabled, ItemClick is raised. (The order of
	 *  the two is not in the SDK; this is what the library does.) The keyboard: arrow keys move
	 *  through the items and Space or Enter acts as a tap; Ctrl+A selects all in Multiple and Extended.
	 * ============================================================================ */

	/** @brief A ListView / a GridView. Zlist_view_* works on both. */
	ZRNUI_API Zid Zlist_view_create(void);
	ZRNUI_API Zid Zgrid_view_create(void);
	/**
	 * @brief An item container by itself, to give to Zitems_control_add (a ListViewItem or a GridViewItem).
	 * `content` is its text (NULL for none). Until it is added the program owns it (Zelement_destroy).
	 */
	ZRNUI_API Zid Zlist_view_item_create(const char* content);
	ZRNUI_API Zid Zgrid_view_item_create(const char* content);

	/**
	 * @brief ListViewBase.SelectionMode, Single by default. Going to None clears the selection, going to Single keeps only the
	 * first selected item; SelectionChanged is raised when that changes the selection.
	 */
	ZRNUI_API void Zlist_view_set_selection_mode(Zid list_view, ZListViewSelectionMode mode);
	ZRNUI_API ZListViewSelectionMode Zlist_view_get_selection_mode(Zid list_view);
	/** @brief IsItemClickEnabled, false by default. */
	ZRNUI_API void Zlist_view_set_is_item_click_enabled(Zid list_view, bool enabled);
	ZRNUI_API bool Zlist_view_get_is_item_click_enabled(Zid list_view);
	/**
	 * @brief ItemClick: a tap on an item while IsItemClickEnabled, with the index of the item. One handler; NULL removes it.
	 * It runs after the selection change of the tap (also in mode None); it does not run when a SelectionChanged
	 * handler removed the item. The handler may destroy the view.
	 */
	ZRNUI_API void Zlist_view_on_item_click(Zid list_view, ZItemClickCallback callback, void* user_data);
	/** @brief IsSwipeEnabled (default true): only stored, a swipe does not select. */
	ZRNUI_API void Zlist_view_set_is_swipe_enabled(Zid list_view, bool enabled);
	ZRNUI_API bool Zlist_view_get_is_swipe_enabled(Zid list_view);
	/**
	 * @brief SelectedItems: writes up to `max` indices, ascending, into `indices` (which may be NULL to only count).
	 * @return How many items are selected (also when more than `max`), 0 for an invalid handle.
	 */
	ZRNUI_API int Zlist_view_get_selected_indices(Zid list_view, int* indices, int max);
	/** @brief ListViewBase.SelectAll (Multiple and Extended; does nothing in the other modes). */
	ZRNUI_API void Zlist_view_select_all(Zid list_view);

	/** @brief Header as a string, shown above the items inside the scrolling area; "" or NULL takes it away. */
	ZRNUI_API void Zlist_view_set_header(Zid list_view, const char* header);
	/**
	 * @brief Header as an element; the view takes ownership and destroys the old header. Z_INVALID_ID takes it away.
	 * An invalid handle leaves the header as it is.
	 */
	ZRNUI_API void Zlist_view_set_header_content(Zid list_view, Zid header);
	/** @brief Footer, below the items: a string ("" or NULL takes it away), or an element (as the header). */
	ZRNUI_API void Zlist_view_set_footer(Zid list_view, const char* footer);
	ZRNUI_API void Zlist_view_set_footer_content(Zid list_view, Zid footer);

	/**
	 * @brief ScrollIntoView: scrolls the least that brings the item into view (the alignment variant as asked). Does nothing
	 * for an index out of range or when the view is not in a tree with a root.
	 */
	ZRNUI_API void Zlist_view_scroll_into_view(Zid list_view, int index);
	ZRNUI_API void Zlist_view_scroll_into_view_with_alignment(Zid list_view, int index, ZScrollIntoViewAlignment alignment);

	/**
	 * @brief A group in its simplest form: a ListViewHeaderItem (GridViewHeaderItem) with `title`, added to the items
	 * as they come; the items that follow belong to it. Indices count the items only, not the headers. In a GridView a
	 * header takes a row of its own. There are no collapsing groups, no sticky headers.
	 * @return The header item, Z_INVALID_ID on failure. `title` NULL is an empty title.
	 */
	ZRNUI_API Zid Zlist_view_add_group_header(Zid list_view, const char* title);
	/** @return How many group headers the view has, 0 for an invalid handle. Zitems_control_clear removes them. */
	ZRNUI_API int Zlist_view_get_group_count(Zid list_view);


	/* ============================================================================
	 *  COMBOBOX
	 *
	 *  The inline ComboBox of the phone: a 2.5 px outlined box (33.5 high, with the 9.5 px of touch
	 *  target above and below) with the selected item in it, or the PlaceholderText. A tap opens
	 *  the list over the box, white with an accent border, the selected item in the accent over
	 *  the place of the box; a tap on an item, a tap outside or Escape closes it. The full screen
	 *  picker the phone shows for a list of more than five items is not made: the list is always inline.
	 * ============================================================================ */

	ZRNUI_API Zid Zcombo_box_create(void);
	/** @brief An item container by itself, to give to Zitems_control_add. `content` is its text (NULL for none). */
	ZRNUI_API Zid Zcombo_box_item_create(const char* content);
	/**
	 * @brief IsDropDownOpen. DropDownOpened and DropDownClosed: Zelement_on(box, Z_EVENT_DROP_DOWN_OPENED...); they are
	 * raised when the state changes (also by this call), and a handler may destroy the box. Setting the state it has does nothing.
	 */
	ZRNUI_API void Zcombo_box_set_is_drop_down_open(Zid combo_box, bool is_open);
	ZRNUI_API bool Zcombo_box_get_is_drop_down_open(Zid combo_box);
	/** @brief MaxDropDownHeight in pixels; the list scrolls past it. Z_INFINITE (the default, also for 0 or less) keeps it to the screen. */
	ZRNUI_API void Zcombo_box_set_max_drop_down_height(Zid combo_box, float height);
	ZRNUI_API float Zcombo_box_get_max_drop_down_height(Zid combo_box);
	/** @brief Header: a small text above the box; "" or NULL takes it away. The getter returns "" for none, never NULL. */
	ZRNUI_API void Zcombo_box_set_header(Zid combo_box, const char* header);
	ZRNUI_API const char* Zcombo_box_get_header(Zid combo_box);
	/** @brief PlaceholderText: shown in the box while nothing is selected; "" or NULL takes it away. The getter returns "" for none, never NULL. */
	ZRNUI_API void Zcombo_box_set_placeholder_text(Zid combo_box, const char* placeholder_text);
	ZRNUI_API const char* Zcombo_box_get_placeholder_text(Zid combo_box);


	/* ============================================================================
	 *  FLIPVIEW
	 *
	 *  One item at a time, as wide and as high as the view. A finger drags the items sideways and
	 *  lets go on the next one when it went a fifth of the width or flicked; the left and right keys
	 *  go to the previous and the next. With animations the snap takes 300 ms (as in a Pivot),
	 *  without them it is at once. SelectionChanged comes when the item in view changes. It does not wrap round.
	 *  The flip buttons of the desktop FlipView are not drawn on the phone.
	 * ============================================================================ */

	ZRNUI_API Zid Zflip_view_create(void);
	/** @brief An item container by itself, to give to Zitems_control_add. `content` is its text (NULL for none). */
	ZRNUI_API Zid Zflip_view_item_create(const char* content);
	/**
	 * @brief UseTouchAnimationsForAllNavigation (default true): false makes a change of SelectedIndex by the program jump instead of slide
	 * (a finger or the keys still slide).
	 */
	ZRNUI_API void Zflip_view_set_use_touch_animations_for_all_navigation(Zid flip_view, bool use);
	ZRNUI_API bool Zflip_view_get_use_touch_animations_for_all_navigation(Zid flip_view);


	/* ============================================================================
	 *  AUTOSUGGESTBOX
	 *
	 *  A TextBox with PlaceholderText and Header and, under it, a white list with an accent border
	 *  of suggestions while IsSuggestionListOpen. Nothing is filtered: the program listens to
	 *  TextChanged, with the reason, and gives the suggestions for the text with
	 *  Zauto_suggest_box_set_items; a non empty list opens, an empty one closes. A tap on a suggestion
	 *  (or Enter on the one the Up and Down keys are on) puts it in the text (UpdateTextOnSelect), raises
	 *  SuggestionChosen, then TextChanged with Z_SUGGEST_SUGGESTION_CHOSEN, then QuerySubmitted.
	 *  Enter in the text raises QuerySubmitted with the text; QuerySubmitted and QueryIcon belong to
	 *  Windows 10: they are not in the 8.1 interface, QuerySubmitted is here for the Enter key.
	 *  The suggestions are items: Zitems_control_get_count and Zitems_control_get_text work on the box.
	 * ============================================================================ */

	ZRNUI_API Zid Zauto_suggest_box_create(void);
	/**
	 * @brief The suggestions (ItemsSource as strings), copied; they replace the old ones (and the highlight is dropped). The list opens
	 * when there are any and the text has the keyboard focus, and closes when there are none. A NULL string is an empty one.
	 */
	ZRNUI_API void Zauto_suggest_box_set_items(Zid box, const char* const* items, int count);
	/** @brief Text. Changing it by the program raises TextChanged with Z_SUGGEST_PROGRAMMATIC_CHANGE and does not open the list. The getter returns "" for an invalid handle, never NULL. */
	ZRNUI_API void Zauto_suggest_box_set_text(Zid box, const char* text);
	ZRNUI_API const char* Zauto_suggest_box_get_text(Zid box);
	/** @brief PlaceholderText and Header of the text; the getters return "" for none, never NULL. */
	ZRNUI_API void Zauto_suggest_box_set_placeholder_text(Zid box, const char* placeholder_text);
	ZRNUI_API const char* Zauto_suggest_box_get_placeholder_text(Zid box);
	ZRNUI_API void Zauto_suggest_box_set_header(Zid box, const char* header);
	ZRNUI_API const char* Zauto_suggest_box_get_header(Zid box);
	/** @brief IsSuggestionListOpen. Opening a list with no suggestions does nothing. It closes by itself when the keyboard leaves the box, on Escape and when a suggestion is chosen. */
	ZRNUI_API void Zauto_suggest_box_set_is_suggestion_list_open(Zid box, bool is_open);
	ZRNUI_API bool Zauto_suggest_box_get_is_suggestion_list_open(Zid box);
	/** @brief MaxSuggestionListHeight in pixels, 300 by default; 0 or less takes the limit away (Z_INFINITE). */
	ZRNUI_API void Zauto_suggest_box_set_max_suggestion_list_height(Zid box, float height);
	/** @brief UpdateTextOnSelect (default true): a chosen suggestion goes into the text (and TextChanged is raised for it). */
	ZRNUI_API void Zauto_suggest_box_set_update_text_on_select(Zid box, bool update);
	/**
	 * @brief TextChanged: the text changed, with the reason (user input, program, or a chosen suggestion). One handler; NULL removes it.
	 * The handler may destroy the box.
	 */
	ZRNUI_API void Zauto_suggest_box_on_text_changed(Zid box, ZAutoSuggestTextChangedCallback callback, void* user_data);
	/** @brief SuggestionChosen: the index of the suggestion that a tap or Enter chose; the list is already closed, the text not yet changed. One handler; NULL removes it. */
	ZRNUI_API void Zauto_suggest_box_on_suggestion_chosen(Zid box, ZAutoSuggestSuggestionChosenCallback callback, void* user_data);
	/**
	 * @brief QuerySubmitted: with the text and the index of the chosen suggestion, or -1 for Enter in the text with none chosen
	 * (the list is closed then). The text is as UpdateTextOnSelect left it. One handler; NULL removes it.
	 */
	ZRNUI_API void Zauto_suggest_box_on_query_submitted(Zid box, ZAutoSuggestQuerySubmittedCallback callback, void* user_data);


	/* ============================================================================
	 *  SEMANTICZOOM
	 *
	 *  Two views of the same data, the zoomed in one (a list of items) and the zoomed out one (a list
	 *  of its groups); one shows at a time, filling the control. There is no pinch: the program toggles the
	 *  view, usually from the ItemClick of the zoomed out list. (The 8.1 phone template has no zoom out
	 *  button, and the zoomed out view is shown in the place of the zoomed in one, not in a Popup.)
	 * ============================================================================ */

	ZRNUI_API Zid Zsemantic_zoom_create(void);
	/**
	 * @brief ZoomedInView and ZoomedOutView: any elements (a ListView, a GridView); the control takes ownership and
	 * destroys the view it had. Z_INVALID_ID takes the view away (and destroys it). A view that has a parent is refused
	 * with Z_ERROR_HAS_PARENT and the current view stays; so does an invalid handle.
	 */
	ZRNUI_API void Zsemantic_zoom_set_zoomed_in_view(Zid semantic_zoom, Zid view);
	ZRNUI_API void Zsemantic_zoom_set_zoomed_out_view(Zid semantic_zoom, Zid view);
	/**
	 * @brief IsZoomedInViewActive, true at first. Setting it is a view change, with the events (nothing happens when
	 * it is the view that shows already, or when CanChangeViews is false).
	 */
	ZRNUI_API void Zsemantic_zoom_set_is_zoomed_in_view_active(Zid semantic_zoom, bool active);
	ZRNUI_API bool Zsemantic_zoom_get_is_zoomed_in_view_active(Zid semantic_zoom);
	/** @brief ToggleActiveView: a view change to the other view, with the events. */
	ZRNUI_API void Zsemantic_zoom_toggle_active_view(Zid semantic_zoom);
	/** @brief CanChangeViews (default true): false keeps the view that shows; the setter of the active view and the toggle do nothing. */
	ZRNUI_API void Zsemantic_zoom_set_can_change_views(Zid semantic_zoom, bool can_change);
	/** @brief IsZoomOutButtonEnabled (default true): only stored, the phone template has no button and nothing reads it. */
	ZRNUI_API void Zsemantic_zoom_set_is_zoom_out_button_enabled(Zid semantic_zoom, bool enabled);
	/**
	 * @brief ViewChangeStarted, before the view changes, and ViewChangeCompleted, after it: `is_source_zoomed_in_view` is true when
	 * the change leaves the zoomed in view (it is the same in both). One handler each; NULL removes it. A handler may destroy
	 * the control (when Started does, the view does not change and Completed is not raised).
	 */
	ZRNUI_API void Zsemantic_zoom_on_view_change_started(Zid semantic_zoom, ZSemanticZoomCallback callback, void* user_data);
	ZRNUI_API void Zsemantic_zoom_on_view_change_completed(Zid semantic_zoom, ZSemanticZoomCallback callback, void* user_data);


	/* ============================================================================
	 *  HUB, HUBSECTION
	 *
	 *  The Hub of the Windows Phone 8.1 template: a big header (78 px Light) over sections that stand side by
	 *  side and scroll sideways with a finger, with inertia and no snap points (no scroll bars). A HubSection is
	 *  a header (19 px SemiBold, margin -1,5,0,31.5) over its content, with the margin 19,0,0,0; its content is its
	 *  Content (Zcontent_control_set_content), where XAML has a template. Each section keeps room for the header of the
	 *  hub above its own, so that they begin together under it.
	 *
	 *    Zid hub = Zhub_create();
	 *    Zhub_set_header(hub, "my app");
	 *    Zid news = Zhub_section_create("news");
	 *    Zcontent_control_set_content(news, list);
	 *    Zhub_add_section(hub, news);
	 *
	 *  A section is 90% of the width of the hub (the margin is outside of it), as in the WP 8.1 hub; a wide section is a
	 *  multiple of that (Zhub_section_set_width_multiple), a section by its content is the multiple 0 (Width="Auto"), and
	 *  Zelement_set_width gives a width in pixels, which wins over the multiple. The header of the hub goes at half the speed of the sections
	 *  (parallax; this is the library's number). Orientation Vertical stacks the sections in a vertical scroller; there
	 *  the header scrolls away with them. Not included: the parallax of the Background, SemanticZoomOwner, section header
	 *  templates, DefaultSectionIndex, an arrow on an interactive header (the phone template draws none).
	 * ============================================================================ */

	/** SectionHeaderClick of a Hub: the hub and the HubSection whose header was tapped. */
	typedef void (*ZHubSectionHeaderClickCallback)(Zid sender, Zid section, void* user_data);

	ZRNUI_API Zid Zhub_create(void);
	/** @brief Header: a string, "" or NULL takes it away. */
	ZRNUI_API void Zhub_set_header(Zid hub, const char* header);
	ZRNUI_API const char* Zhub_get_header(Zid hub);
	/** @brief IsLooping (default true; the library's, the phone hub does not): in a horizontal hub of two sections or more the
	 *  sections go round, past the last comes the first and before the first the last, with the same fling. The offsets of
	 *  Zhub_get_scroll_viewer are then those of a ring that is turned as it goes (not 0 at the first section). A ring too short
	 *  to fill the screen with others shows a gap where a section would have to be twice. */
	ZRNUI_API void Zhub_set_is_looping(Zid hub, bool is_looping);
	ZRNUI_API bool Zhub_get_is_looping(Zid hub);
	/** @brief Orientation, Z_HORIZONTAL by default. Setting it scrolls back to the start. */
	ZRNUI_API void Zhub_set_orientation(Zid hub, ZOrientation orientation);
	ZRNUI_API ZOrientation Zhub_get_orientation(Zid hub);
	/** @return The ScrollViewer of the template (the template part named ScrollViewer): its offsets tell where the hub is. */
	ZRNUI_API Zid Zhub_get_scroll_viewer(Zid hub);
	/**
	 * @brief Sections.Append: the hub owns the section. @return its index, -1 on failure (an invalid handle, or the section
	 * has a parent: Z_ERROR_HAS_PARENT).
	 */
	ZRNUI_API int Zhub_add_section(Zid hub, Zid hub_section);
	/** @brief Sections.Insert: an `index` out of range appends. Returns as Zhub_add_section. */
	ZRNUI_API int Zhub_insert_section(Zid hub, int index, Zid hub_section);
	/** @brief Sections.RemoveAt: the section is destroyed. An index out of range does nothing. */
	ZRNUI_API void Zhub_remove_section_at(Zid hub, int index);
	/** @brief Sections.Clear: all the sections are destroyed. */
	ZRNUI_API void Zhub_clear_sections(Zid hub);
	ZRNUI_API int Zhub_get_section_count(Zid hub);
	/** @return The section at `index`, Z_INVALID_ID when it is out of range. */
	ZRNUI_API Zid Zhub_get_section(Zid hub, int index);
	/** @brief ScrollToSection: puts the section at the start of the view (at once, as far as the extent lets it go). */
	ZRNUI_API void Zhub_scroll_to_section(Zid hub, int index);
	/** @brief SectionsInView: the sections that show at least half a pixel, in order. */
	ZRNUI_API int Zhub_get_sections_in_view_count(Zid hub);
	ZRNUI_API Zid Zhub_get_section_in_view(Zid hub, int index);
	/** @brief SectionsInViewChanged: raised from the scroll that makes other sections show (a finger, the inertia, ScrollToSection). */
	ZRNUI_API void Zhub_on_sections_in_view_changed(Zid hub, ZRoutedCallback callback, void* user_data);
	/** @brief SectionHeaderClick: a tap on the header of a section with IsHeaderInteractive. One handler; NULL removes it. */
	ZRNUI_API void Zhub_on_section_header_click(Zid hub, ZHubSectionHeaderClickCallback callback, void* user_data);

	/** @brief A HubSection with `header` (NULL for none). Until it is added to a hub the program owns it (Zelement_destroy). */
	ZRNUI_API Zid Zhub_section_create(const char* header);
	/** @brief Header of the section; the getter returns "" for none, never NULL. */
	ZRNUI_API void Zhub_section_set_header(Zid hub_section, const char* header);
	ZRNUI_API const char* Zhub_section_get_header(Zid hub_section);
	/** @brief IsHeaderInteractive (default false): the header can be tapped, for SectionHeaderClick. */
	ZRNUI_API void Zhub_section_set_is_header_interactive(Zid hub_section, bool is_interactive);
	ZRNUI_API bool Zhub_section_get_is_header_interactive(Zid hub_section);
	/** @brief WidthMultiple: the width of the section is this times 90% of the width of the hub (default 1, the standard
	 *  section; 2 is a wide one). 0 (or less) sizes it to its content, never wider than the screen less the margin.
	 *  A Width set by Zelement_set_width wins. Only in a horizontal hub. */
	ZRNUI_API void Zhub_section_set_width_multiple(Zid hub_section, float multiple);
	ZRNUI_API float Zhub_section_get_width_multiple(Zid hub_section);


	/* ============================================================================
	 *  LOOPINGSELECTOR
	 *
	 *  The column of the date and time pickers: items that go round, one in the middle. At rest only the
	 *  chosen item shows, filled with the accent; while a finger is on the column (and while it moves) the
	 *  others come out as boxes outlined in LoopingSelectorForeground, in 330 ms. A drag has inertia (a
	 *  third of the speed is left after 0.32 s) and ends on an item, which becomes the chosen one when the
	 *  column stops; a tap on an item goes to it, the arrow keys and the wheel move by one. The items are a
	 *  line of 45 px SemiBold text (spacing -24) with, under it, a smaller one of 17 px (the weekday of a day).
	 *  SelectionChanged (Z_EVENT_SELECTION_CHANGED) comes when the chosen item changes. Without animations the
	 *  column ends at once where the inertia would have taken it.
	 *  ItemWidth and ItemHeight are not in the XAML: the width is the widest text (64 px at least) in its margin
	 *  5 and border 2.5, the height the text with its paddings; the column is as high as its room.
	 * ============================================================================ */

	ZRNUI_API Zid Zlooping_selector_create(void);
	/**
	 * @brief Items.Append, with the text of the item and a smaller one under it (NULL or "" for none); both are copied.
	 * The first item added becomes the selected one (without SelectionChanged).
	 * @return the index, -1 on failure.
	 */
	ZRNUI_API int Zlooping_selector_add_item(Zid looping_selector, const char* primary_text, const char* secondary_text);
	/** @brief Items.Clear: the selected index becomes -1 (without SelectionChanged). */
	ZRNUI_API void Zlooping_selector_clear_items(Zid looping_selector);
	ZRNUI_API int Zlooping_selector_get_item_count(Zid looping_selector);
	/** @return The texts of an item; "" (never NULL) for an index out of range, an invalid handle or no secondary text. Valid until the items change. */
	ZRNUI_API const char* Zlooping_selector_get_item_text(Zid looping_selector, int index);
	ZRNUI_API const char* Zlooping_selector_get_item_secondary_text(Zid looping_selector, int index);
	/**
	 * @brief SelectedIndex, -1 without items. A change by the program is at once, and raises SelectionChanged when the index differs.
	 * An index out of range is clamped to the first or the last item; without items nothing happens. A handler may destroy the selector.
	 */
	ZRNUI_API void Zlooping_selector_set_selected_index(Zid looping_selector, int index);
	ZRNUI_API int Zlooping_selector_get_selected_index(Zid looping_selector);
	/** @brief ShouldLoop (default true): the items go round. false stops the column at the first and the last item. */
	ZRNUI_API void Zlooping_selector_set_should_loop(Zid looping_selector, bool should_loop);
	ZRNUI_API bool Zlooping_selector_get_should_loop(Zid looping_selector);
	/** @brief ItemWidth and ItemHeight: 0 (the default, and any value below it) is the size of the items as they come. The getters tell the size in use. */
	ZRNUI_API void Zlooping_selector_set_item_width(Zid looping_selector, float width);
	ZRNUI_API float Zlooping_selector_get_item_width(Zid looping_selector);
	ZRNUI_API void Zlooping_selector_set_item_height(Zid looping_selector, float height);
	ZRNUI_API float Zlooping_selector_get_item_height(Zid looping_selector);
	/**
	 * @brief SelectionChanged (Z_EVENT_SELECTION_CHANGED): the chosen item changed, by a finger, a key, a tap or the setter above.
	 * A handler may destroy the selector, also while the column snaps or runs on by inertia.
	 */
	ZRNUI_API void Zlooping_selector_on_selection_changed(Zid looping_selector, ZRoutedCallback callback, void* user_data);


	/* ============================================================================
	 *  DATEPICKER, TIMEPICKER, DATEPICKERFLYOUT, TIMEPICKERFLYOUT, PICKERFLYOUTBASE
	 *
	 *  A DatePicker (TimePicker) is a Header (16 px, PhoneMid) over a Button as wide as its room, 2.5 px outlined, with
	 *  the value in it ("November 7, 2026", "3:07 PM" or "15:07"; the formats are the library's: DayFormat and the like are not
	 *  here). A tap on the button shows the flyout, which fills the screen: the Header as its title, the selectors side by side
	 *  (month, day with its weekday, year; hour, minute, AM / PM) with the value as it is, and under them "done" and "cancel"
	 *  buttons. Done makes the selectors the value: the picker's Date changes and DateChanged comes (also, with the old and the new
	 *  date, DatePicked of the flyout, before the flyout closes). Cancel, Back and a tap outside close the flyout and leave the
	 *  value; the selectors go back to it. A change of the value by the program raises DateChanged too, when it is another date.
	 *  Choosing another month or year makes the days those of that month (a day past its end becomes the last one), February
	 *  of a leap year has 29; the calendar is the Gregorian, in plain arithmetic.
	 *
	 *    Zid picker = Zdate_picker_create();
	 *    Zdate_picker_set_header(picker, "birthday");
	 *    Zdate_picker_set_date(picker, (ZDate){ 1999, 12, 31 });
	 *    Zdate_picker_on_date_changed(picker, changed, NULL);
	 *
	 *  The flyouts can also be used by themselves: made, given a value, shown with Zflyout_base_show_at, and listened to
	 *  with DatePicked / TimePicked. They are FlyoutBase (Opening, Opened, Closed, Hide work) with the placement Full.
	 *  Defaults the SDK does not give: a DatePicker begins on today (UTC) and its years are 100 either side of this year;
	 *  a TimePicker on 0:00 with the 12 hour clock and MinuteIncrement 1; a time whose minute is not on the grid of
	 *  MinuteIncrement shows on the nearest minute in the flyout. Not included: CalendarIdentifier, Day, Month and YearFormat,
	 *  Orientation, the header and flyout templates.
	 * ============================================================================ */

	ZRNUI_API Zid Zdate_picker_create(void);
	/** @brief Header over the button; "" or NULL for none. The getter returns "" for none, never NULL. */
	ZRNUI_API void Zdate_picker_set_header(Zid date_picker, const char* header);
	ZRNUI_API const char* Zdate_picker_get_header(Zid date_picker);
	/**
	 * @brief Date: the year is clamped to MinYear..MaxYear, the month to 1..12, the day to one the month has.
	 * DateChanged (old and new date) comes when the result is another date, and it may destroy the picker.
	 * The getter returns {0, 0, 0} for an invalid handle.
	 */
	ZRNUI_API void Zdate_picker_set_date(Zid date_picker, ZDate date);
	ZRNUI_API ZDate Zdate_picker_get_date(Zid date_picker);
	/**
	 * @brief MinYear and MaxYear of the year selector. Setting one past the other moves the other to it; the date is
	 * clamped again (DateChanged if it changes). The getters return 0 for an invalid handle.
	 */
	ZRNUI_API void Zdate_picker_set_min_year(Zid date_picker, int year);
	ZRNUI_API void Zdate_picker_set_max_year(Zid date_picker, int year);
	ZRNUI_API int Zdate_picker_get_min_year(Zid date_picker);
	ZRNUI_API int Zdate_picker_get_max_year(Zid date_picker);
	/** @brief DayVisible, MonthVisible, YearVisible (default true): which selectors the flyout has (the flyout of the picker, when it still has one). */
	ZRNUI_API void Zdate_picker_set_day_visible(Zid date_picker, bool visible);
	ZRNUI_API void Zdate_picker_set_month_visible(Zid date_picker, bool visible);
	ZRNUI_API void Zdate_picker_set_year_visible(Zid date_picker, bool visible);
	ZRNUI_API bool Zdate_picker_get_day_visible(Zid date_picker);
	ZRNUI_API bool Zdate_picker_get_month_visible(Zid date_picker);
	ZRNUI_API bool Zdate_picker_get_year_visible(Zid date_picker);
	/**
	 * @return The flyout of the picker (a DatePickerFlyout the picker owns), and the Button that shows it (the template part
	 * FlyoutButton), the flyout being the Flyout of that button. Z_INVALID_ID for an invalid handle. Zdate_picker_get_flyout is
	 * Z_INVALID_ID too once the flyout was taken away (Zbutton_set_flyout with Z_INVALID_ID, or destroyed); the picker keeps working.
	 */
	ZRNUI_API Zid Zdate_picker_get_flyout(Zid date_picker);
	ZRNUI_API Zid Zdate_picker_get_flyout_button(Zid date_picker);
	/**
	 * @brief DateChanged: the Date of the picker changed, by the program or by Done of the flyout. One handler; NULL removes it.
	 * It may destroy the picker.
	 */
	ZRNUI_API void Zdate_picker_on_date_changed(Zid date_picker, ZDateChangedCallback callback, void* user_data);

	ZRNUI_API Zid Ztime_picker_create(void);
	/** @brief Header over the button; "" or NULL for none. The getter returns "" for none, never NULL. */
	ZRNUI_API void Ztime_picker_set_header(Zid time_picker, const char* header);
	ZRNUI_API const char* Ztime_picker_get_header(Zid time_picker);
	/**
	 * @brief Time: the hour is clamped to 0..23, the minute to 0..59 (a minute off the MinuteIncrement grid is kept; only the
	 * flyout shows the nearest one). TimeChanged comes when the result is another time, and it may destroy the picker.
	 * The getter returns {0, 0} for an invalid handle.
	 */
	ZRNUI_API void Ztime_picker_set_time(Zid time_picker, ZTimeOfDay time_of_day);
	ZRNUI_API ZTimeOfDay Ztime_picker_get_time(Zid time_picker);
	/** @brief MinuteIncrement (1 to 59, a value outside is clamped; default 1): the minute selector has 0, the increment, twice it... */
	ZRNUI_API void Ztime_picker_set_minute_increment(Zid time_picker, int increment);
	ZRNUI_API int Ztime_picker_get_minute_increment(Zid time_picker);
	/** @brief ClockIdentifier: Z_CLOCK_12_HOUR (the default, with an AM / PM selector) or Z_CLOCK_24_HOUR; another value is ignored. */
	ZRNUI_API void Ztime_picker_set_clock_identifier(Zid time_picker, ZClockIdentifier clock);
	ZRNUI_API ZClockIdentifier Ztime_picker_get_clock_identifier(Zid time_picker);
	/** @return The flyout and its button, as Zdate_picker_get_flyout and Zdate_picker_get_flyout_button, for a TimePickerFlyout. */
	ZRNUI_API Zid Ztime_picker_get_flyout(Zid time_picker);
	ZRNUI_API Zid Ztime_picker_get_flyout_button(Zid time_picker);
	/** @brief TimeChanged: the Time of the picker changed, by the program or by Done of the flyout. One handler; NULL removes it. It may destroy the picker. */
	ZRNUI_API void Ztime_picker_on_time_changed(Zid time_picker, ZTimeChangedCallback callback, void* user_data);

	/**
	 * @brief The flyouts by themselves; the Date of a flyout that was not shown yet is today. Until it is given to a button
	 * (Zbutton_set_flyout) the program owns it (Zelement_destroy). Setting a Date clamps it as Zdate_picker_set_date does,
	 * and the years as Zdate_picker_set_min_year / set_max_year (the getters return 0 for an invalid handle).
	 */
	ZRNUI_API Zid Zdate_picker_flyout_create(void);
	ZRNUI_API void Zdate_picker_flyout_set_date(Zid flyout, ZDate date);
	ZRNUI_API ZDate Zdate_picker_flyout_get_date(Zid flyout);
	ZRNUI_API void Zdate_picker_flyout_set_min_year(Zid flyout, int year);
	ZRNUI_API void Zdate_picker_flyout_set_max_year(Zid flyout, int year);
	ZRNUI_API int Zdate_picker_flyout_get_min_year(Zid flyout);
	ZRNUI_API int Zdate_picker_flyout_get_max_year(Zid flyout);
	/** @brief DayVisible, MonthVisible, YearVisible of the flyout (default true). */
	ZRNUI_API void Zdate_picker_flyout_set_day_visible(Zid flyout, bool visible);
	ZRNUI_API void Zdate_picker_flyout_set_month_visible(Zid flyout, bool visible);
	ZRNUI_API void Zdate_picker_flyout_set_year_visible(Zid flyout, bool visible);
	ZRNUI_API bool Zdate_picker_flyout_get_day_visible(Zid flyout);
	ZRNUI_API bool Zdate_picker_flyout_get_month_visible(Zid flyout);
	ZRNUI_API bool Zdate_picker_flyout_get_year_visible(Zid flyout);
	/**
	 * @brief DatePicked: Done was tapped. `old_date` is the flyout's Date before, `new_date` what the selectors show, and it is
	 * already the flyout's Date. The handler runs on every Done, also when the date is the same; then the
	 * flyout hands the date and its own range (min and max year) to the picker it belongs to (DateChanged), and closes. The handler may destroy
	 * the flyout. One handler; NULL removes it.
	 */
	ZRNUI_API void Zdate_picker_flyout_on_date_picked(Zid flyout, ZDatePickedCallback callback, void* user_data);

	/** @brief A TimePickerFlyout, as the date one: the program owns it until a button takes it. Time is clamped as in Ztime_picker_set_time. */
	ZRNUI_API Zid Ztime_picker_flyout_create(void);
	ZRNUI_API void Ztime_picker_flyout_set_time(Zid flyout, ZTimeOfDay time_of_day);
	ZRNUI_API ZTimeOfDay Ztime_picker_flyout_get_time(Zid flyout);
	/** @brief MinuteIncrement (clamped to 1..59) and ClockIdentifier of the flyout (another clock value is ignored); the defaults are those of a TimePicker. */
	ZRNUI_API void Ztime_picker_flyout_set_minute_increment(Zid flyout, int increment);
	ZRNUI_API int Ztime_picker_flyout_get_minute_increment(Zid flyout);
	ZRNUI_API void Ztime_picker_flyout_set_clock_identifier(Zid flyout, ZClockIdentifier clock);
	ZRNUI_API ZClockIdentifier Ztime_picker_flyout_get_clock_identifier(Zid flyout);
	/**
	 * @brief TimePicked: Done was tapped; as DatePicked, the picker gets the time, the MinuteIncrement and the clock of the flyout
	 * (TimeChanged). The selectors show the nearest minute on the MinuteIncrement grid, and Done keeps that minute.
	 * The handler may destroy the flyout. One handler; NULL removes it.
	 */
	ZRNUI_API void Ztime_picker_flyout_on_time_picked(Zid flyout, ZTimePickedCallback callback, void* user_data);

	/** @brief PickerFlyoutBase.Title: a string over the selectors, "" takes it away (a picker's flyout takes its Header when it shows). */
	ZRNUI_API void Zpicker_flyout_base_set_title(Zid picker_flyout, const char* title);
	ZRNUI_API const char* Zpicker_flyout_base_get_title(Zid picker_flyout);
	/**
	 * @return A LoopingSelector of the flyout, 0 to 2, left to right: month, day and year of a DatePickerFlyout; hour, minute and
	 * AM / PM of a TimePickerFlyout. Z_INVALID_ID for another index. The Done and Cancel buttons are AppBarButtons.
	 */
	ZRNUI_API Zid Zpicker_flyout_base_get_selector(Zid picker_flyout, int index);
	/** @return The Done and the Cancel button of the flyout (owned by it). */
	ZRNUI_API Zid Zpicker_flyout_base_get_done_button(Zid picker_flyout);
	ZRNUI_API Zid Zpicker_flyout_base_get_cancel_button(Zid picker_flyout);


	/* ============================================================================
	 *  IconElement
	 *
	 *  The icon of an app bar button: a SymbolIcon draws a Symbol as vector shapes, a FontIcon a glyph
	 *  with the text engine, a PathIcon a geometry, a BitmapIcon a PNG. An icon draws in the foreground of
	 *  the control it is in (the white outline of an AppBarButton, the background color while it is
	 *  pressed) unless Zicon_element_set_foreground gives it one. In the circle of an AppBarButton an icon is 20 x 20.
	 *  Vector drawings exist for Accept, Add, Back, Cancel, Clear, Delete, Download, Edit, Favorite, Find,
	 *  Forward, Home, Mail, MailFilled, More, Next, OutlineStar, Pause, Pin, Play, Previous, Refresh, Remove, Save,
	 *  SaveLocal, Send, Setting, SolidStar, Stop, Sync, Up and Upload; any other Symbol is an empty box.
	 *  (The 8.1 Symbol has Find and Setting, no Search and Settings, and has no Share and no Down.)
	 * ============================================================================ */

	/** @brief IconElement.Foreground: a color of its own for the icon, for good (there is no way back to the foreground of the control). */
	ZRNUI_API void Zicon_element_set_foreground(Zid icon, ZColor color);

	/** @brief A SymbolIcon. An icon is not in a tree at first: give it to Zapp_bar_button_create / _set_icon (the button takes it), or Zelement_destroy it. */
	ZRNUI_API Zid Zsymbol_icon_create(ZSymbol symbol);
	ZRNUI_API void Zsymbol_icon_set_symbol(Zid symbol_icon, ZSymbol symbol);
	/** @return The symbol, Z_SYMBOL_PLACEHOLDER for an invalid handle. */
	ZRNUI_API ZSymbol Zsymbol_icon_get_symbol(Zid symbol_icon);

	/** @brief A FontIcon: `glyph` is UTF-8 (NULL for none), drawn in the font of the text engine (Selawik, FontSize 20 by default). */
	ZRNUI_API Zid Zfont_icon_create(const char* glyph);
	/** @brief Glyph; the getter returns "" for none, never NULL. */
	ZRNUI_API void Zfont_icon_set_glyph(Zid font_icon, const char* glyph);
	ZRNUI_API const char* Zfont_icon_get_glyph(Zid font_icon);
	/** @brief FontSize (a value of 0 or less is ignored) and FontFamily of the glyph. */
	ZRNUI_API void Zfont_icon_set_font_size(Zid font_icon, float font_size);
	ZRNUI_API void Zfont_icon_set_font_family(Zid font_icon, ZFontFamily family);

	/**
	 * @brief A PathIcon. `data` is Path.Data in the mini language "M x,y L x,y H x V y Z" (also m, l, h, v, z):
	 * every sub-path is a filled polygon, and the whole fits the icon uniformly. NULL makes an empty one.
	 * @return Z_INVALID_ID when the data is not understood (Z_ERROR_INVALID_ARGUMENT is reported); "" or data without a point is not understood.
	 */
	ZRNUI_API Zid Zpath_icon_create(const char* data);
	/** @return false, the icon unchanged, when `data` is NULL or not understood (as above, Z_ERROR_INVALID_ARGUMENT is reported for the latter). */
	ZRNUI_API bool Zpath_icon_set_data(Zid path_icon, const char* data);

	/**
	 * @brief A BitmapIcon from a PNG file, drawn as it is (it is not tinted with the foreground). NULL makes an empty one.
	 * @return Z_INVALID_ID when the file cannot be read as a PNG (Z_ERROR_IO is reported).
	 */
	ZRNUI_API Zid Zbitmap_icon_create(const char* png_path);
	/** @return false, the icon unchanged, when `png_path` is NULL or cannot be read as a PNG. */
	ZRNUI_API bool Zbitmap_icon_set_uri_source(Zid bitmap_icon, const char* png_path);


	/* ============================================================================
	 *  APPBAR, COMMANDBAR, APPBARBUTTON, APPBARTOGGLEBUTTON, APPBARSEPARATOR
	 *
	 *  A bar a Page shows over its top or its bottom. Closed it is 57.5 px tall (the 54.5 of
	 *  AppBarThemeMinHeight and a 1.5 px border and padding), or 35 px as ClosedDisplayMode Minimal
	 *  (the XAML has no number for it: 35 is this library's); the page gives its content what is left.
	 *  Three dots at the right edge (all of a Minimal bar) open it: a tap, or IsOpen. Open, a bar
	 *  grows over the page (up for a bottom bar, down for a top bar) in 167 ms, the buttons show
	 *  their labels and a CommandBar lists its secondary commands as rows of text; a press outside
	 *  an open bar that is not sticky closes it, and is not given to what is under it; so does Back.
	 *  A secondary command that is tapped closes the bar. The phone template of CommandBar is only
	 *  a stub (the shell draws the real bar): the dots, the size of a Minimal bar, the rows of the secondary
	 *  commands and the separator are this library's own, in the measures of the other 8.1 templates.
	 *  The Compact bar is 57.5 px, not the 68 of other sources.
	 *
	 *  An AppBarButton is a 38.5 px circle with a 2.5 px outline in the foreground, 12 px under the top
	 *  (11 for a toggle button), the icon in it, and its label under it in 9.5 px type, 80 px wide;
	 *  IsCompact (a CommandBar sets it while it is closed) hides the label and makes it 48 wide. Pressed it fills
	 *  with the foreground and the icon takes the background color; a checked AppBarToggleButton looks so too.
	 *  AppBarButton is a Button: Zbutton_on_click and Zbutton_set_flyout work on it, and the AppBarToggleButton
	 *  is a ToggleButton (Ztoggle_button_*). The Icon is the Content of the button.
	 * ============================================================================ */

	ZRNUI_API Zid Zapp_bar_create(void);
	/**
	 * @brief IsOpen. Opened and Closed come when it changes (not when the animation ends), and a handler may destroy the bar.
	 * Only one bar is open per root: opening a bar (one that is in a tree) closes the one that is open (its Closed comes first).
	 */
	ZRNUI_API void Zapp_bar_set_is_open(Zid app_bar, bool is_open);
	ZRNUI_API bool Zapp_bar_get_is_open(Zid app_bar);
	/** @brief IsSticky (default false): true keeps an open bar open through a press outside it (light dismiss) and Back. */
	ZRNUI_API void Zapp_bar_set_is_sticky(Zid app_bar, bool is_sticky);
	ZRNUI_API bool Zapp_bar_get_is_sticky(Zid app_bar);
	/** @brief ClosedDisplayMode: Z_APP_BAR_CLOSED_COMPACT (the default) or Z_APP_BAR_CLOSED_MINIMAL; another value is refused (Z_ERROR_INVALID_ARGUMENT). */
	ZRNUI_API void Zapp_bar_set_closed_display_mode(Zid app_bar, ZAppBarClosedDisplayMode mode);
	ZRNUI_API ZAppBarClosedDisplayMode Zapp_bar_get_closed_display_mode(Zid app_bar);
	/** @brief Opened and Closed: the bar opened or closed (IsOpen changed, by the program, the three dots, light dismiss or Back). One handler each; NULL removes it. */
	ZRNUI_API void Zapp_bar_on_opened(Zid app_bar, ZRoutedCallback callback, void* user_data);
	ZRNUI_API void Zapp_bar_on_closed(Zid app_bar, ZRoutedCallback callback, void* user_data);

	/** @brief A CommandBar (an AppBar, so Zapp_bar_* work on it). Its Content (Zcontent_control_set_content) is at the left, the primary commands at the right. */
	ZRNUI_API Zid Zcommand_bar_create(void);
	/**
	 * @brief PrimaryCommands.Append and SecondaryCommands.Append: an AppBarButton, an AppBarToggleButton or an AppBarSeparator;
	 * the bar takes ownership. A secondary command shows as a row of text when the bar is open; when it is clicked (an
	 * AppBarToggleButton too) the bar closes.
	 * @return the index in that list, -1 on failure: an invalid handle, a command of another class (Z_ERROR_WRONG_CLASS) or
	 * one that has a parent (Z_ERROR_HAS_PARENT).
	 */
	ZRNUI_API int Zcommand_bar_add_primary_command(Zid command_bar, Zid command);
	ZRNUI_API int Zcommand_bar_add_secondary_command(Zid command_bar, Zid command);
	ZRNUI_API int Zcommand_bar_get_primary_command_count(Zid command_bar);
	ZRNUI_API int Zcommand_bar_get_secondary_command_count(Zid command_bar);
	/** @return The command at `index`, Z_INVALID_ID when it is out of range. */
	ZRNUI_API Zid Zcommand_bar_get_primary_command(Zid command_bar, int index);
	ZRNUI_API Zid Zcommand_bar_get_secondary_command(Zid command_bar, int index);
	/** @brief Clear: the commands are destroyed. */
	ZRNUI_API void Zcommand_bar_clear_primary_commands(Zid command_bar);
	ZRNUI_API void Zcommand_bar_clear_secondary_commands(Zid command_bar);

	/**
	 * @brief An AppBarButton with a Label (NULL for none) and an Icon (an IconElement the button takes, or Z_INVALID_ID;
	 * an invalid handle is ignored). Until it is added to a bar the program owns the button (Zelement_destroy).
	 */
	ZRNUI_API Zid Zapp_bar_button_create(const char* label, Zid icon);
	/** @brief An AppBarToggleButton, a ToggleButton (Ztoggle_button_*); the arguments as above. */
	ZRNUI_API Zid Zapp_bar_toggle_button_create(const char* label, Zid icon);
	/**
	 * @brief Label, Icon and IsCompact of an AppBarButton or an AppBarToggleButton (another element is reported as an error, and the
	 * call does nothing). The Icon is Z_INVALID_ID for none; the old icon is destroyed when it is replaced. The label getter returns "" for none, never NULL.
	 */
	ZRNUI_API void Zapp_bar_button_set_label(Zid app_bar_button, const char* label);
	ZRNUI_API const char* Zapp_bar_button_get_label(Zid app_bar_button);
	ZRNUI_API void Zapp_bar_button_set_icon(Zid app_bar_button, Zid icon);
	ZRNUI_API Zid Zapp_bar_button_get_icon(Zid app_bar_button);
	ZRNUI_API void Zapp_bar_button_set_is_compact(Zid app_bar_button, bool is_compact);
	ZRNUI_API bool Zapp_bar_button_get_is_compact(Zid app_bar_button);

	/** @brief An AppBarSeparator, to give to Zcommand_bar_add_primary_command / _add_secondary_command. IsCompact as in an AppBarButton. */
	ZRNUI_API Zid Zapp_bar_separator_create(void);
	ZRNUI_API void Zapp_bar_separator_set_is_compact(Zid separator, bool is_compact);
	ZRNUI_API bool Zapp_bar_separator_get_is_compact(Zid separator);


	/* ============================================================================
	 *  FLYOUTBASE, FLYOUT, MENUFLYOUT, BUTTON.FLYOUT
	 *
	 *  A flyout shows above the page, in a Popup, next to the element it is shown at; a tap outside
	 *  it, Back, an item of a menu or Hide closes it. On the phone it is as wide as the screen. Placement
	 *  Top (the default) puts it above the element and Bottom under it (each flips to the other side
	 *  when it does not fit); Left and Right are Bottom here; Full fills the screen.
	 *  A Flyout is a FlyoutPresenter: the chrome color, at least 43 px tall, its content 25.5 px under its top
	 *  edge. A MenuFlyout is a MenuFlyoutPresenter: the page background with a 1.5 px line in the foreground above and below,
	 *  its items 25.5 px SemiLight, padded 8.5 px above and below, at 19 px from the left.
	 *  Opening comes before the flyout shows, Opened after, Closed when it has gone.
	 *  A flyout is owned by the program (Zelement_destroy) until Zbutton_set_flyout gives it to a button, which
	 *  shows it when it is tapped, after its Click.
	 * ============================================================================ */

	/** @brief Placement (default Z_FLYOUT_PLACEMENT_TOP); takes effect at the next show. Another value is refused (Z_ERROR_INVALID_ARGUMENT). */
	ZRNUI_API void Zflyout_base_set_placement(Zid flyout, ZFlyoutPlacementMode placement);
	ZRNUI_API ZFlyoutPlacementMode Zflyout_base_get_placement(Zid flyout);
	/**
	 * @brief ShowAt: shows the flyout at `placement_target`, which must be in a tree that belongs to a root (otherwise
	 * Z_ERROR_INVALID_ARGUMENT is reported and nothing shows). Opening is raised first, Opened after the flyout shows.
	 * Showing it again at the same target does nothing; at another target it closes (Closed) and shows there.
	 */
	ZRNUI_API void Zflyout_base_show_at(Zid flyout, Zid placement_target);
	/** @brief Hide: closes the flyout (Closed is raised); does nothing when it is not shown. */
	ZRNUI_API void Zflyout_base_hide(Zid flyout);
	/** @brief Whether it shows now (the property of the later flyouts, here for the program to ask). */
	ZRNUI_API bool Zflyout_base_get_is_open(Zid flyout);
	/** @brief Opening (before it shows), Opened (after) and Closed (when it has gone, by a tap outside, Back, an item or Hide). One handler each. Handlers may destroy the flyout. */
	ZRNUI_API void Zflyout_base_on_opening(Zid flyout, ZRoutedCallback callback, void* user_data);
	ZRNUI_API void Zflyout_base_on_opened(Zid flyout, ZRoutedCallback callback, void* user_data);
	ZRNUI_API void Zflyout_base_on_closed(Zid flyout, ZRoutedCallback callback, void* user_data);

	ZRNUI_API Zid Zflyout_create(void);
	/**
	 * @brief Flyout.Content: any element, owned by the flyout; the old content is destroyed. An element that has a parent is
	 * taken out of it. Z_INVALID_ID takes the content away. FlyoutPresenterStyle is not supported.
	 */
	ZRNUI_API void Zflyout_set_content(Zid flyout, Zid content);
	/** @return The content, Z_INVALID_ID for none. */
	ZRNUI_API Zid Zflyout_get_content(Zid flyout);

	ZRNUI_API Zid Zmenu_flyout_create(void);
	/**
	 * @brief Items.Append: a MenuFlyoutItem, a ToggleMenuFlyoutItem or a MenuFlyoutSeparator, owned by the menu.
	 * @return the index, -1 on failure (an invalid handle, another kind of element, or an item that has a parent: Z_ERROR_HAS_PARENT).
	 */
	ZRNUI_API int Zmenu_flyout_add_item(Zid menu_flyout, Zid item);
	ZRNUI_API int Zmenu_flyout_get_item_count(Zid menu_flyout);
	/** @return The item at `index`, Z_INVALID_ID when it is out of range. */
	ZRNUI_API Zid Zmenu_flyout_get_item(Zid menu_flyout, int index);
	/** @brief Items.Clear: the items are destroyed. */
	ZRNUI_API void Zmenu_flyout_clear_items(Zid menu_flyout);

	/**
	 * @brief A MenuFlyoutItem. Click runs on a tap, and then the menu closes. A handler may destroy the item or the menu.
	 * Until it is added to a menu the program owns it (Zelement_destroy). The text getter returns "" for none, never NULL;
	 * set_text, get_text and on_click work on a ToggleMenuFlyoutItem too.
	 */
	ZRNUI_API Zid Zmenu_flyout_item_create(const char* text);
	ZRNUI_API void Zmenu_flyout_item_set_text(Zid menu_flyout_item, const char* text);
	ZRNUI_API const char* Zmenu_flyout_item_get_text(Zid menu_flyout_item);
	ZRNUI_API void Zmenu_flyout_item_on_click(Zid menu_flyout_item, ZRoutedCallback callback, void* user_data);
	/** @brief A ToggleMenuFlyoutItem: a tap flips IsChecked before Click runs; a check is drawn at the right edge when it is on (the template has none). */
	ZRNUI_API Zid Ztoggle_menu_flyout_item_create(const char* text, bool is_checked);
	ZRNUI_API void Ztoggle_menu_flyout_item_set_is_checked(Zid toggle_menu_flyout_item, bool is_checked);
	ZRNUI_API bool Ztoggle_menu_flyout_item_get_is_checked(Zid toggle_menu_flyout_item);
	/** @brief A MenuFlyoutSeparator: a thin line between items. */
	ZRNUI_API Zid Zmenu_flyout_separator_create(void);

	/**
	 * @brief Button.Flyout: the button takes ownership (a Button, also an AppBarButton); a tap shows it at the button, after Click.
	 * A flyout that is attached to a button already is refused (Z_ERROR_HAS_PARENT). The flyout the button had is destroyed
	 * when it is replaced; Z_INVALID_ID takes the flyout away that way.
	 */
	ZRNUI_API void Zbutton_set_flyout(Zid button, Zid flyout);
	/** @return The flyout of the button, Z_INVALID_ID for none. */
	ZRNUI_API Zid Zbutton_get_flyout(Zid button);


	/* ============================================================================
	 *  CONTENTDIALOG
	 *
	 *  A dialog at the top of the screen, as wide as it, in the chrome color: the title (25.5 px SemiBold),
	 *  the content (18.14 px) and under them the buttons, side by side in two halves (the primary one at the left,
	 *  and a button without text is not there). It shows in a modal Popup: what is under it is dimmed
	 *  (ContentDialogDimmingThemeBrush: black 60% in Dark, white 60% in Light) and gets no input, Tab stays in the dialog,
	 *  and Back or Hide closes it with the result None. The program owns the dialog (it is not put in a tree).
	 *
	 *    Zid dialog = Zcontent_dialog_create();
	 *    Zcontent_dialog_set_title(dialog, "Delete?");
	 *    Zcontent_control_set_text(dialog, "The file will be gone.");
	 *    Zcontent_dialog_set_primary_button_text(dialog, "delete");
	 *    Zcontent_dialog_set_secondary_button_text(dialog, "cancel");
	 *    Zcontent_dialog_show(dialog, root, done, NULL);   // done(dialog, Z_CONTENT_DIALOG_RESULT_PRIMARY, NULL)
	 *
	 *  There is no await in C: ShowAsync is Zcontent_dialog_show with a callback that gets the result. Closed
	 *  comes before it. The keyboard focus goes to the primary button and comes back when the dialog closes.
	 *  Not included: Closing (it cannot be cancelled), FullSizeDesired, the buttons' Command, TitleTemplate.
	 * ============================================================================ */

	/** @return A ContentDialog, which the program owns (Zelement_destroy), Z_INVALID_ID on failure. */
	ZRNUI_API Zid Zcontent_dialog_create(void);
	/** @brief Title as a string; "" or NULL takes it away. The Content is Zcontent_control_set_content / set_text. The getter returns "" for none, never NULL. */
	ZRNUI_API void Zcontent_dialog_set_title(Zid content_dialog, const char* title);
	ZRNUI_API const char* Zcontent_dialog_get_title(Zid content_dialog);
	/** @brief PrimaryButtonText and SecondaryButtonText: "" or NULL takes the button away. The getters return "" for none, never NULL. */
	ZRNUI_API void Zcontent_dialog_set_primary_button_text(Zid content_dialog, const char* text);
	ZRNUI_API const char* Zcontent_dialog_get_primary_button_text(Zid content_dialog);
	ZRNUI_API void Zcontent_dialog_set_secondary_button_text(Zid content_dialog, const char* text);
	ZRNUI_API const char* Zcontent_dialog_get_secondary_button_text(Zid content_dialog);
	/** @brief IsPrimaryButtonEnabled and IsSecondaryButtonEnabled (default true): a disabled button does not react. */
	ZRNUI_API void Zcontent_dialog_set_is_primary_button_enabled(Zid content_dialog, bool is_enabled);
	ZRNUI_API bool Zcontent_dialog_get_is_primary_button_enabled(Zid content_dialog);
	ZRNUI_API void Zcontent_dialog_set_is_secondary_button_enabled(Zid content_dialog, bool is_enabled);
	ZRNUI_API bool Zcontent_dialog_get_is_secondary_button_enabled(Zid content_dialog);
	/**
	 * @brief PrimaryButtonClick and SecondaryButtonClick: the button was tapped; they come before Closed, and the dialog closes after
	 * them (a handler cannot cancel that). One handler each; NULL removes it. A handler may destroy the dialog.
	 */
	ZRNUI_API void Zcontent_dialog_on_primary_button_click(Zid content_dialog, ZRoutedCallback callback, void* user_data);
	ZRNUI_API void Zcontent_dialog_on_secondary_button_click(Zid content_dialog, ZRoutedCallback callback, void* user_data);
	/** @brief Opened: raised by Zcontent_dialog_show when the dialog shows. Closed: when it has gone, before the result callback. */
	ZRNUI_API void Zcontent_dialog_on_opened(Zid content_dialog, ZRoutedCallback callback, void* user_data);
	ZRNUI_API void Zcontent_dialog_on_closed(Zid content_dialog, ZRoutedCallback callback, void* user_data);
	/**
	 * @brief ShowAsync: shows the dialog over `root`. `callback` (may be NULL) gets the result once, after Closed, whatever closed
	 * the dialog (primary, secondary, or None for Back and Hide); it may destroy the dialog.
	 * @return false when it already shows, when the dialog is in a tree (Z_ERROR_HAS_PARENT), when `root` is not a root
	 * (Z_ERROR_INVALID_ID) or on failure; the dialog is then not shown and `callback` is not kept.
	 */
	ZRNUI_API bool Zcontent_dialog_show(Zid content_dialog, Zid root, ZContentDialogResultCallback callback, void* user_data);
	/** @brief Hide: closes the dialog with the result None (Closed, then the callback of Zcontent_dialog_show); does nothing when it does not show. */
	ZRNUI_API void Zcontent_dialog_hide(Zid content_dialog);

#ifdef __cplusplus
}
#endif
