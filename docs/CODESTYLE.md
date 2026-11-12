# Code Style (zrnui and zrnapp)

## Language
- C23 (`-std=c23`), CMake project.
- No C++ features.

## Naming
- Public API: `Z` prefix, CamelCase (`Zroot_create`, `Zid`).
- Constants and enum values: `Z_` prefix (`Z_EVENT_CLICK`).
- Internal functions and types: `zrn_` prefix, snake_case; internal constants and macros: `ZRN_` prefix.
- Internal types: `_t` suffix (`zrn_root_t`, `glyph_entry_t`). Public types have none (`Zid`, `ZColor`, `ZFrame`).

## Formatting
- Tabs for indentation.
- Braces on same line for control statements.
- One statement per line; no chained declarations where unclear.

## Booleans
- Use the `bool` type (a keyword in C23)
- Name boolean variables and fields with a `b_` prefix (`b_closed`, `b_is_root`)

## Integers
- Use uint and 8/16/32 bit size where it appreciatable

## If
- Use inverted if() to increase code readability

## Const
- Use const in func parameters and variables in code

## Memory
- Manual allocation (`calloc`/`malloc`/`free`).
- Check allocation results and report via `zrn_error`.

## Errors
- Use `zrn_error(...)` for runtime errors; return invalid IDs/`false`.

## Includes
- A `.c` file includes its own header first, then the other zrnui headers, then the system headers.
- Headers are guarded with `#pragma once`.

## Comments
- Doxygen-style `/** ... */` for public API.
- Non english comments not allowed; 
