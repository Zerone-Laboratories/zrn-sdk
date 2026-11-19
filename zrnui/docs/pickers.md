# Pickers

DatePicker and TimePicker are a header over a button that shows the value; a tap shows a full-screen flyout with looping selectors and "done" / "cancel" buttons. The flyouts (`DatePickerFlyout`, `TimePickerFlyout`, based on `PickerFlyoutBase`) can also be used by themselves. The selectors inside are [LoopingSelectors](items.md#loopingselector). Flyouts in general are in [flyouts-and-dialogs.md](flyouts-and-dialogs.md).

Widgets on this page:

- [DatePicker](#datepicker)
- [TimePicker](#timepicker)
- [DatePickerFlyout](#datepickerflyout)
- [TimePickerFlyout](#timepickerflyout)
- [PickerFlyoutBase](#pickerflyoutbase)

Value types (controls.h):

```c
typedef struct ZDate { int year, month, day; } ZDate;          // Gregorian
typedef struct ZTimeOfDay { int hour, minute; } ZTimeOfDay;    // 24 hour clock
typedef enum ZClockIdentifier { Z_CLOCK_12_HOUR, Z_CLOCK_24_HOUR } ZClockIdentifier;
```

## DatePicker

**What it is.** The Windows Phone 8.1 `DatePicker`: a Header (16 px) over a 2.5 px outlined Button as wide as its room showing the value ("November 7, 2026"; the formats are the library's).

**Create**

```c
Zid Zdate_picker_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zdate_picker_set_header(p, text)` / `Zdate_picker_get_header(p)` | Header; also the title of the flyout | none |
| `Zdate_picker_set_date(p, date)` / `Zdate_picker_get_date(p)` | `Date`: coerced into MinYear..MaxYear and to a day the month has; `DateChanged` comes when it is another date | today (UTC) |
| `Zdate_picker_set_min_year(p, y)` / `Zdate_picker_get_min_year(p)` | `MinYear` (the date is coerced again) | 100 years before this year |
| `Zdate_picker_set_max_year(p, y)` / `Zdate_picker_get_max_year(p)` | `MaxYear` | 100 years after this year |
| `Zdate_picker_set_day_visible(p, on)` / `_get_day_visible` | `DayVisible`: which selectors the flyout has | true |
| `Zdate_picker_set_month_visible(p, on)` / `_get_month_visible` | `MonthVisible` | true |
| `Zdate_picker_set_year_visible(p, on)` / `_get_year_visible` | `YearVisible` | true |
| `Zdate_picker_get_flyout(p)` | The `DatePickerFlyout` the picker owns. It is the Flyout of the picker's button, so it can be taken away (`Zbutton_set_flyout` with `Z_INVALID_ID`) or destroyed; the getter then returns `Z_INVALID_ID` and the picker keeps working | |
| `Zdate_picker_get_flyout_button(p)` | The Button that shows the flyout (template part `FlyoutButton`) | |

**Events.** `Zdate_picker_on_date_changed(p, ZDateChangedCallback, user_data)`; callback `void (*)(Zid sender, ZDate old_date, ZDate new_date, void* user_data)`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void changed(Zid sender, ZDate old_date, ZDate new_date, void* user_data) {
	(void)sender; (void)old_date; (void)new_date; (void)user_data;
}

Zid make_birthday(void) {
	Zid picker = Zdate_picker_create();
	Zdate_picker_set_header(picker, "birthday");
	Zdate_picker_set_date(picker, (ZDate){ 1999, 12, 31 });
	Zdate_picker_on_date_changed(picker, changed, NULL);
	return picker;
}
```

**Notes**

- The flyout fills the screen: the Header as its title, selectors side by side (month, day with its weekday, year) and "done" / "cancel" under them.
- Done makes the selectors the value: the picker's Date changes and DateChanged comes (the flyout's DatePicked comes first, before the flyout closes). Done also copies the flyout's own year range (MinYear, MaxYear) to the picker. Cancel, Back and a tap outside close the flyout and leave the value.
- Another month or year makes the days those of that month (a day past its end becomes the last one); the calendar is Gregorian, in plain arithmetic.
- Not included: CalendarIdentifier, Day/Month/YearFormat, Orientation, header and flyout templates.

## TimePicker

**What it is.** The 8.1 `TimePicker`: the same layout as DatePicker, showing "3:07 PM" or "15:07".

**Create**

```c
Zid Ztime_picker_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Ztime_picker_set_header(p, text)` / `Ztime_picker_get_header(p)` | Header | none |
| `Ztime_picker_set_time(p, time)` / `Ztime_picker_get_time(p)` | `Time`; `TimeChanged` comes when it is another time | 0:00 |
| `Ztime_picker_set_minute_increment(p, n)` / `Ztime_picker_get_minute_increment(p)` | 1 to 59, a value outside is clamped: the minute selector has 0, n, 2n... | 1 |
| `Ztime_picker_set_clock_identifier(p, clock)` / `Ztime_picker_get_clock_identifier(p)` | `Z_CLOCK_12_HOUR` (with an AM / PM selector) or `Z_CLOCK_24_HOUR` | `Z_CLOCK_12_HOUR` |
| `Ztime_picker_get_flyout(p)` | The `TimePickerFlyout` the picker owns; as for the date picker, it hangs on the picker's button and can be taken away (then `Z_INVALID_ID`) | |
| `Ztime_picker_get_flyout_button(p)` | The Button that shows the flyout | |

**Events.** `Ztime_picker_on_time_changed(p, ZTimeChangedCallback, user_data)`; callback `void (*)(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data)`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void time_changed(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data) {
	(void)sender; (void)old_time; (void)new_time; (void)user_data;
}

Zid make_alarm(void) {
	Zid alarm = Ztime_picker_create();
	Ztime_picker_set_header(alarm, "alarm");
	Ztime_picker_set_clock_identifier(alarm, Z_CLOCK_24_HOUR);
	Ztime_picker_set_minute_increment(alarm, 5);
	Ztime_picker_set_time(alarm, (ZTimeOfDay){ 7, 30 });
	Ztime_picker_on_time_changed(alarm, time_changed, NULL);
	return alarm;
}
```

**Notes**

- A time whose minute is not on the grid of MinuteIncrement is kept by the picker, but the flyout shows the nearest grid minute, and Done keeps that minute.
- Done copies the flyout's minute increment and clock to the picker, with the time (TimePicked of the flyout comes first, then TimeChanged).
- Selectors: hour, minute, AM / PM.

## DatePickerFlyout

**What it is.** The `DatePickerFlyout` by itself: made, given a value, shown with `Zflyout_base_show_at`, and listened to with DatePicked. It is a FlyoutBase with placement Full (Opening, Opened, Closed and Hide work).

**Create**

```c
Zid Zdate_picker_flyout_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zdate_picker_flyout_set_date(f, date)` / `Zdate_picker_flyout_get_date(f)` | Date | today until shown |
| `Zdate_picker_flyout_set_min_year(f, y)` / `_set_max_year` / `_get_min_year` / `_get_max_year` | Year range | |
| `Zdate_picker_flyout_set_day_visible(f, on)` / `_set_month_visible` / `_set_year_visible` and the `_get_` forms | Which selectors show | true |

**Events.** `Zdate_picker_flyout_on_date_picked(f, ZDatePickedCallback, user_data)`; callback `void (*)(Zid sender, ZDate old_date, ZDate new_date, void* user_data)`. Fired when Done was tapped (also when the date is the same); the new date is already the flyout's Date. A flyout that belongs to a picker then hands its date and year range to that picker. One handler; `NULL` removes it.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void picked(Zid sender, ZDate old_date, ZDate new_date, void* user_data) {
	(void)sender; (void)old_date; (void)new_date; (void)user_data;
}

void ask_date(Zid anchor) {
	Zid fly = Zdate_picker_flyout_create();
	Zpicker_flyout_base_set_title(fly, "pick a day");
	Zdate_picker_flyout_set_date(fly, (ZDate){ 2026, 11, 7 });
	Zdate_picker_flyout_on_date_picked(fly, picked, NULL);
	Zflyout_base_show_at(fly, anchor);
}
```

**Notes**

- As with other flyouts, the program owns a flyout until it is given to something; destroy it with `Zelement_destroy` when done ([flyouts-and-dialogs.md](flyouts-and-dialogs.md)). `anchor` must be in a tree that belongs to a root.

## TimePickerFlyout

**What it is.** The `TimePickerFlyout` by itself; same use as DatePickerFlyout.

**Create**

```c
Zid Ztime_picker_flyout_create(void);
```

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Ztime_picker_flyout_set_time(f, t)` / `Ztime_picker_flyout_get_time(f)` | Time | 0:00 |
| `Ztime_picker_flyout_set_minute_increment(f, n)` / `_get_minute_increment` | Minute grid, 1 to 59 (a value outside is clamped). The flyout shows the nearest grid minute to its Time, and Done keeps that minute. | 1 |
| `Ztime_picker_flyout_set_clock_identifier(f, clock)` / `_get_clock_identifier` | 12 or 24 hour | 12 hour |

**Events.** `Ztime_picker_flyout_on_time_picked(f, ZTimePickedCallback, user_data)`; callback `void (*)(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data)`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

static void time_picked(Zid sender, ZTimeOfDay old_time, ZTimeOfDay new_time, void* user_data) {
	(void)sender; (void)old_time; (void)new_time; (void)user_data;
}

void ask_time(Zid anchor) {
	Zid fly = Ztime_picker_flyout_create();
	Ztime_picker_flyout_set_clock_identifier(fly, Z_CLOCK_24_HOUR);
	Ztime_picker_flyout_on_time_picked(fly, time_picked, NULL);
	Zflyout_base_show_at(fly, anchor);
}
```

## PickerFlyoutBase

**What it is.** The shared part of both picker flyouts: a Title, the selectors and the Done / Cancel buttons. The functions take a DatePickerFlyout or TimePickerFlyout (also the ones a picker owns, from `Zdate_picker_get_flyout` / `Ztime_picker_get_flyout`).

**Create.** None; use `Zdate_picker_flyout_create` or `Ztime_picker_flyout_create`.

**Properties**

| Function | Meaning | Default |
|---|---|---|
| `Zpicker_flyout_base_set_title(f, title)` / `Zpicker_flyout_base_get_title(f)` | `Title` over the selectors; `""` removes it | none (a picker's flyout takes the picker's Header when it shows) |
| `Zpicker_flyout_base_get_selector(f, index)` | The LoopingSelector 0 to 2, left to right: month, day, year (date); hour, minute, AM / PM (time) | |
| `Zpicker_flyout_base_get_done_button(f)` | The Done button (an AppBarButton) | |
| `Zpicker_flyout_base_get_cancel_button(f)` | The Cancel button (an AppBarButton) | |

**Events.** The FlyoutBase events: `Zflyout_base_on_opening`, `Zflyout_base_on_opened`, `Zflyout_base_on_closed` (callback `void (*)(Zid sender, void* user_data)`), plus `Zflyout_base_show_at`, `Zflyout_base_hide`, `Zflyout_base_get_is_open`.

**Example**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

void tune_flyout(Zid picker) {
	Zid fly = Zdate_picker_get_flyout(picker);
	Zid month = Zpicker_flyout_base_get_selector(fly, 0);
	Zlooping_selector_set_should_loop(month, false);
	Zpicker_flyout_base_set_title(fly, "when?");
}
```

**Notes**

- The flyout placement is Full (`Z_FLYOUT_PLACEMENT_FULL`).

Back to the [documentation index](index.md)
