// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.

#include <iostream>

#include <magic_enum/magic_enum_extname.hpp>

enum class Color { red = 1, green = 2, blue = 3 };

// Three-name external table: "RED" and "red" are same-value aliases and
// merge into one canonical row; the table is checked and sorted at compile
// time. Case folding covers ASCII letters only (see
// magic_enum::extname::unicode_support_range), full Unicode case folding is
// not supported.
inline constexpr auto color_names = magic_enum::extname::make_table<Color>(
    magic_enum::extname::case_policy::insensitive_ascii,
    magic_enum::extname::entry<Color>{"RED", Color::red},
    magic_enum::extname::entry<Color>{"red", Color::red},
    magic_enum::extname::entry<Color>{"green", Color::green});

int main() {
  std::cout << "support range: " << magic_enum::extname::unicode_support_range << '\n';
  std::cout << "canonical rows: " << color_names.size() << '\n'; // 2 rows from 3 names
  for (const auto& row : color_names) {
    std::cout << "  " << row.name << " -> " << magic_enum::enum_integer(row.value) << " (" << row.sources << " sources)\n";
  }

  const auto hit = magic_enum::extname::parse(color_names, "Green");
  std::cout << "parse(\"Green\"): " << (hit ? "ok" : "error") << '\n'; // ok

  const auto miss = magic_enum::extname::parse(color_names, "blue");
  std::cout << "parse(\"blue\") error: " << magic_enum::enum_integer(miss.error) << '\n'; // unknown_name, no zero fallback

  const auto bad = magic_enum::extname::parse(color_names, "red", static_cast<magic_enum::extname::case_policy>(9));
  std::cout << "parse(invalid policy) error: " << magic_enum::enum_integer(bad.error) << '\n'; // invalid_policy

  return hit && hit.value == Color::green && miss.error == magic_enum::extname::parse_error::unknown_name &&
                 bad.error == magic_enum::extname::parse_error::invalid_policy
             ? 0
             : 1;
}
