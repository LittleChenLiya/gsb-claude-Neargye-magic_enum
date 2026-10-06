// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniil Goncharov <neargye@gmail.com>.

// Focused, self-contained tests for the explicit external name table facility.
// Built and run as a tiny standalone binary (no doctest dependency).

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_explicit_names.hpp>

#include <array>
#include <cassert>
#include <string_view>

namespace en = magic_enum::explicit_names;

enum class Color { RED = -10, BLUE = 0, GREEN = 10 };

// Three external names for three distinct enum values.
static constexpr en::name_entry<Color> color_entries[] = {
    {"RED", Color::RED}, {"GREEN", Color::GREEN}, {"BLUE", Color::BLUE}};

static constexpr auto color_table =
    en::make_name_table<en::case_policy::case_sensitive>(color_entries);

// A case-insensitive table where two spellings name the same value (merge).
static constexpr en::name_entry<Color> ci_entries[] = {
    {"RED", Color::RED}, {"red", Color::RED}, {"Green", Color::GREEN}, {"BLUE", Color::BLUE}};
static constexpr auto ci_table =
    en::make_name_table<en::case_policy::case_insensitive_ascii>(ci_entries);

// Macro form, including an enum whose zero value is explicitly not named.
enum class Status : int { Unknown = 0, Active = 1, Closed = 2 };
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Status, status_table, case_sensitive,
                               {{"active", Status::Active}, {"closed", Status::Closed}});

// Non-ASCII names: matched byte for byte; no Unicode folding is performed.
enum class Language { Japanese, Korean, Swedish };
static constexpr en::name_entry<Language> lang_entries[] = {
    {"日本語", Language::Japanese}, {"한국어", Language::Korean}, {"TVÅ", Language::Swedish}};
static constexpr auto lang_table =
    en::make_name_table<en::case_policy::case_insensitive_ascii>(lang_entries);

static_assert(color_table.size() == 3, "three distinct names -> three rows");
static_assert(color_table.canonical_name(0) == "BLUE", "rows are sorted at compile time");
static_assert(color_table.canonical_name(1) == "GREEN");
static_assert(color_table.canonical_name(2) == "RED");

// Compile-time parsing, exact policy.
static_assert(en::parse<en::case_policy::case_sensitive>(color_table, "RED").value == Color::RED);
static_assert(en::parse<en::case_policy::case_sensitive>(color_table, "GREEN").value == Color::GREEN);
static_assert(en::parse<en::case_policy::case_sensitive>(color_table, "BLUE").value == Color::BLUE);

static_assert(en::parse<en::case_policy::case_sensitive>(color_table, "red").status ==
                  en::parse_status::unknown_name,
              "case sensitive table does not fold case");
static_assert(en::parse<en::case_policy::case_sensitive>(color_table, "PURPLE").status ==
                  en::parse_status::unknown_name,
              "unknown input is reported, never a fallback value");

// Unknown name must not silently yield the zero-valued enumerator.
static_assert(!en::parse(color_table, "missing").has_value());
static_assert(en::parse(color_table, "missing").status == en::parse_status::unknown_name);
static_assert(!en::parse(status_table, "").has_value());
static_assert(en::parse(status_table, "bogus").status == en::parse_status::unknown_name);

// A policy that does not match the table policy is a separate error.
static_assert(en::parse<en::case_policy::case_insensitive_ascii>(color_table, "RED").status ==
              en::parse_status::invalid_policy);
static_assert(en::parse<en::case_policy::unicode_case_insensitive>(color_table, "RED").status ==
              en::parse_status::invalid_policy);

constexpr bool runtime_policy_checks() {
  // Matching runtime policy works and folds ASCII case.
  const auto r1 = en::parse_with_policy(ci_table, "rEd", en::case_policy::case_insensitive_ascii);
  if (r1.status != en::parse_status::ok || *r1 != Color::RED) {
    return false;
  }
  // Conflicting policy is an invalid_policy error, not unknown_name.
  const auto r2 = en::parse_with_policy(ci_table, "RED", en::case_policy::case_sensitive);
  if (r2.status != en::parse_status::invalid_policy || r2.has_value()) {
    return false;
  }
  // The reserved/unsupported policy value is rejected distinctly.
  const auto r3 = en::parse_with_policy(ci_table, "RED", en::case_policy::unicode_case_insensitive);
  if (r3.status != en::parse_status::invalid_policy) {
    return false;
  }
  // Unknown name after policy check.
  const auto r4 = en::parse_with_policy(ci_table, "cyan", en::case_policy::case_insensitive_ascii);
  if (r4.status != en::parse_status::unknown_name || r4.has_value()) {
    return false;
  }
  return true;
}
static_assert(runtime_policy_checks(), "runtime policy parsing");

// Merging: "RED" and "red" collapse to one row whose sources carry both.
static_assert(ci_table.size() == 3, "same canonical name merges to one row");
constexpr bool merged_sources() {
  for (std::size_t i = 0; i < ci_table.size(); ++i) {
    if (ci_table.canonical_name(i) == "RED") {
      const auto sources = ci_table.sources_of(i);
      bool has_upper = false;
      bool has_lower = false;
      for (std::string_view s : sources) {
        has_upper = has_upper || s == "RED";
        has_lower = has_lower || s == "red";
      }
      return sources.size == 2 && has_upper && has_lower;
    }
  }
  return false;
}
static_assert(merged_sources(), "merged row keeps both alias spellings as sources");

// sources_of_value aggregates sources for aliases that share an enum value.
constexpr bool value_sources() {
  std::size_t count = 0;
  const auto sources = ci_table.sources_of_value(Color::RED, count);
  if (count != 2) {
    return false;
  }
  bool has_upper = false;
  bool has_lower = false;
  for (std::size_t i = 0; i < count; ++i) {
    has_upper = has_upper || sources[i] == "RED";
    has_lower = has_lower || sources[i] == "red";
  }
  return has_upper && has_lower;
}
static_assert(value_sources(), "sources_of_value combines same-value aliases");

// Non-ASCII bytes are compared exactly even in the ASCII-folding policy.
static_assert(en::parse<en::case_policy::case_insensitive_ascii>(lang_table, "日本語").value ==
              Language::Japanese);
static_assert(en::parse<en::case_policy::case_insensitive_ascii>(lang_table, "한국어").value ==
              Language::Korean);
static_assert(en::parse<en::case_policy::case_insensitive_ascii>(lang_table, "tVå").status ==
                  en::parse_status::unknown_name,
              "no full Unicode folding: lowercase a-ring does not fold to capital A-ring");
static_assert(en::parse<en::case_policy::case_insensitive_ascii>(lang_table, "två").status ==
                  en::parse_status::unknown_name,
              "non-ASCII letters are not folded");
static_assert(en::unicode_support_scope == en::unicode_support::ascii_fold_only);
static_assert(!en::full_unicode_case_folding_supported);

// The ordinary magic_enum mapping is unaffected by the external tables.
static_assert(magic_enum::enum_name(Color::RED) == "RED");
static_assert(magic_enum::enum_cast<Color>("BLUE").value() == Color::BLUE);
static_assert(!magic_enum::enum_cast<Color>("RED-BY-EXTERNAL-TABLE").has_value());

// In a case-sensitive table "A" and "a" are different canonical names; when
// they happen to name the same enum value they are accepted and the value's
// sources combine both aliases.
static constexpr en::name_entry<Color> same_value_entries[] = {
    {"A", Color::RED}, {"a", Color::RED}, {"BLUE", Color::BLUE}};
static constexpr auto same_value_table =
    en::make_name_table<en::case_policy::case_sensitive>(same_value_entries);
static_assert(same_value_table.size() == 3, "case-sensitive names stay distinct rows");
static_assert(en::parse(same_value_table, "A").value == Color::RED);
static_assert(en::parse(same_value_table, "a").value == Color::RED);
constexpr bool same_value_sources() {
  std::size_t count = 0;
  const auto sources = same_value_table.sources_of_value(Color::RED, count);
  bool upper = false;
  bool lower = false;
  for (std::size_t i = 0; i < count; ++i) {
    upper = upper || sources[i] == "A";
    lower = lower || sources[i] == "a";
  }
  return count == 2 && upper && lower;
}
static_assert(same_value_sources(), "aliases sharing one value combine sources");

// The constexpr precheck is also usable directly as a structured result.
static constexpr en::name_entry<Color> collision_entries[] = {{"A", Color::RED}, {"a", Color::BLUE}};
static_assert(en::detail::validate<en::case_policy::case_sensitive>(color_entries).valid);
static_assert(en::detail::validate<en::case_policy::case_sensitive>(collision_entries).valid,
              "A and a are distinct under case_sensitive even with different values");
static_assert(!en::detail::validate<en::case_policy::case_insensitive_ascii>(collision_entries).valid,
              "A and a collide under ASCII folding while naming different values");

// Runtime smoke checks.
int main() {
  // Macro works inside a function body as well (checked independently of
  // assert, which compiles away under NDEBUG).
  MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, local, case_sensitive,
                                 {{"RED", Color::RED}, {"BLUE", Color::BLUE}});
  if (en::parse(local, "BLUE").value != Color::BLUE) {
    return 1;
  }
  if (en::parse(local, "red").status != en::parse_status::unknown_name) {
    return 1;
  }

  assert(en::parse(color_table, "GREEN").value == Color::GREEN);
  assert(en::parse(ci_table, "green").status == en::parse_status::ok);
  assert(*en::parse(ci_table, "green") == Color::GREEN);
  assert(!en::parse(color_table, "nope").has_value());
  assert(runtime_policy_checks());
  return 0;
}
