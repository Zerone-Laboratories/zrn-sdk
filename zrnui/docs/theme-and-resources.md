# Theme, resources, fonts, configuration

## Theme and accent

A root has a theme (`Z_THEME_DARK`, the default, or `Z_THEME_LIGHT`) and an accent. The accent colours pressed buttons,
sliders, progress bars and selected items; whatever has no colour of its own follows the theme.

```c
Zroot_set_theme(root, Z_THEME_LIGHT);
Zroot_set_accent(root, Z_ACCENT_ORANGE);           // one of the 20 accents of Windows Phone 8
Zroot_set_accent_color(root, Zcolor_argb(0xFF1BA1E2));   // or any colour
Zroot_set_background(root, Zcolor_argb(0xFF101010));     // the colour behind the tree
```

Accents: Lime, Green, Emerald, Teal, Cyan, Cobalt (the default), Indigo, Violet, Pink, Magenta, Crimson, Red, Orange,
Amber, Yellow, Brown, Olive, Steel, Mauve, Taupe (`Z_ACCENT_*`).
`Zcolor_rgba(r, g, b, a)` takes floats, `Zcolor_argb(0xAARRGGBB)` the notation of the WP8 theme files.

## theme.ini

The colours of both themes are a readable file, `zrnui/theme/theme.ini`, with `Name = #AARRGGBB` entries in the sections
`[accent]`, `[dark]` and `[light]`. A missing key keeps the built-in value, so an override file may contain just the keys it changes.
A line that is not understood is skipped; a value is `#RRGGBB` or `#AARRGGBB`.
It is looked for in this order:

1. `ZConfig.theme_file` (`Zinit_ex`), or `$ZRNUI_THEME` with `Zinit`; `""` means no file at all;
2. next to the binary;
3. the one compiled in (`-DZRNUI_THEME_FILE=...`, installed as `share/zrnui/theme/theme.ini`).

A file that is not there beside the binary is skipped silently. When the path comes from `theme_file` / `$ZRNUI_THEME`, or is the
compiled-in one, and cannot be opened, `Z_ERROR_IO` is reported through the error callback and the built-in colours stay. No recompilation is needed after editing it.

## Theme resources

`resources.h` lists the 491 keys of `themeresources.xaml` of Windows Phone 8.1 as `ZThemeResource`
(`Z_RES_PHONE_FOREGROUND_BRUSH`, `Z_RES_PHONE_TOUCH_TARGET_OVERHANG`, ...). The header is generated; do not edit it.

| need | function |
|---|---|
| read a value for the current theme and accent | `Zroot_get_theme_color` / `_double` / `_int` / `_thickness` / `_font_family` |
| override a key for a root (both themes) | `Zroot_set_resource_color` / `_double` / `_int` / `_thickness` / `_font_family` |
| remove the override | `Zroot_clear_resource` |
| make a property follow a key | `Zcontrol_bind_foreground`, `_background`, `_border_brush`, `_border_thickness`, `_padding` |

A key of the theme keeps its type (an override of another type is refused and returns false). Keys from `Z_RES_USER_BASE` up
belong to the application and take the type of the first value set. Elements bound to a key follow every change.

```c
Zroot_set_resource_color(root, Z_RES_PHONE_FOREGROUND_BRUSH, Zcolor_argb(0xFFE51400));
Zcontrol_bind_foreground(label, Z_RES_PHONE_FOREGROUND_BRUSH);   // follows the key from now on
```

## Fonts

`Zinit` scans the font directory and, with fontconfig, every system font, and resolves the five faces from that inventory;
nothing is searched for while text is drawn. The bundled Selawik fonts (free, metric-compatible with Segoe UI) stand in for
Segoe WP and contain Latin only: Cyrillic and other scripts are drawn with a font fontconfig finds for the character,
so build with fontconfig to get them.

The font directory is, in order: `ZConfig.font_directory` / `$ZRNUI_FONT_DIR`, which override `Zset_font_directory` (call that
before `Zinit`), then `Zset_font_directory`, then the directory compiled in (`-DZRNUI_FONT_DIR=...`, installed as `share/fonts/zrnui/selawik`).
Text styles are in [Text](text.md).

## Renderer and animations

| setting | values | notes |
|---|---|---|
| `Zroot_set_renderer`, `$ZRNUI_RENDERER` | `Z_RENDERER_AUTO`, `_SOFTWARE`, `_GPU` | AUTO uses OpenGL ES 2 when the platform can, else the software painter. SOFTWARE never touches the GPU (safe mode). `Zroot_get_renderer_name` returns `"software"` or `"gles2"`. |
| `Zroot_set_animations`, `$ZRNUI_ANIMATIONS` | `Z_ANIMATIONS_AUTO`, on, off | AUTO runs animations only with the GPU renderer; in the software renderer every change is immediate. |
| `$ZRNUI_ANIMATION_SPEED` / `ZConfig.animation_speed` | float | 0.5 is half as fast, 2 twice as fast; 0 or less is ignored (1). |
| `Zroot_set_scale` | float | the layout grid is the window divided by the scale (720 x 1280 at 1.5 is laid out as 480 x 800) |

## ZConfig

A program that embeds zrnui should not rely on the environment. Pass the configuration explicitly:

```c
ZConfig config;
Zconfig_default(&config);                 // every field zero = the default, except animation_speed = 1
config.font_directory = "/opt/shell/fonts";
config.theme_file = "";                    // no theme file
config.renderer = Z_RENDERER_GPU;
if (!Zinit_ex(&config)) return 1;          // false when the fonts cannot be loaded
```

`Zconfig_from_environment(&config)` fills a `ZConfig` from the `ZRNUI_*` variables; it is what `Zinit` does.

Back to the [documentation index](index.md).
