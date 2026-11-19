/**
 * zrnui - theme.h
 * The colors and sizes of a theme (theme.c).
 */

#pragma once

#include "zrnui/element.h"

typedef struct zrn_element zrn_element_t;

/** The colors of a theme; the values are in theme.c (theme.ini can change them). ZRN_TC_ACCENT is the accent of the root. */
typedef enum zrn_theme_color_id {
	ZRN_TC_FOREGROUND,
	ZRN_TC_BACKGROUND,
	ZRN_TC_CONTRAST_FOREGROUND,
	ZRN_TC_CONTRAST_BACKGROUND,
	ZRN_TC_DISABLED,
	ZRN_TC_PROGRESS_BAR_BACKGROUND,
	ZRN_TC_TEXT_BOX,
	ZRN_TC_TEXT_BOX_FOREGROUND,
	ZRN_TC_TEXT_BOX_EDIT_BACKGROUND,
	ZRN_TC_TEXT_BOX_READ_ONLY,
	ZRN_TC_TEXT_CARET,
	ZRN_TC_SUBTLE,
	ZRN_TC_BUTTON_PRESSED_FOREGROUND,
	ZRN_TC_CHROME,
	ZRN_TC_INACTIVE,
	ZRN_TC_BORDER,
	ZRN_TC_SEMITRANSPARENT,
	ZRN_TC_ACCENT,
	/* Windows Phone 8.1 (themeresources.xaml) */
	ZRN_TC_BASE_MEDIUM_HIGH,
	ZRN_TC_BASE_MID,
	ZRN_TC_BASE_MEDIUM_MID,
	ZRN_TC_BASE_MEDIUM_MID_SOLID,
	ZRN_TC_BASE_LOW,
	ZRN_TC_BASE_LOW_SOLID,
	ZRN_TC_TEXT_OVER_ACCENT,
	ZRN_TC_TEXT_BOX_BACKGROUND_81,
	ZRN_TC_TEXT_BOX_BORDER_81,
	ZRN_TC_TEXT_BOX_FOREGROUND_81,
	ZRN_TC_TEXT_BOX_PLACEHOLDER_81,
	ZRN_TC_TEXT_BOX_FOCUSED_BACKGROUND_81,
	ZRN_TC_SLIDER_THUMB,
	ZRN_TC_SLIDER_THUMB_DISABLED,
	ZRN_TC_SLIDER_TRACK,
	ZRN_TC_SLIDER_DECREASE_DISABLED,
	/* PhoneBaseBlackColor, PhoneBaseWhiteColor, PhoneDisabledColor, PhoneTextMidColor, PhoneTextLowColor */
	ZRN_TC_BASE_BLACK,
	ZRN_TC_BASE_WHITE,
	ZRN_TC_PHONE_DISABLED,
	ZRN_TC_TEXT_MID,
	ZRN_TC_TEXT_LOW,
	/* ContentDialogDimmingColor */
	ZRN_TC_CONTENT_DIALOG_DIMMING,
	/* the text of a ListBox: PhoneBaseBlack on the white 80% box of Dark; the 8.1 Light dictionary has PhoneBaseBlack on a black 75% box, unreadable, so Light uses PhoneBaseWhite */
	ZRN_TC_LIST_BOX_FOREGROUND,
	ZRN_TC_COUNT
} zrn_theme_color_id_t;

typedef struct zrn_theme {
	/** Dark or Light. */
	ZTheme theme;
	ZColor colors[ZRN_TC_COUNT];
	ZColor accent;
	/** The color the window is cleared with: ZRN_TC_BACKGROUND unless b_has_background (Zroot_set_background). */
	ZColor background;
	bool b_has_background;
} zrn_theme_t;

/** Fills `t` for `theme` from the tables (theme.ini applied); ZRN_TC_ACCENT becomes `accent`. */
void zrn_theme_init(zrn_theme_t* t, ZTheme theme, ZColor accent);
/** The theme of the root an element is in, or the default theme (Dark, Cobalt) for a detached element or NULL. */
ZColor zrn_theme_color(const zrn_element_t* e, zrn_theme_color_id_t id);
/** The color of a named accent; an unknown value gives Cobalt. */
ZColor zrn_accent_color(ZAccent accent);

/** Forgets the tables read from theme.ini: the next use reads the file again. */
void zrn_theme_reset(void);

/** WP8 sizes in logical px (themes.md): the font sizes and the border thickness of the phone. ZRN_FONT_MEDIUM_LARGE, ZRN_BORDER_THICKNESS and ZRN_TOUCH_TARGET_OVERHANG are not used by any source. */
static constexpr float ZRN_FONT_SMALL = 18.667f;
static constexpr float ZRN_FONT_NORMAL = 20.0f;
static constexpr float ZRN_FONT_MEDIUM = 22.667f;
static constexpr float ZRN_FONT_MEDIUM_LARGE = 25.333f;
static constexpr float ZRN_FONT_LARGE = 32.0f;
static constexpr float ZRN_FONT_EXTRA_LARGE = 42.667f;
static constexpr float ZRN_FONT_EXTRA_EXTRA_LARGE = 72.0f;
static constexpr float ZRN_FONT_HUGE = 186.667f;
static constexpr float ZRN_BORDER_THICKNESS = 3.0f;
static constexpr float ZRN_TOUCH_TARGET_OVERHANG = 12.0f;

/** Windows Phone 8.1 sizes in logical px (themeresources.xaml and generic.xaml), taken as they are. */
static constexpr float ZRN_81_BORDER_THICKNESS = 2.5f;
/** PhoneTouchTargetOverhang is 0,9.5 and PhoneTouchTargetLargeOverhang 0,16: only above and below. */
static constexpr float ZRN_81_TOUCH_OVERHANG = 9.5f;
static constexpr float ZRN_81_TOUCH_LARGE_OVERHANG = 16.0f;
static constexpr float ZRN_81_FONT_LARGE = 18.14f;
static constexpr float ZRN_81_FONT_CONTROL_HEADER = 16.0f;
static constexpr float ZRN_81_FONT_CONTENT = 20.26f;
static constexpr float ZRN_81_FONT_EXTRA_LARGE = 25.5f;
static constexpr float ZRN_81_FONT_EXTRA_LARGE_PLUS = 29.86f;
