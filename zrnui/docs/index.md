# zrnSDK documentation

zrnSDK builds applications in the style of Windows Phone 8.1 in C23. How to build and install it is on the
[main page](../../Readme.md#building).

## Start here

| page | what is in it |
|---|---|
| [Getting started](getting-started.md) | handles (`Zid`), the application life cycle, building a tree, pages, events, input and time |
| [Theme, fonts, configuration](theme-and-resources.md) | dark/light theme, accents, `theme.ini`, theme resources, fonts, renderer and animations, `ZConfig` |
| [Embedding without zrnapp](embedding.md) | the loop of a platform layer: input in, frames out, OpenGL ES 2 target |
| [Building for a phone](devices.md) | cross build, upload and run on a phone with `tools/zrn`, login options, ssh keys |

## Widgets

| page | widgets |
|---|---|
| [Layout and panels](layout.md) | element tree, FrameworkElement properties, StackPanel, VirtualizingStackPanel, ItemsWrapGrid, WrapPanel, Grid, Canvas, Border, Viewbox, Popup |
| [Text and images](text.md) | TextBlock, RichTextBlock, Image, text styles |
| [Navigation](navigation.md) | UserControl, Page, Frame, Pivot, PivotItem, Hub, HubSection |
| [Buttons and toggles](buttons.md) | Button, HyperlinkButton, RepeatButton, ToggleButton, CheckBox, RadioButton, ToggleSwitch |
| [Range and progress](range-and-progress.md) | Slider, ProgressBar, ProgressRing, ScrollBar, Thumb, Separator |
| [Text input](text-input.md) | TextBox, PasswordBox, AutoSuggestBox |
| [Scrolling](scrolling.md) | ScrollViewer |
| [Items and selectors](items.md) | ItemsControl, Selector, ListBox, ListView, GridView, ComboBox, FlipView, LoopingSelector, SemanticZoom |
| [Date and time pickers](pickers.md) | DatePicker, TimePicker, DatePickerFlyout, TimePickerFlyout |
| [Flyouts and dialogs](flyouts-and-dialogs.md) | Flyout, MenuFlyout, ContentDialog, ToolTip |
| [App bars and icons](app-bars-and-icons.md) | AppBar, CommandBar, AppBarButton, AppBarToggleButton, AppBarSeparator, SymbolIcon, FontIcon, PathIcon, BitmapIcon |

## Reference

- The API is documented in the headers, every function with a `@brief`: `zrnui/include/zrnui/` (`id.h`, `core.h`, `element.h`,
  `controls.h`, `host.h`, `platform.h`, `resources.h`) and `zrnapp/include/zrnapp.h`. There is no umbrella `zrnui.h`: include
  what you use (each header includes the one before it: `platform.h`, `host.h`, `controls.h`, `element.h`, `core.h`, `id.h`; `id.h`
  alone is for code that only passes `Zid` around, as `zrnapp.h` does).
- [zrnapp](../../zrnapp/README.md): the Wayland window, title and app id.
- [zrnui](../Readme.md): layers, fonts, environment, fuzzing and coverage.
- [Code style](../../docs/CODESTYLE.md).
- Examples: [`examples/helloworld`](../../examples/helloworld), [`examples/demoapp`](../../examples/demoapp).
