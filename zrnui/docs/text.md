# Text, images and fonts

This page covers the elements that show text and pictures: TextBlock,
RichTextBlock and Image, the text styles (`Z_TEXT_STYLE_*`) that give a
TextBlock the look of a Windows Phone 8.1 or 8.0 text style, and the fonts the
library uses. Editable text is in the TextBox and PasswordBox of the controls
(see [buttons.md](buttons.md) for the other basic controls).

On this page:

- [TextBlock](#textblock)
- [Text styles](#text-styles)
- [RichTextBlock](#richtextblock)
- [Image](#image)
- [Fonts](#fonts)

Layout properties (margin, alignment, size) are common to all elements: see
[layout.md](layout.md).

## TextBlock

**What it is.** A read-only run of text (WP8.1 `TextBlock`).

**Create.**

```c
Zid Ztext_block_create(const char* text);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `Ztext_block_set_text` / `Ztext_block_get_text` | The text (UTF-8, copied; NULL is empty). The pointer of the getter is valid until the next set or the destroy. | the text passed to create, "" for NULL |
| `Ztext_block_set_font_size(tb, size)` | FontSize. | inherited |
| `Ztext_block_set_font_family(tb, ZFontFamily)` | See [Fonts](#fonts). | inherited |
| `Ztext_block_set_foreground(tb, ZColor)` | Foreground color. | inherited |
| `Ztext_block_set_text_wrapping(tb, ZTextWrapping)` | `Z_NO_WRAP` or `Z_WRAP`. | `Z_NO_WRAP` |
| `Ztext_block_set_text_alignment(tb, ZTextAlignment)` | `Z_TEXT_LEFT`, `Z_TEXT_CENTER`, `Z_TEXT_RIGHT`. | `Z_TEXT_LEFT` |
| `Ztext_block_set_text_trimming(tb, ZTextTrimming)` | `Z_TRIM_NONE` or `Z_TRIM_WORD_ELLIPSIS`. | `Z_TRIM_NONE` |
| `Ztext_block_set_max_lines(tb, max_lines)` | MaxLines-like limit. | 0: no limit |
| `Ztext_block_set_line_height(tb, line_height)` | LineHeight in pixels. | 0: the line height of the font |
| `Ztext_block_set_character_spacing(tb, spacing)` | CharacterSpacing in 1/1000 em (the panorama titles use -35). | |
| `Ztext_block_set_style(tb, ZTextStyle)` | Applies a text style. Returns false for `Z_TEXT_STYLE_NONE` or an out-of-range value. | |
| `Ztext_block_bind_foreground(tb, key)` | Binds the foreground to a color or brush theme resource (for example `Z_RES_PHONE_FOREGROUND_BRUSH`). | not bound |
| `Ztext_block_bind_font_size(tb, key)` | Binds the font size to a Double key. | not bound |
| `Ztext_block_bind_character_spacing(tb, key)` | Binds the spacing to an Int key (for example `Z_RES_PIVOT_HEADER_ITEM_CHARACTER_SPACING`). | not bound |

A bound property follows the key when the theme, the accent or an override of
the root changes. Its direct setter (and `Ztext_block_set_style`) removes the
binding. The bind functions return false for a Zid that is not a TextBlock or a
key of another type.

**Events.** None of its own (routed events through `Zelement_on`).

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_title(void) {
    Zid title = Ztext_block_create("MY APPLICATION");
    Ztext_block_set_style(title, Z_TEXT_STYLE_TITLE);
    Zelement_set_margin(title, Zthickness(19, 12, 0, 0));
    return title;
}

Zid make_paragraph(const char* text) {
    Zid body = Ztext_block_create(text);
    Ztext_block_set_style(body, Z_TEXT_STYLE_BODY);
    Ztext_block_set_text_wrapping(body, Z_WRAP);
    Ztext_block_set_max_lines(body, 3);
    Ztext_block_set_text_trimming(body, Z_TRIM_WORD_ELLIPSIS);
    return body;
}
```

**Notes.**

- Font properties are inherited down the tree like Silverlight's: a TextBlock
  that was given none takes the ones of the nearest control above it (a Button
  gives its content the semibold 18.14 of its template). A value set on the
  TextBlock wins.
- `Ztext_block_set_style` sets font, size, foreground, line height, character
  spacing, wrapping and margin. A value set after the style wins.

## Text styles

`Ztext_block_set_style` applies a `ZTextStyle`. The values `Z_TEXT_STYLE_BASE`
to `Z_TEXT_STYLE_COMBO_BOX_PLACEHOLDER` are the Windows Phone 8.1 styles
(`HeaderTextBlockStyle`, `BodyTextBlockStyle`...). The `Z_TEXT_STYLE_PHONE_*`
values are the `PhoneText*Style` of Windows Phone 8.0 Silverlight. The 8.1
styles carry no margin; the `Z_TEXT_STYLE_PHONE_*` ones set 12 on the left and
right. `Z_TEXT_STYLE_NONE` is not a style (the call returns false).

Sizes are logical pixels. "Font" is a `ZFontFamily` value. Foreground says
what the color follows: "inherited" is the foreground of the surrounding control
or theme, "subtle", "contrast" and "accent" are theme colors. These columns are
taken from the implementation (`zrnui/src/controls/basic.c`), not from the
header comments.

### Windows Phone 8.1 styles

| Style | Font | Size | Line height | Wraps | Meaning (WP8.1 style) |
| --- | --- | --- | --- | --- | --- |
| `Z_TEXT_STYLE_BASE` | Normal | 18.14 | 21.5 | yes | BaseTextBlockStyle: body text base. |
| `Z_TEXT_STYLE_BODY` | Normal | 18.14 | 24 | yes | BodyTextBlockStyle: running text. |
| `Z_TEXT_STYLE_HEADER` | SemiLight | 57.5 | 57.5 | no | HeaderTextBlockStyle: the big page title. |
| `Z_TEXT_STYLE_SUBHEADER` | SemiLight | 30 | 33 | yes | SubheaderTextBlockStyle. |
| `Z_TEXT_STYLE_TITLE` | SemiBold | 18.14 | 21.5 | no | TitleTextBlockStyle: the application name above a page title. |
| `Z_TEXT_STYLE_GROUP_HEADER` | SemiLight | 23.73 | 23 | yes | GroupHeaderTextBlockStyle: subtle, character spacing -3. |
| `Z_TEXT_STYLE_CONTROL_HEADER` | Normal | 16 | 17.5 | yes | ControlHeaderTextBlockStyle: subtle header above a control. |
| `Z_TEXT_STYLE_CONTROL_CONTEXTUAL_INFO` | Normal | 16 | 17.5 | yes | ControlContextualInfoTextBlockStyle: subtle small note. |
| `Z_TEXT_STYLE_FLYOUT_PICKER_TITLE` | SemiBold | 18.14 | 21.5 | no | Title of a full-screen picker flyout, character spacing 15. |
| `Z_TEXT_STYLE_LIST_VIEW_ITEM` | SemiLight | 29.86 | 33.5 | no | ListViewItemTextBlockStyle: the main line of a list item. |
| `Z_TEXT_STYLE_LIST_VIEW_EMPTY_STATIC` | SemiLight | 29.86 | 33.5 | yes | Message of an empty list, subtle. |
| `Z_TEXT_STYLE_LIST_VIEW_ITEM_SUBHEADER` | SemiLight | 16 | 20 | no | Subtle second line of a list item. |
| `Z_TEXT_STYLE_LIST_VIEW_ITEM_CONTENT` | Normal | 16 | 20 | no | Details line of a list item. |
| `Z_TEXT_STYLE_MESSAGE_DIALOG_TITLE` | SemiBold | 25.5 | 31.5 | yes | Title of a message dialog, character spacing 3. |
| `Z_TEXT_STYLE_MESSAGE_DIALOG_CONTENT` | Normal | 18.14 | 21.5 | yes | Body of a message dialog. |
| `Z_TEXT_STYLE_COMBO_BOX_PLACEHOLDER` | Normal | 20.26 | 30 | no | Placeholder text of a ComboBox. |

Line height 0 in the implementation means "the line height of the font" (all
the 8.1 styles above set one).

### Windows Phone 8.0 Silverlight styles

All of these have a margin of 12 on the left and right, no wrapping and use the
font line height. The size is the Silverlight font size name.

| Style | Font | Size | Foreground | Meaning (Silverlight style) |
| --- | --- | --- | --- | --- |
| `Z_TEXT_STYLE_PHONE_NORMAL` | Normal | 20 | inherited | PhoneTextNormalStyle. |
| `Z_TEXT_STYLE_PHONE_SUBTLE` | Normal | 20 | subtle | PhoneTextSubtleStyle. |
| `Z_TEXT_STYLE_PHONE_TITLE1` | SemiLight | 72 | inherited | PhoneTextTitle1Style. |
| `Z_TEXT_STYLE_PHONE_TITLE2` | SemiLight | 32 | inherited | PhoneTextTitle2Style. |
| `Z_TEXT_STYLE_PHONE_TITLE3` | SemiLight | 22.667 | inherited | PhoneTextTitle3Style. |
| `Z_TEXT_STYLE_PHONE_EXTRA_LARGE` | SemiLight | 42.667 | inherited | PhoneTextExtraLargeStyle. |
| `Z_TEXT_STYLE_PHONE_GROUP_HEADER` | SemiLight | 32 | subtle | PhoneTextGroupHeaderStyle. |
| `Z_TEXT_STYLE_PHONE_LARGE` | SemiLight | 32 | inherited | PhoneTextLargeStyle. |
| `Z_TEXT_STYLE_PHONE_SMALL` | Normal | 18.667 | subtle | PhoneTextSmallStyle. |
| `Z_TEXT_STYLE_PHONE_CONTRAST` | SemiBold | 20 | contrast | PhoneTextContrastStyle. |
| `Z_TEXT_STYLE_PHONE_ACCENT` | SemiBold | 20 | accent | PhoneTextAccentStyle. |
| `Z_TEXT_STYLE_PHONE_HUGE` | Light | 186.667 | inherited | PhoneTextHugeStyle. |

Which style the demo application uses where: `Z_TEXT_STYLE_TITLE` for the
application name, `Z_TEXT_STYLE_HEADER` for the big page title,
`Z_TEXT_STYLE_SUBHEADER` and `Z_TEXT_STYLE_LIST_VIEW_ITEM_SUBHEADER` for the two
lines of a menu entry (`examples/demoapp/pages/main.c`).

## RichTextBlock

**What it is.** Paragraphs of plain text under one another (WP8.1
`RichTextBlock` with Paragraphs that hold a Run), each wrapped at the width of the
block.

**Create.**

```c
Zid Zrich_text_block_create(void);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `int Zrich_text_block_add_paragraph(rtb, text)` | Blocks.Append: a paragraph with a text. Returns its index; -1 for an invalid handle or on failure. | |
| `Zrich_text_block_set_paragraph_text(rtb, index, text)` / `Zrich_text_block_get_paragraph_text(rtb, index)` | Text of a paragraph. | |
| `Zrich_text_block_get_paragraph_count(rtb)` | Number of paragraphs. | |
| `Zid Zrich_text_block_get_paragraph(rtb, index)` | The TextBlock of a paragraph, for what the block has no function for. | |
| `Zrich_text_block_remove_paragraph_at(rtb, index)` | Removes a paragraph. | |
| `Zrich_text_block_clear_paragraphs(rtb)` | Removes all. | |
| `Zrich_text_block_set_font_size` / `Zrich_text_block_get_font_size` | FontSize. | as TextBlock |
| `Zrich_text_block_set_font_family(rtb, ZFontFamily)` | Font. | as TextBlock |
| `Zrich_text_block_set_foreground(rtb, ZColor)` | Foreground. | as TextBlock |
| `Zrich_text_block_set_text_wrapping`, `_set_text_alignment`, `_set_text_trimming` | As the TextBlock functions. | the text wraps (the 8.1 default) |
| `Zrich_text_block_set_line_height(rtb, line_height)` | LineHeight in pixels; 0 uses the line height of the font. | 0 |
| `Zrich_text_block_set_character_spacing(rtb, spacing)` | In 1/1000 em. | |
| `Zrich_text_block_set_padding` / `Zrich_text_block_get_padding` | Padding as `ZThickness`. | |
| `Zrich_text_block_set_style(rtb, style)` | Only `Z_TEXT_STYLE_BASE` (16 px, line 21.5) or `Z_TEXT_STYLE_BODY` (18 px, line 24), with the normal font, wrapping and no trimming; returns false for another style. | |

**Events.** None of its own.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_article(void) {
    Zid article = Zrich_text_block_create();
    Zrich_text_block_set_style(article, Z_TEXT_STYLE_BODY);
    Zrich_text_block_add_paragraph(article, "The first paragraph.");
    Zrich_text_block_add_paragraph(article, "The second paragraph.");
    Zrich_text_block_set_padding(article, Zthickness(19, 0, 19, 0));
    return article;
}
```

**Notes.**

- Minimal on purpose. Not made: inlines (Run, Bold, Hyperlink...),
  OverflowContentTarget and RichTextBlockOverflow, text selection, TextIndent,
  MaxLines, LineStackingStrategy.
- There is no space between paragraphs (the space is not in the XAML either).
- A paragraph is a TextBlock owned by the block; use
  `Zrich_text_block_get_paragraph` to reach TextBlock-only functions.
- For a RichTextBlock `Z_TEXT_STYLE_BASE` is 16 px (line 21.5) and
  `Z_TEXT_STYLE_BODY` 18 px (line 24); on a TextBlock the same styles are 18.14 px.
- The look set on the block goes to every paragraph, present and added later. A
  property that was never set leaves the paragraphs to inherit it.

## Image

**What it is.** Shows a bitmap (WP8.1 `Image`).

**Create.**

```c
Zid Zimage_create(void);
```

**Properties.**

| Functions | Meaning | Default |
| --- | --- | --- |
| `bool Zimage_set_source_file(image, path)` | Image.Source from a PNG file. Returns false on failure (an unreadable file is `Z_ERROR_IO`); the image stays as it was. | no source: the image measures 0 x 0 |
| `bool Zimage_set_source_rgba(image, width, height, rgba)` | Image.Source from pixels: width * height * 4 RGBA bytes (`const uint8_t*`), copied. Returns false when `width` or `height` is not positive, `rgba` is NULL or memory is out; the image stays as it was. | |
| `Zimage_set_stretch(image, ZStretch)` | `Z_STRETCH_NONE`, `Z_STRETCH_FILL`, `Z_STRETCH_UNIFORM`, `Z_STRETCH_UNIFORM_TO_FILL`. The picture is cut at the bounds of the element. | `Z_STRETCH_UNIFORM` |
| `ZSize Zimage_get_natural_size(image)` | Size of the bitmap in pixels; 0 x 0 without a source. | |

**Events.** None of its own.

**Example.**

```c
#include <zrnui/controls.h>
#include <zrnui/host.h>

Zid make_logo(const char* png_path) {
    Zid image = Zimage_create();
    if (!Zimage_set_source_file(image, png_path)) {
        /* the image stays empty; Zget_error_message() tells why */
    }
    Zimage_set_stretch(image, Z_STRETCH_UNIFORM);
    Zelement_set_width(image, 200);
    return image;
}
```

**Notes.** The only file format named in the header is PNG.

## Fonts

`ZFontFamily` values, used by `Ztext_block_set_font_family`,
`Zrich_text_block_set_font_family` and `Zcontrol_set_font_family`:

| Value | Weight |
| --- | --- |
| `Z_FONT_NORMAL` | Regular |
| `Z_FONT_LIGHT` | Light |
| `Z_FONT_SEMILIGHT` | Semilight |
| `Z_FONT_SEMIBOLD` | Semibold |
| `Z_FONT_BOLD` | Bold |

The library looks for the Selawik fonts bundled in `zrnui/fonts/selawik`
(`selawk.ttf` regular, `selawkl.ttf` light, `selawksl.ttf` semilight,
`selawksb.ttf` semibold, `selawkb.ttf` bold): free fonts that metric-match Segoe UI
and stand in for Segoe WP. A Segoe font in the font directory or installed in the
system is preferred, and a weight that is missing falls back to a neighbour.

| Function | Meaning |
| --- | --- |
| `Zset_font_directory(path)` (`core.h`) | Directory of the fonts. Call before `Zinit`. |

Which directory is used: the `font_directory` of `ZConfig` (`Zinit_ex`), or
`$ZRNUI_FONT_DIR` with `Zinit`, when it is set; it overrides `Zset_font_directory`.
Otherwise the directory given to `Zset_font_directory`, otherwise the directory
compiled in. `Zinit` returns false when no font for the regular face is found.

Control fonts: `Zcontrol_set_font_size`, `Zcontrol_get_font_size` and
`Zcontrol_set_font_family` set the font of a control; a TextBlock inside takes
them when it has none of its own.

Back to the [documentation index](index.md)
