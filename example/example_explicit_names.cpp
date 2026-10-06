// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniil Goncharov <neargye@gmail.com>.
//
// Explicit external name table for an enum: three external names mapped to
// three enum values, with constexpr validation and binary-search parsing.
//
// This example is independent of the ordinary magic_enum mapping.
//
// Targeted C++ build command (C++17, header only), run from the repository
// root:
//
//   g++ -std=c++17 -Wall -Wextra -pedantic -Iinclude
//       example/example_explicit_names.cpp -o example_explicit_names
//
// A case-insensitive table that maps external "A" and "a" to different enum
// values is rejected at compile time (the file below defines such a table
// behind CASE_COLLISION):
//
//   g++ -std=c++17 -Iinclude -DCASE_COLLISION -fsyntax-only
//       example/example_explicit_names.cpp
//
// -> error: "the same canonical name must not point to different enum values"

#include <iostream>

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_explicit_names.hpp>

namespace en = magic_enum::explicit_names;

enum class Color { RED = -10, BLUE = 0, GREEN = 10 };

// Three external names, one per enum value. The table is generated and sorted at
// compile time; parsing is a binary search, not a linear rescan.
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, color_names, case_sensitive,
                               {{"RED", Color::RED}, {"GREEN", Color::GREEN}, {"BLUE", Color::BLUE}});

// ASCII case-insensitive table: spellings that fold to the same canonical name
// must agree on the enum value; agreeing spellings merge into one row whose
// sources keep both forms.
static constexpr en::name_entry<Color> ci_entries[] = {
    {"RED", Color::RED}, {"red", Color::RED}, // same value: merged aliases
    {"GREEN", Color::GREEN}, {"BLUE", Color::BLUE}};
static constexpr auto ci_names =
    en::make_name_table<en::case_policy::case_insensitive_ascii>(ci_entries);

#ifdef CASE_COLLISION
// Rejected at compile time: external "A" and "a" fold together but point at
// different enum values.
static constexpr en::name_entry<Color> bad_entries[] = {
    {"A", Color::RED}, {"a", Color::BLUE}};
static constexpr auto bad_names =
    en::make_name_table<en::case_policy::case_insensitive_ascii>(bad_entries);
#endif

int main() {
  std::cout << std::boolalpha;

  static_assert(color_names.size() == 3, "three names -> three rows");
  static_assert(en::unicode_support_scope == en::unicode_support::ascii_fold_only,
                "only ASCII case folding is supported");
  static_assert(!en::full_unicode_case_folding_supported,
                "full Unicode case folding is neither performed nor claimed");

  // Known names parse to their value.
  std::cout << "RED   -> " << static_cast<int>(*en::parse(color_names, "RED")) << '\n';
  std::cout << "GREEN -> " << static_cast<int>(*en::parse(color_names, "GREEN")) << '\n';
  std::cout << "BLUE  -> " << static_cast<int>(*en::parse(color_names, "BLUE")) << '\n';

  // Unknown input is an error, never a fallback to the zero-valued enum (BLUE).
  const auto unknown = en::parse(color_names, "purple");
  std::cout << "unknown 'purple' has value: " << unknown.has_value() << '\n';

  // Case policy mismatch is a distinct error.
  const auto wrong_policy =
      en::parse<en::case_policy::case_insensitive_ascii>(color_names, "RED");
  std::cout << "wrong policy status is invalid_policy: "
            << (wrong_policy.status == en::parse_status::invalid_policy) << '\n';

  // Merged aliases share one row and keep both sources.
  const auto folded = en::parse<en::case_policy::case_insensitive_ascii>(ci_names, "rEd");
  std::cout << "case-insensitive 'rEd' -> RED: "
            << (folded.status == en::parse_status::ok && *folded == Color::RED) << '\n';

  // The ordinary magic_enum mapping is untouched.
  std::cout << "magic_enum mapping unchanged: "
            << (magic_enum::enum_cast<Color>("BLUE").value() == Color::BLUE) << '\n';

  return 0;
}
