/**
 * zrnui
 * Windows Phone 8 Silverlight controls in C.
 *
 * core.h : base types, enums, lifecycle, errors.
 *
 * zrnui follows System.Windows.Controls of the Windows Phone 8 SDK
 * (docs/wp8-reference/10-silverlight-controls.md): the same class
 * hierarchy, the same properties and events, the same Measure / Arrange
 * layout, drawn the way the WP8 templates draw them (Metro, Dark or Light,
 * with an accent color).
 *
 * Conventions:
 *   - every object is an opaque Zid handle (id.h), Z_INVALID_ID on failure; a destroyed or never valid
 *     handle is reported through the error callback and the call does nothing
 *   - functions are Z<class>_<action>: Zbutton_create, Zcontent_control_set_text,
 *     Zrange_base_set_value... A function of a base class works on every class
 *     that derives from it: Zcontrol_set_padding takes a Button, a ListBox
 *     or a TextBox; Zrange_base_set_value takes a Slider, a ProgressBar or a
 *     ScrollBar (the hierarchy of the SDK is in element.h and
 *     controls.h)
 *   - types are ZName, callbacks are ZNameCallback, constants and enum
 *     values are Z_NAME
 *   - yes / no values and flags are C23 bool; counts and indexes are int (-1 usually means failure for a
 *     function that returns an index); colors are ZColor
 *   - strings are UTF-8 and always copied by the library
 *   - lengths are in logical pixels of a 480 x 800 phone screen
 *
 * Threading: nothing is thread-safe. Call everything from one thread.
 */

#pragma once

#include "id.h"

#define ZRNUI_VERSION_MAJOR 2
#define ZRNUI_VERSION_MINOR 0
#define ZRNUI_VERSION_PATCH 0
#define ZRNUI_VERSION_STRING "2.0.0"

#ifdef __cplusplus
 #include <cstdint>
 #include <cstddef>
#else
 #include <stdint.h>
 #include <stddef.h>
#endif

#if defined(_WIN32) && defined(ZRNUI_BUILD_DLL)
	#define ZRNUI_API __declspec(dllexport)
#elif defined(_WIN32) && defined(ZRNUI_USE_DLL)
	#define ZRNUI_API __declspec(dllimport)
#elif defined(__GNUC__) && defined(ZRNUI_BUILD_DLL)
	#define ZRNUI_API __attribute__((visibility("default")))
#else
	#define ZRNUI_API
#endif



/** "Not set" for a length (Width, Height, Canvas.Left...): Double.NaN of Silverlight. */
#define Z_AUTO (-1.0f)
/** No limit for MaxWidth / MaxHeight: Double.PositiveInfinity of Silverlight. */
#define Z_INFINITE (1.0e30f)


/** A color: red, green, blue and alpha, each from 0 to 1. */
typedef struct ZColor {
	float r, g, b, a;
} ZColor;

/** Four edge sizes (left, top, right, bottom): Margin, Padding, BorderThickness. Windows.UI.Xaml.Thickness of 8.1. */
typedef struct ZThickness {
	float left, top, right, bottom;
} ZThickness;

/** The four corner radii of a Border, in logical pixels: Windows.UI.Xaml.CornerRadius of 8.1. */
typedef struct ZCornerRadius {
	float top_left, top_right, bottom_right, bottom_left;
} ZCornerRadius;

/** A width and a height in logical pixels. */
typedef struct ZSize {
	float width, height;
} ZSize;

/** A point in logical pixels. */
typedef struct ZPoint {
	float x, y;
} ZPoint;

/** A rectangle in logical pixels: the origin (x, y) and the size. */
typedef struct ZRect {
	float x, y, width, height;
} ZRect;


/** FrameworkElement.HorizontalAlignment: where an element sits in the slot its parent gives it, when it is narrower than the slot. */
typedef enum ZHorizontalAlignment {
	Z_HALIGN_LEFT = 0x0100,
	Z_HALIGN_CENTER,
	Z_HALIGN_RIGHT,
	/** The default of an element: takes all the space of its slot. */
	Z_HALIGN_STRETCH
} ZHorizontalAlignment;

/** FrameworkElement.VerticalAlignment: where an element sits in the slot its parent gives it, when it is shorter than the slot. Z_VALIGN_STRETCH is the default. */
typedef enum ZVerticalAlignment {
	Z_VALIGN_TOP = 0x0110,
	Z_VALIGN_CENTER,
	Z_VALIGN_BOTTOM,
	Z_VALIGN_STRETCH
} ZVerticalAlignment;

/** UIElement.Visibility. */
typedef enum ZVisibility {
	Z_VISIBLE = 0x0120,
	/** Not drawn and takes no space. Silverlight has no "hidden". */
	Z_COLLAPSED
} ZVisibility;

/** The direction a StackPanel, a WrapPanel or a ScrollBar runs in. */
typedef enum ZOrientation {
	Z_HORIZONTAL = 0x0130,
	Z_VERTICAL
} ZOrientation;

/** TextBlock.TextWrapping: whether a line that is too long breaks into more lines. */
typedef enum ZTextWrapping {
	Z_NO_WRAP = 0x0140,
	Z_WRAP
} ZTextWrapping;

/** TextBlock.TextAlignment: how the lines of a text sit in the width of the block. */
typedef enum ZTextAlignment {
	Z_TEXT_LEFT = 0x0150,
	Z_TEXT_CENTER,
	Z_TEXT_RIGHT
} ZTextAlignment;

/**
 * The text styles of a TextBlock: the styles of the Windows Phone 8.1 theme
 * (HeaderTextBlockStyle, BodyTextBlockStyle...), and Z_TEXT_STYLE_PHONE_* the
 * PhoneText*Style of Silverlight 8.0. Ztext_block_set_style applies one;
 * a property set after it wins.
 */
typedef enum ZTextStyle {
	Z_TEXT_STYLE_NONE = 0x0800,
	/* Windows Phone 8.1 */
	Z_TEXT_STYLE_BASE,
	Z_TEXT_STYLE_BODY,
	Z_TEXT_STYLE_HEADER,
	Z_TEXT_STYLE_SUBHEADER,
	Z_TEXT_STYLE_TITLE,
	Z_TEXT_STYLE_GROUP_HEADER,
	Z_TEXT_STYLE_CONTROL_HEADER,
	Z_TEXT_STYLE_CONTROL_CONTEXTUAL_INFO,
	Z_TEXT_STYLE_FLYOUT_PICKER_TITLE,
	Z_TEXT_STYLE_LIST_VIEW_ITEM,
	Z_TEXT_STYLE_LIST_VIEW_EMPTY_STATIC,
	Z_TEXT_STYLE_LIST_VIEW_ITEM_SUBHEADER,
	Z_TEXT_STYLE_LIST_VIEW_ITEM_CONTENT,
	Z_TEXT_STYLE_MESSAGE_DIALOG_TITLE,
	Z_TEXT_STYLE_MESSAGE_DIALOG_CONTENT,
	Z_TEXT_STYLE_COMBO_BOX_PLACEHOLDER,
	/* Windows Phone 8.0 Silverlight */
	Z_TEXT_STYLE_PHONE_NORMAL,
	Z_TEXT_STYLE_PHONE_SUBTLE,
	Z_TEXT_STYLE_PHONE_TITLE1,
	Z_TEXT_STYLE_PHONE_TITLE2,
	Z_TEXT_STYLE_PHONE_TITLE3,
	Z_TEXT_STYLE_PHONE_EXTRA_LARGE,
	Z_TEXT_STYLE_PHONE_GROUP_HEADER,
	Z_TEXT_STYLE_PHONE_LARGE,
	Z_TEXT_STYLE_PHONE_SMALL,
	Z_TEXT_STYLE_PHONE_CONTRAST,
	Z_TEXT_STYLE_PHONE_ACCENT,
	Z_TEXT_STYLE_PHONE_HUGE,
	Z_TEXT_STYLE_LAST_
} ZTextStyle;

/** TextBlock.TextTrimming: what happens to a line that does not fit and does not wrap. */
typedef enum ZTextTrimming {
	/** The text is cut off at the edge. */
	Z_TRIM_NONE = 0x0160,
	/** The line ends at a word with an ellipsis. */
	Z_TRIM_WORD_ELLIPSIS
} ZTextTrimming;

/** Image.Stretch, Viewbox.Stretch: how the content is fitted to the space it gets. */
typedef enum ZStretch {
	/** Original size. */
	Z_STRETCH_NONE = 0x0170,
	/** Fills the space, the aspect ratio is not kept. */
	Z_STRETCH_FILL,
	/** The largest size that fits, aspect ratio kept. */
	Z_STRETCH_UNIFORM,
	/** The smallest size that covers, aspect ratio kept (cropped). */
	Z_STRETCH_UNIFORM_TO_FILL
} ZStretch;

/** Viewbox.StretchDirection: whether the content may be scaled up, down or both. */
typedef enum ZStretchDirection {
	/** Only larger than the original; never smaller. */
	Z_STRETCH_UP_ONLY = 0x0180,
	/** Only smaller than the original; never larger. */
	Z_STRETCH_DOWN_ONLY,
	Z_STRETCH_BOTH
} ZStretchDirection;

/** ScrollViewer.HorizontalScrollBarVisibility / VerticalScrollBarVisibility. */
typedef enum ZScrollBarVisibility {
	/** No scrolling in that direction: the content is measured to the size of the viewport. */
	Z_SCROLLBAR_DISABLED = 0x0190,
	/** The bar shows when the content is larger than the viewport. The default of a ScrollViewer. */
	Z_SCROLLBAR_AUTO,
	/** The content scrolls, the bar is never shown. */
	Z_SCROLLBAR_HIDDEN,
	/** The bar is always shown. */
	Z_SCROLLBAR_VISIBLE
} ZScrollBarVisibility;

/** ButtonBase.ClickMode: when a button raises Click. */
typedef enum ZClickMode {
	/** When the press ends on the button. The default. */
	Z_CLICK_RELEASE = 0x01A0,
	/** As soon as the pointer goes down on the button. */
	Z_CLICK_PRESS,
	/** When the pointer moves over the button. */
	Z_CLICK_HOVER
} ZClickMode;

/** ListBox.SelectionMode: Single selects one item, Multiple toggles items, Extended selects ranges. */
typedef enum ZSelectionMode {
	Z_SELECTION_SINGLE = 0x01B0,
	Z_SELECTION_MULTIPLE,
	Z_SELECTION_EXTENDED
} ZSelectionMode;

/** Grid.ColumnDefinition / RowDefinition: how a column or a row gets its size. */
typedef enum ZGridUnitType {
	/** As large as its content needs. */
	Z_GRID_AUTO = 0x01C0,
	/** A fixed size in logical pixels. */
	Z_GRID_PIXEL,
	/** A share of the space left, in proportion to the value (the 2 of "2*"). */
	Z_GRID_STAR
} ZGridUnitType;

/** ScrollEventType of a ScrollBar's Scroll event: what the user did to move the value. */
typedef enum ZScrollEventType {
	Z_SCROLL_SMALL_DECREMENT = 0x01D0,
	Z_SCROLL_SMALL_INCREMENT,
	Z_SCROLL_LARGE_DECREMENT,
	Z_SCROLL_LARGE_INCREMENT,
	Z_SCROLL_THUMB_POSITION,
	Z_SCROLL_THUMB_TRACK,
	Z_SCROLL_FIRST,
	Z_SCROLL_LAST,
	Z_SCROLL_END
} ZScrollEventType;

/** Zpopup_set_placement_target: where a popup sits against its target element. Bottom and Top flip to the other side when the popup would leave the screen. */
typedef enum ZPlacementMode {
	Z_PLACE_BOTTOM = 0x01E0,
	Z_PLACE_TOP,
	Z_PLACE_LEFT,
	Z_PLACE_RIGHT,
	/** Not tracked: laid out like Z_PLACE_BOTTOM. */
	Z_PLACE_MOUSE
} ZPlacementMode;

/** ListViewBase.SelectionMode (Windows.UI.Xaml.Controls.ListViewSelectionMode). ListBox has the three-valued ZSelectionMode above. */
typedef enum ZListViewSelectionMode {
	Z_LIST_VIEW_SELECTION_NONE = 0x0280,
	Z_LIST_VIEW_SELECTION_SINGLE,
	Z_LIST_VIEW_SELECTION_MULTIPLE,
	Z_LIST_VIEW_SELECTION_EXTENDED
} ZListViewSelectionMode;

/** Why the text of an AutoSuggestBox changed (AutoSuggestionBoxTextChangeReason). */
typedef enum ZAutoSuggestionBoxTextChangeReason {
	Z_SUGGEST_USER_INPUT = 0x0290,
	Z_SUGGEST_PROGRAMMATIC_CHANGE,
	Z_SUGGEST_SUGGESTION_CHOSEN
} ZAutoSuggestionBoxTextChangeReason;

/** ListViewBase.ScrollIntoView: where the item ends up. Default only brings it into view, Leading puts it at the start of the viewport. */
typedef enum ZScrollIntoViewAlignment {
	Z_SCROLL_INTO_VIEW_DEFAULT = 0x02A0,
	Z_SCROLL_INTO_VIEW_LEADING
} ZScrollIntoViewAlignment;

/** Slider.TickPlacement: where the tick marks are drawn (with a TickFrequency above 0). */
typedef enum ZTickPlacement {
	Z_TICK_NONE = 0x0260,
	Z_TICK_TOP_LEFT,
	Z_TICK_BOTTOM_RIGHT,
	Z_TICK_OUTSIDE,       /* both sides of the track: the default */
	Z_TICK_INLINE         /* over the track */
} ZTickPlacement;

/** Slider.SnapsTo: what the value of a touch or a key lands on. */
typedef enum ZSliderSnapsTo {
	Z_SNAPS_TO_STEP_VALUES = 0x0270,
	Z_SNAPS_TO_TICKS
} ZSliderSnapsTo;

/** Frame.Navigate, GoBack, GoForward: how the page was reached (Windows.UI.Xaml.Navigation.NavigationMode). */
typedef enum ZNavigationMode {
	Z_NAVIGATION_NEW = 0x02B0,
	Z_NAVIGATION_BACK,
	Z_NAVIGATION_FORWARD,
	Z_NAVIGATION_REFRESH
} ZNavigationMode;

/** Page.NavigationCacheMode: whether a page is kept alive when the frame leaves it (Windows.UI.Xaml.Navigation.NavigationCacheMode). */
typedef enum ZNavigationCacheMode {
	Z_NAVIGATION_CACHE_DISABLED = 0x02C0,
	Z_NAVIGATION_CACHE_REQUIRED,
	Z_NAVIGATION_CACHE_ENABLED
} ZNavigationCacheMode;

/** AppBar.ClosedDisplayMode (Windows.UI.Xaml.Controls.AppBarClosedDisplayMode: the 8.1 enum has these two values only). */
typedef enum ZAppBarClosedDisplayMode {
	Z_APP_BAR_CLOSED_COMPACT = 0x02D0,
	Z_APP_BAR_CLOSED_MINIMAL
} ZAppBarClosedDisplayMode;

/** FlyoutBase.Placement (Windows.UI.Xaml.Controls.Primitives.FlyoutPlacementMode). */
typedef enum ZFlyoutPlacementMode {
	Z_FLYOUT_PLACEMENT_TOP = 0x02E0,
	Z_FLYOUT_PLACEMENT_BOTTOM,
	Z_FLYOUT_PLACEMENT_LEFT,
	Z_FLYOUT_PLACEMENT_RIGHT,
	Z_FLYOUT_PLACEMENT_FULL
} ZFlyoutPlacementMode;

/** What closed a ContentDialog (Windows.UI.Xaml.Controls.ContentDialogResult). */
typedef enum ZContentDialogResult {
	Z_CONTENT_DIALOG_RESULT_NONE = 0x02F0,
	Z_CONTENT_DIALOG_RESULT_PRIMARY,
	Z_CONTENT_DIALOG_RESULT_SECONDARY
} ZContentDialogResult;

/** SymbolIcon.Symbol (Windows.UI.Xaml.Controls.Symbol): the values are the code points of the 8.1 symbol font. */
typedef enum ZSymbol {
	Z_SYMBOL_PREVIOUS = 57600,
	Z_SYMBOL_NEXT = 57601,
	Z_SYMBOL_PLAY = 57602,
	Z_SYMBOL_PAUSE = 57603,
	Z_SYMBOL_EDIT = 57604,
	Z_SYMBOL_SAVE = 57605,
	Z_SYMBOL_CLEAR = 57606,
	Z_SYMBOL_DELETE = 57607,
	Z_SYMBOL_REMOVE = 57608,
	Z_SYMBOL_ADD = 57609,
	Z_SYMBOL_CANCEL = 57610,
	Z_SYMBOL_ACCEPT = 57611,
	Z_SYMBOL_MORE = 57612,
	Z_SYMBOL_REDO = 57613,
	Z_SYMBOL_UNDO = 57614,
	Z_SYMBOL_HOME = 57615,
	Z_SYMBOL_UP = 57616,
	Z_SYMBOL_FORWARD = 57617,
	Z_SYMBOL_BACK = 57618,
	Z_SYMBOL_FAVORITE = 57619,
	Z_SYMBOL_CAMERA = 57620,
	Z_SYMBOL_SETTING = 57621,
	Z_SYMBOL_VIDEO = 57622,
	Z_SYMBOL_SYNC = 57623,
	Z_SYMBOL_DOWNLOAD = 57624,
	Z_SYMBOL_MAIL = 57625,
	Z_SYMBOL_FIND = 57626,
	Z_SYMBOL_HELP = 57627,
	Z_SYMBOL_UPLOAD = 57628,
	Z_SYMBOL_EMOJI = 57629,
	Z_SYMBOL_TWO_PAGE = 57630,
	Z_SYMBOL_LEAVE_CHAT = 57631,
	Z_SYMBOL_MAIL_FORWARD = 57632,
	Z_SYMBOL_CLOCK = 57633,
	Z_SYMBOL_SEND = 57634,
	Z_SYMBOL_CROP = 57635,
	Z_SYMBOL_ROTATE_CAMERA = 57636,
	Z_SYMBOL_PEOPLE = 57637,
	Z_SYMBOL_OPEN_PANE = 57638,
	Z_SYMBOL_CLOSE_PANE = 57639,
	Z_SYMBOL_WORLD = 57640,
	Z_SYMBOL_FLAG = 57641,
	Z_SYMBOL_PREVIEW_LINK = 57642,
	Z_SYMBOL_GLOBE = 57643,
	Z_SYMBOL_TRIM = 57644,
	Z_SYMBOL_ATTACH_CAMERA = 57645,
	Z_SYMBOL_ZOOM_IN = 57646,
	Z_SYMBOL_BOOKMARKS = 57647,
	Z_SYMBOL_DOCUMENT = 57648,
	Z_SYMBOL_PROTECTED_DOCUMENT = 57649,
	Z_SYMBOL_PAGE = 57650,
	Z_SYMBOL_BULLETS = 57651,
	Z_SYMBOL_COMMENT = 57652,
	Z_SYMBOL_MAIL_FILLED = 57653,
	Z_SYMBOL_CONTACT_INFO = 57654,
	Z_SYMBOL_HANG_UP = 57655,
	Z_SYMBOL_VIEW_ALL = 57656,
	Z_SYMBOL_MAP_PIN = 57657,
	Z_SYMBOL_PHONE = 57658,
	Z_SYMBOL_VIDEO_CHAT = 57659,
	Z_SYMBOL_SWITCH = 57660,
	Z_SYMBOL_CONTACT = 57661,
	Z_SYMBOL_RENAME = 57662,
	Z_SYMBOL_PIN = 57665,
	Z_SYMBOL_MUSIC_INFO = 57666,
	Z_SYMBOL_GO = 57667,
	Z_SYMBOL_KEYBOARD = 57668,
	Z_SYMBOL_DOCK_LEFT = 57669,
	Z_SYMBOL_DOCK_RIGHT = 57670,
	Z_SYMBOL_DOCK_BOTTOM = 57671,
	Z_SYMBOL_REMOTE = 57672,
	Z_SYMBOL_REFRESH = 57673,
	Z_SYMBOL_ROTATE = 57674,
	Z_SYMBOL_SHUFFLE = 57675,
	Z_SYMBOL_LIST = 57676,
	Z_SYMBOL_SHOP = 57677,
	Z_SYMBOL_SELECT_ALL = 57678,
	Z_SYMBOL_ORIENTATION = 57679,
	Z_SYMBOL_IMPORT = 57680,
	Z_SYMBOL_IMPORT_ALL = 57681,
	Z_SYMBOL_BROWSE_PHOTOS = 57685,
	Z_SYMBOL_WEB_CAM = 57686,
	Z_SYMBOL_PICTURES = 57688,
	Z_SYMBOL_SAVE_LOCAL = 57689,
	Z_SYMBOL_CAPTION = 57690,
	Z_SYMBOL_STOP = 57691,
	Z_SYMBOL_SHOW_RESULTS = 57692,
	Z_SYMBOL_VOLUME = 57693,
	Z_SYMBOL_REPAIR = 57694,
	Z_SYMBOL_MESSAGE = 57695,
	Z_SYMBOL_PAGE2 = 57696,
	Z_SYMBOL_CALENDAR_DAY = 57697,
	Z_SYMBOL_CALENDAR_WEEK = 57698,
	Z_SYMBOL_CALENDAR = 57699,
	Z_SYMBOL_CHARACTER = 57700,
	Z_SYMBOL_MAIL_REPLY_ALL = 57701,
	Z_SYMBOL_READ = 57702,
	Z_SYMBOL_LINK = 57703,
	Z_SYMBOL_ACCOUNT = 57704,
	Z_SYMBOL_SHOW_BCC = 57705,
	Z_SYMBOL_HIDE_BCC = 57706,
	Z_SYMBOL_CUT = 57707,
	Z_SYMBOL_ATTACH = 57708,
	Z_SYMBOL_PASTE = 57709,
	Z_SYMBOL_FILTER = 57710,
	Z_SYMBOL_COPY = 57711,
	Z_SYMBOL_EMOJI2 = 57712,
	Z_SYMBOL_IMPORTANT = 57713,
	Z_SYMBOL_MAIL_REPLY = 57714,
	Z_SYMBOL_SLIDE_SHOW = 57715,
	Z_SYMBOL_SORT = 57716,
	Z_SYMBOL_MANAGE = 57720,
	Z_SYMBOL_ALL_APPS = 57721,
	Z_SYMBOL_DISCONNECT_DRIVE = 57722,
	Z_SYMBOL_MAP_DRIVE = 57723,
	Z_SYMBOL_NEW_WINDOW = 57724,
	Z_SYMBOL_OPEN_WITH = 57725,
	Z_SYMBOL_CONTACT_PRESENCE = 57729,
	Z_SYMBOL_PRIORITY = 57730,
	Z_SYMBOL_GO_TO_TODAY = 57732,
	Z_SYMBOL_FONT = 57733,
	Z_SYMBOL_FONT_COLOR = 57734,
	Z_SYMBOL_CONTACT2 = 57735,
	Z_SYMBOL_FOLDER = 57736,
	Z_SYMBOL_AUDIO = 57737,
	Z_SYMBOL_PLACEHOLDER = 57738,
	Z_SYMBOL_VIEW = 57739,
	Z_SYMBOL_SET_LOCK_SCREEN = 57740,
	Z_SYMBOL_SET_TILE = 57741,
	Z_SYMBOL_CLOSED_CAPTION = 57744,
	Z_SYMBOL_STOP_SLIDE_SHOW = 57745,
	Z_SYMBOL_PERMISSIONS = 57746,
	Z_SYMBOL_HIGHLIGHT = 57747,
	Z_SYMBOL_DISABLE_UPDATES = 57748,
	Z_SYMBOL_UN_FAVORITE = 57749,
	Z_SYMBOL_UN_PIN = 57750,
	Z_SYMBOL_OPEN_LOCAL = 57751,
	Z_SYMBOL_MUTE = 57752,
	Z_SYMBOL_ITALIC = 57753,
	Z_SYMBOL_UNDERLINE = 57754,
	Z_SYMBOL_BOLD = 57755,
	Z_SYMBOL_MOVE_TO_FOLDER = 57756,
	Z_SYMBOL_LIKE_DISLIKE = 57757,
	Z_SYMBOL_DISLIKE = 57758,
	Z_SYMBOL_LIKE = 57759,
	Z_SYMBOL_ALIGN_RIGHT = 57760,
	Z_SYMBOL_ALIGN_CENTER = 57761,
	Z_SYMBOL_ALIGN_LEFT = 57762,
	Z_SYMBOL_ZOOM = 57763,
	Z_SYMBOL_ZOOM_OUT = 57764,
	Z_SYMBOL_OPEN_FILE = 57765,
	Z_SYMBOL_OTHER_USER = 57766,
	Z_SYMBOL_ADMIN = 57767,
	Z_SYMBOL_STREET = 57795,
	Z_SYMBOL_MAP = 57796,
	Z_SYMBOL_CLEAR_SELECTION = 57797,
	Z_SYMBOL_FONT_DECREASE = 57798,
	Z_SYMBOL_FONT_INCREASE = 57799,
	Z_SYMBOL_FONT_SIZE = 57800,
	Z_SYMBOL_CELL_PHONE = 57801,
	Z_SYMBOL_RE_SHARE = 57802,
	Z_SYMBOL_TAG = 57803,
	Z_SYMBOL_REPEAT_ONE = 57804,
	Z_SYMBOL_REPEAT_ALL = 57805,
	Z_SYMBOL_OUTLINE_STAR = 57806,
	Z_SYMBOL_SOLID_STAR = 57807,
	Z_SYMBOL_CALCULATOR = 57808,
	Z_SYMBOL_DIRECTIONS = 57809,
	Z_SYMBOL_TARGET = 57810,
	Z_SYMBOL_LIBRARY = 57811,
	Z_SYMBOL_PHONE_BOOK = 57812,
	Z_SYMBOL_MEMO = 57813,
	Z_SYMBOL_MICROPHONE = 57814,
	Z_SYMBOL_POST_UPDATE = 57815,
	Z_SYMBOL_BACK_TO_WINDOW = 57816,
	Z_SYMBOL_FULL_SCREEN = 57817,
	Z_SYMBOL_NEW_FOLDER = 57818,
	Z_SYMBOL_CALENDAR_REPLY = 57819,
	Z_SYMBOL_UN_SYNC_FOLDER = 57821,
	Z_SYMBOL_REPORT_HACKED = 57822,
	Z_SYMBOL_SYNC_FOLDER = 57823,
	Z_SYMBOL_BLOCK_CONTACT = 57824,
	Z_SYMBOL_SWITCH_APPS = 57825,
	Z_SYMBOL_ADD_FRIEND = 57826,
	Z_SYMBOL_TOUCH_POINTER = 57827,
	Z_SYMBOL_GO_TO_START = 57828,
	Z_SYMBOL_ZERO_BARS = 57829,
	Z_SYMBOL_ONE_BAR = 57830,
	Z_SYMBOL_TWO_BARS = 57831,
	Z_SYMBOL_THREE_BARS = 57832,
	Z_SYMBOL_FOUR_BARS = 57833,
	Z_SYMBOL_SCAN = 58004,
	Z_SYMBOL_PREVIEW = 58005
} ZSymbol;

/** The tri-state of ToggleButton.IsChecked. */
typedef enum ZToggleState {
	Z_UNCHECKED = 0x0200,
	Z_CHECKED,
	Z_INDETERMINATE
} ZToggleState;

/** The WP8 font families (themes.md): "Segoe WP" and its weights. Selawik stands in for them (Zset_font_directory). */
typedef enum ZFontFamily {
	Z_FONT_NORMAL = 0x0210,
	Z_FONT_LIGHT,
	Z_FONT_SEMILIGHT,
	Z_FONT_SEMIBOLD,
	Z_FONT_BOLD
} ZFontFamily;

/** The base theme of a root (Zroot_set_theme): Dark is white on black, Light is black on white. */
typedef enum ZTheme {
	Z_THEME_DARK = 0x0220,
	Z_THEME_LIGHT
} ZTheme;

/** The 20 accent colors of Windows Phone 8. */
typedef enum ZAccent {
	Z_ACCENT_LIME = 0x0230,
	Z_ACCENT_GREEN,
	Z_ACCENT_EMERALD,
	Z_ACCENT_TEAL,
	Z_ACCENT_CYAN,
	Z_ACCENT_COBALT,
	Z_ACCENT_INDIGO,
	Z_ACCENT_VIOLET,
	Z_ACCENT_PINK,
	Z_ACCENT_MAGENTA,
	Z_ACCENT_CRIMSON,
	Z_ACCENT_RED,
	Z_ACCENT_ORANGE,
	Z_ACCENT_AMBER,
	Z_ACCENT_YELLOW,
	Z_ACCENT_BROWN,
	Z_ACCENT_OLIVE,
	Z_ACCENT_STEEL,
	Z_ACCENT_MAUVE,
	Z_ACCENT_TAUPE
} ZAccent;

/**
 * Z_DEBUG is defined for a debug build (CMake defines it for CMAKE_BUILD_TYPE=Debug or -DZRNUI_DEBUG=ON, on
 * the zrnui target and so on every target that links it). All the debug features of the library and of the
 * programs that use it are compiled under it; without it they are not in the binary.
 * Z_DEBUG_ONLY(code) is the code in a debug build and nothing otherwise; Z_DEBUG_LOG(...) prints a line to stderr.
 */
#ifdef Z_DEBUG
	#include <stdio.h>
	#define Z_DEBUG_ONLY(code) code
	#define Z_DEBUG_LOG(...) (void)fprintf(stderr, "zrnui: " __VA_ARGS__)
#else
	#define Z_DEBUG_ONLY(code)
	#define Z_DEBUG_LOG(...) ((void)0)
#endif

/** Whether the animations of a root run (Zroot_set_animations, ZConfig.animations). */
typedef enum ZAnimations {
	/** On with the OpenGL renderer, off with the software one. The default. */
	Z_ANIMATIONS_AUTO = 0,
	/** Every change is at once. */
	Z_ANIMATIONS_OFF,
	/** Animations run whatever the renderer. */
	Z_ANIMATIONS_ON
} ZAnimations;

/** Which painter draws a root (Zroot_set_renderer, ZConfig.renderer). */
typedef enum ZRenderer {
	/** The platform uses OpenGL ES 2 when it can, Zroot_render the software painter. The default. */
	Z_RENDERER_AUTO = 0,
	/** The software painter only; the GPU is never touched. */
	Z_RENDERER_SOFTWARE,
	/** OpenGL ES 2 only: failing when there is no context. */
	Z_RENDERER_GPU
} ZRenderer;

/**
 * Every class of the library. Zelement_get_class returns the class of an
 * object; Zelement_is_a tells whether it is, or derives from, a class.
 */
typedef enum ZClass {
	Z_CLASS_ELEMENT = 0x0300,          /* UIElement + FrameworkElement */
	Z_CLASS_PANEL,                     /* abstract */
	Z_CLASS_STACK_PANEL,
	Z_CLASS_VIRTUALIZING_STACK_PANEL,
	Z_CLASS_WRAP_PANEL,
	Z_CLASS_GRID,
	Z_CLASS_CANVAS,
	Z_CLASS_BORDER,
	Z_CLASS_VIEWBOX,
	Z_CLASS_TEXT_BLOCK,
	Z_CLASS_IMAGE,
	Z_CLASS_POPUP,
	Z_CLASS_CONTROL,                   /* abstract */
	Z_CLASS_USER_CONTROL,
	Z_CLASS_CONTENT_CONTROL,
	Z_CLASS_TOOLTIP,
	Z_CLASS_BUTTON_BASE,               /* abstract */
	Z_CLASS_BUTTON,
	Z_CLASS_HYPERLINK_BUTTON,
	Z_CLASS_REPEAT_BUTTON,
	Z_CLASS_TOGGLE_BUTTON,
	Z_CLASS_CHECKBOX,
	Z_CLASS_RADIO_BUTTON,
	Z_CLASS_RANGE_BASE,                /* abstract */
	Z_CLASS_SLIDER,
	Z_CLASS_PROGRESS_BAR,
	Z_CLASS_SCROLLBAR,
	Z_CLASS_THUMB,
	Z_CLASS_SEPARATOR,
	Z_CLASS_SCROLL_VIEWER,
	Z_CLASS_TEXT_BOX,
	Z_CLASS_PASSWORD_BOX,
	Z_CLASS_ITEMS_CONTROL,
	Z_CLASS_SELECTOR,                  /* abstract */
	Z_CLASS_SELECTOR_ITEM,             /* abstract: IsSelected */
	Z_CLASS_LIST_BOX,
	Z_CLASS_LIST_BOX_ITEM,
	Z_CLASS_COMBO_BOX,
	Z_CLASS_COMBO_BOX_ITEM,
	Z_CLASS_AUTO_SUGGEST_BOX,
	Z_CLASS_FRAME,                     /* a ContentControl that navigates between pages */
	Z_CLASS_PAGE,                      /* a UserControl a Frame shows */
	Z_CLASS_PIVOT,                     /* headers over pages that slide */
	Z_CLASS_TOGGLE_SWITCH,             /* an on/off switch with a header */
	Z_CLASS_PROGRESS_RING,             /* the dots that run round while something loads */
	Z_CLASS_LIST_VIEW_BASE,            /* abstract: SelectionMode, ItemClick, Header, Footer */
	Z_CLASS_LIST_VIEW,
	Z_CLASS_GRID_VIEW,
	Z_CLASS_LIST_VIEW_ITEM,
	Z_CLASS_GRID_VIEW_ITEM,
	Z_CLASS_LIST_VIEW_BASE_HEADER_ITEM, /* abstract: the header of a group */
	Z_CLASS_LIST_VIEW_HEADER_ITEM,
	Z_CLASS_GRID_VIEW_HEADER_ITEM,
	Z_CLASS_FLIP_VIEW,
	Z_CLASS_FLIP_VIEW_ITEM,
	Z_CLASS_ITEMS_STACK_PANEL,
	Z_CLASS_ITEMS_WRAP_GRID,
	Z_CLASS_SEMANTIC_ZOOM,
	Z_CLASS_ICON_ELEMENT,              /* abstract: the icon of an app bar button */
	Z_CLASS_SYMBOL_ICON,
	Z_CLASS_FONT_ICON,
	Z_CLASS_PATH_ICON,
	Z_CLASS_BITMAP_ICON,
	Z_CLASS_APP_BAR,                   /* a ContentControl the page shows at its top or bottom: IsOpen, IsSticky */
	Z_CLASS_COMMAND_BAR,
	Z_CLASS_APP_BAR_BUTTON,
	Z_CLASS_APP_BAR_TOGGLE_BUTTON,
	Z_CLASS_APP_BAR_SEPARATOR,
	Z_CLASS_FLYOUT_BASE,               /* abstract: a thing that shows above the page next to an element */
	Z_CLASS_FLYOUT,
	Z_CLASS_MENU_FLYOUT,
	Z_CLASS_MENU_FLYOUT_ITEM_BASE,     /* abstract */
	Z_CLASS_MENU_FLYOUT_ITEM,
	Z_CLASS_TOGGLE_MENU_FLYOUT_ITEM,
	Z_CLASS_MENU_FLYOUT_SEPARATOR,
	Z_CLASS_CONTENT_DIALOG,
	Z_CLASS_PIVOT_ITEM,                /* a ContentControl with a Header: the page of a Pivot */
	Z_CLASS_HUB,                       /* sections side by side in a scroller, over a big header */
	Z_CLASS_HUB_SECTION,               /* a section of a Hub: a header over its content */
	Z_CLASS_LOOPING_SELECTOR,          /* a column of items that goes round, with the chosen one in the middle */
	Z_CLASS_DATE_PICKER,               /* a header over a button with the date; a tap opens the DatePickerFlyout */
	Z_CLASS_TIME_PICKER,
	Z_CLASS_PICKER_FLYOUT_BASE,        /* abstract: a full screen flyout with Done and Cancel */
	Z_CLASS_DATE_PICKER_FLYOUT,
	Z_CLASS_TIME_PICKER_FLYOUT,
	Z_CLASS_RICH_TEXT_BLOCK,           /* paragraphs of plain text */
	Z_CLASS_ROOT,                      /* the host of a visual tree, host.h */
	Z_CLASS_LAST_
} ZClass;

/** What went wrong, as Zget_error and the error callback report it. */
typedef enum ZError {
	/** No error has been reported. */
	Z_OK = 0,
	/** The handle is invalid, destroyed or of no object. */
	Z_ERROR_INVALID_ID = 1,
	/** The handle is valid but its class does not have that function. */
	Z_ERROR_WRONG_CLASS,
	/** A value is out of range, a key unknown, a pointer NULL. */
	Z_ERROR_INVALID_ARGUMENT,
	Z_ERROR_OUT_OF_MEMORY,
	/** A font, a file, a window or a GPU context could not be had. */
	Z_ERROR_IO,
	/** An element would be added under itself. */
	Z_ERROR_CYCLE,
	/** An element or a command that already has a parent (or a flyout already attached) was given another. */
	Z_ERROR_HAS_PARENT
} ZError;


#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * @brief Starts the library with the environment ($ZRNUI_*, see Zconfig_from_environment): what an application
	 * wants. A program that embeds zrnui (a shell) passes its own ZConfig to Zinit_ex and the environment is never read.
	 * Reference counted: every call needs a Zshutdown, and only the first one starts the library (the configuration of
	 * the first call stays). Zroot_create calls it, so a program that only creates roots need not.
	 * @return false when the fonts cannot be loaded (see Zget_error); true otherwise.
	 */
	ZRNUI_API bool Zinit(void);

	/**
	 * @brief Gives back one Zinit. The call that balances the first one destroys every root and every element
	 * (no handle is valid afterwards) and frees the fonts. Does nothing when no Zinit is open.
	 */
	ZRNUI_API void Zshutdown(void);

	/** @brief The version of the library as "major.minor.patch" (ZRNUI_VERSION_STRING): a static string. */
	ZRNUI_API const char* Zget_version(void);

	/** @brief The last error reported on the calling thread; Z_OK when none has been. A call that works does not reset it. */
	ZRNUI_API ZError Zget_error(void);
	/** @brief The text of the last error ("" before the first). Valid until the next error. */
	ZRNUI_API const char* Zget_error_message(void);

	/**
	 * @brief Called when a function fails (invalid handle, wrong class...).
	 * By default (or with a NULL callback) the message is printed to stderr. The callback runs inside the
	 * failing call, before it returns; one callback serves the whole library.
	 */
	typedef void (*ZErrorCallback)(ZError error, const char* message, void* user_data);
	ZRNUI_API void Zset_error_callback(ZErrorCallback callback, void* user_data);

	/**
	 * @brief Directory of the Selawik fonts (Selawik-Regular.ttf,
	 * Selawik-Semilight.ttf, Selawik-Semibold.ttf), the free fonts that
	 * metric-match Segoe UI and stand in for Segoe WP. Call before Zinit.
	 * Default: ZConfig.font_directory (Zinit_ex; $ZRNUI_FONT_DIR with Zinit), then the directory compiled in.
	 */
	ZRNUI_API void Zset_font_directory(const char* path);

	/* The explicit configuration of the library: ZConfig, Zinit_ex (host.h). */

	/** @brief A color from its four components, each from 0 to 1. */
	ZRNUI_API ZColor Zcolor_rgba(float r, float g, float b, float a);
	/** @brief 0xAARRGGBB, the notation of the WP8 theme files. */
	ZRNUI_API ZColor Zcolor_argb(uint32_t argb);
	/** @brief A thickness from its four edges. */
	ZRNUI_API ZThickness Zthickness(float left, float top, float right, float bottom);
	/** @brief A thickness with the same size on all four edges. */
	ZRNUI_API ZThickness Zthickness_uniform(float all);

#ifdef __cplusplus
}
#endif

