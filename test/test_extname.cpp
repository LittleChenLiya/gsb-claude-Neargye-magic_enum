// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.

#include <magic_enum/magic_enum_extname.hpp>

enum class Color { red = 1, green = 2, blue = 3 };
enum class Direction { up = 0, down = 1 };

using magic_enum::extname::case_policy;
using magic_enum::extname::entry;
using magic_enum::extname::parse;
using magic_enum::extname::parse_error;

// Three-name table: "RED" and "red" are same-value aliases and merge sources.
inline constexpr auto color_table = magic_enum::extname::make_table<Color>(
    case_policy::insensitive_ascii,
    entry<Color>{"RED", Color::red},
    entry<Color>{"red", Color::red},
    entry<Color>{"green", Color::green});

// Same-value aliases merge into one canonical row.
static_assert(color_table.size() == 2);
static_assert(color_table[0].sources + color_table[1].sources == 3);
static_assert(color_table[1].sources == 2);

// Rows are sorted by canonical name at compile time for binary search.
static_assert(color_table[0].name == "green");
static_assert(color_table[1].name == "RED");

// Case-insensitive lookup folds ASCII only.
static_assert(parse(color_table, "Red").error == parse_error::none);
static_assert(parse(color_table, "Red").value == Color::red);
static_assert(parse(color_table, "GREEN").value == Color::green);
// An explicit sensitive policy matches only the representative spelling.
static_assert(parse(color_table, "RED", case_policy::sensitive).error == parse_error::none);
static_assert(parse(color_table, "red", case_policy::sensitive).error == parse_error::unknown_name);
static_assert(parse(color_table, "Red", case_policy::sensitive).error == parse_error::unknown_name);

// Unknown names report unknown_name and never fall back to a zero value.
static_assert(parse(color_table, "blue").error == parse_error::unknown_name);
static_assert(!parse(color_table, "blue"));
static_assert(!parse(color_table, ""));
static_assert(parse(color_table, "blue").value == Color{});

// Invalid policies are reported separately from unknown names.
static_assert(parse(color_table, "red", static_cast<case_policy>(42)).error == parse_error::invalid_policy);
static_assert(parse(color_table, "nope", static_cast<case_policy>(-1)).error == parse_error::invalid_policy);

// The declared Unicode support range is ASCII-only, no full Unicode folding.
static_assert(!magic_enum::extname::unicode_support_range.empty());

// Case-sensitive table matches byte-for-byte.
inline constexpr auto direction_table = magic_enum::extname::make_table<Direction>(
    case_policy::sensitive,
    entry<Direction>{"up", Direction::up},
    entry<Direction>{"down", Direction::down});

static_assert(direction_table.size() == 2);
static_assert(parse(direction_table, "up").value == Direction::up);
static_assert(parse(direction_table, "UP").error == parse_error::unknown_name);

// Array overload of make_table.
inline constexpr auto color_table_arr = magic_enum::extname::make_table<Color>(
    case_policy::insensitive_ascii,
    {{"RED", Color::red}, {"red", Color::red}, {"green", Color::green}});
static_assert(color_table_arr.size() == 2);

// Binary search returns rows from the compile-time generated table.
static_assert(color_table.find("red") == &color_table[1]);
static_assert(color_table.find("absent") == nullptr);

// The standard magic_enum mapping is unchanged by the external table.
static_assert(magic_enum::enum_name(Color::red) == "red");
static_assert(magic_enum::enum_cast<Color>("blue").value() == Color::blue);
static_assert(magic_enum::enum_cast<Color>("RED") == magic_enum::optional<Color>{});

int main() {
  // Runtime lookup goes through the same compile-time generated table.
  const auto ok = parse(color_table, "gReEn");
  if (!ok || ok.value != Color::green) {
    return 1;
  }

  const auto miss = parse(color_table, "blue");
  if (miss || miss.error != parse_error::unknown_name) {
    return 2;
  }

  const auto bad_policy = parse(color_table, "red", static_cast<case_policy>(7));
  if (bad_policy || bad_policy.error != parse_error::invalid_policy) {
    return 3;
  }

  if (color_table.find("green") == nullptr || color_table.find("green")->value != Color::green) {
    return 4;
  }

  // The declared support range must not claim full Unicode case folding.
  if (magic_enum::extname::unicode_support_range.find("not supported") == magic_enum::string_view::npos) {
    return 5;
  }

  return 0;
}
