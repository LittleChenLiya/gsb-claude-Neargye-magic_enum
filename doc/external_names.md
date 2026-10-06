# Explicit External Name Tables (`magic_enum_extname.hpp`)

`magic_enum::extname` provides explicit external name tables: a user-supplied
list of `(name, value)` entries that is pre-checked at compile time and
compiled into a sorted lookup table. The standard `magic_enum` enum-name
mapping (`enum_name`, `enum_cast`, ...) is **not** changed by this facility.

## Constexpr pre-check

`magic_enum::extname::make_table<E>(policy, entries...)` validates the table
when it is built (with C++20 `consteval` this always happens during
compilation; on C++17 the checks fire in constant expressions):

* the case policy must be a valid `case_policy` enumerator;
* names must not be empty;
* **one canonical name must not map to different enum values** — for example
  `"A"` and `"a"` under `insensitive_ascii` pointing at different enumerators
  is a compile-time error, the compile plan is rejected;
* **same-value aliases of one canonical name merge their sources** — `"RED"`
  and `"red"` for the same enumerator become one row with `sources == 2`;
* rows are sorted by canonical name, so `find`/`parse` binary-search the
  compile-time generated table in `O(log n)` and never linearly re-enumerate
  the source entries.

## Case policy and Unicode support range

```cpp
enum class case_policy { sensitive = 0, insensitive_ascii = 1 };
```

The declared Unicode support range is intentionally narrow:

> Case folding covers **ASCII letters A-Z/a-z only**. Every non-ASCII
> character is compared byte-wise. **Full Unicode case folding is not
> supported and is not claimed.** Non-ASCII names are allowed, they just
> never fold.

The declaration is also available to code as
`magic_enum::extname::unicode_support_range`.

## Distinct parse errors

`magic_enum::extname::parse` returns `parse_result<E>{value, error}` and
keeps failure causes apart instead of falling back to a zero enum value:

* `parse_error::none` — name found, `value` is valid;
* `parse_error::unknown_name` — name not present in the table (an unknown
  name never silently yields `E{}`);
* `parse_error::invalid_policy` — the supplied policy is not a valid
  `case_policy` enumerator (reported separately from unknown input).

## Three-name example

```cpp
#include <magic_enum/magic_enum_extname.hpp>

enum class Color { red = 1, green = 2, blue = 3 };

// Three names, two canonical rows: "RED"/"red" merge as same-value aliases.
inline constexpr auto color_names = magic_enum::extname::make_table<Color>(
    magic_enum::extname::case_policy::insensitive_ascii,
    magic_enum::extname::entry<Color>{"RED", Color::red},
    magic_enum::extname::entry<Color>{"red", Color::red},
    magic_enum::extname::entry<Color>{"green", Color::green});

static_assert(color_names.size() == 2);
static_assert(magic_enum::extname::parse(color_names, "Green").value == Color::green);
static_assert(magic_enum::extname::parse(color_names, "blue").error ==
              magic_enum::extname::parse_error::unknown_name);
```

A conflicting table is rejected at compile time:

```cpp
// error: magic_enum::extname: canonical name maps to different enum values
inline constexpr auto bad = magic_enum::extname::make_table<Color>(
    magic_enum::extname::case_policy::insensitive_ascii,
    magic_enum::extname::entry<Color>{"A", Color::red},
    magic_enum::extname::entry<Color>{"a", Color::green});
```

## Targeted C++ commands

Build and run only this feature (no full-repo rebuild):

```sh
# Direct compiler invocation (C++17 or newer):
c++ -std=c++17 -I include example/example_extname.cpp -o example_extname
./example_extname

c++ -std=c++17 -I include test/test_extname.cpp -o test_extname
./test_extname

# CMake targets:
cmake -S . -B build
cmake --build build --target example_extname
cmake --build build --target test_extname-cpp17
ctest --test-dir build -R extname --output-on-failure

# Compile-fail checks (expected to fail with the extname diagnostics):
c++ -std=c++17 -I include -DMAGIC_ENUM_EXTNAME_COMPILE_FAIL_CASE=1 -c test/test_extname_compile_fail.cpp
```
