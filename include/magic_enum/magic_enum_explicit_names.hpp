// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Daniil Goncharov <neargye@gmail.com>.
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS  OR  COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Explicit external name table for enum parsing.
//
// This facility is deliberately separate from the built-in magic_enum name
// mapping (magic_enum::enum_cast / enum_name): defining an explicit external
// table never changes the ordinary magic_enum mapping.
//
// A table is validated at compile time (a constexpr precheck):
//   * two entries whose canonical name is identical must refer to the same enum
//     value; pointing at different enum values is rejected ("the same canonical
//     name cannot point to different enum values");
//   * equal entries are merged into one row, keeping every original spelling as
//     a source (aliases sharing one value combine their sources);
//   * empty names are rejected;
//   * the case policy is fixed when the table is built and an unsupported
//     policy is a compile-time error;
//   * the lookup table is generated and sorted at compile time, so parsing uses
//     binary search instead of linearly enumerating the entries again;
//   * unknown input never silently falls back to a value (such as the
//     zero-valued enumerator); unknown input and an illegal policy are reported
//     with distinct status codes.
//
// Case policy / Unicode support scope:
//   * case_sensitive compares names byte for byte;
//   * case_insensitive_ascii folds only the 26 ASCII letters A-Z/a-z; every
//     other byte (including the bytes of UTF-8 encoded non-ASCII letters) is
//     compared unchanged. This facility therefore does NOT perform and does NOT
//     claim full Unicode case folding; the unicode_case_insensitive policy
//     value exists only to mark that unsupported scope and is rejected.

#ifndef NEARGYE_MAGIC_ENUM_EXPLICIT_NAMES_HPP
#define NEARGYE_MAGIC_ENUM_EXPLICIT_NAMES_HPP

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>

namespace magic_enum::explicit_names {

// Case folding policy selected when an external name table is built.
enum class case_policy {
  case_sensitive,            // Exact, byte-for-byte comparison.
  case_insensitive_ascii,    // A-Z folded onto a-z; all other bytes unchanged.
  unicode_case_insensitive,  // Reserved: full Unicode folding is not supported.
};

// Declares the Unicode support scope of this facility.
//
// Only ASCII case conversion (the 26 letters A-Z <-> a-z) is applied.
// Non-ASCII characters are matched exactly as encoded; no full Unicode case
// folding, normalization or locale-aware comparison is performed.
enum class unicode_support {
  ascii_fold_only,  // The one supported scope.
  full_folding,     // Reserved: not supported and never reported as supported.
};

inline constexpr unicode_support unicode_support_scope = unicode_support::ascii_fold_only;
inline constexpr bool full_unicode_case_folding_supported = false;

// Outcome of a parse attempt. Unknown input and an illegal policy are reported
// separately instead of silently falling back to a value (such as the
// zero-valued enumerator).
enum class parse_status {
  ok,             // A canonical name matched.
  unknown_name,   // No canonical name matched the input.
  invalid_policy, // The requested policy is illegal for this table or unknown.
};

template <typename E>
struct parse_result {
  std::optional<E> value;
  parse_status status = parse_status::unknown_name;

  constexpr explicit operator bool() const noexcept { return status == parse_status::ok; }
  [[nodiscard]] constexpr bool has_value() const noexcept { return status == parse_status::ok; }
  constexpr E operator*() const { return *value; }
};

// One user supplied association between an external spelling and an enum value.
template <typename E>
struct name_entry {
  std::string_view name;
  E value;
};

namespace detail {

inline constexpr std::size_t no_index = static_cast<std::size_t>(-1);

constexpr char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

template <case_policy Policy>
constexpr bool eq_char(char table_char, char input_char) noexcept {
  if constexpr (Policy == case_policy::case_insensitive_ascii) {
    return ascii_lower(table_char) == ascii_lower(input_char);
  } else {
    return table_char == input_char;
  }
}

template <case_policy Policy>
constexpr bool less_name(std::string_view lhs, std::string_view rhs) noexcept {
  const std::size_t n = lhs.size() < rhs.size() ? lhs.size() : rhs.size();
  for (std::size_t i = 0; i < n; ++i) {
    const char l = (Policy == case_policy::case_insensitive_ascii) ? ascii_lower(lhs[i]) : lhs[i];
    const char r = (Policy == case_policy::case_insensitive_ascii) ? ascii_lower(rhs[i]) : rhs[i];
    if (l != r) {
      return l < r;
    }
  }
  return lhs.size() < rhs.size();
}

template <case_policy Policy>
constexpr bool equal_name(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (!eq_char<Policy>(lhs[i], rhs[i])) {
      return false;
    }
  }
  return true;
}

// Result of the constexpr precheck.
struct table_check {
  bool valid = false;
  const char* message = "";
};

constexpr table_check check_fail(const char* message) noexcept { return {false, message}; }
constexpr table_check check_ok() noexcept { return {true, ""}; }

template <case_policy Policy>
constexpr table_check validate_policy() noexcept {
  if constexpr (Policy == case_policy::unicode_case_insensitive) {
    return check_fail(
        "magic_enum::explicit_names: unicode_case_insensitive is not supported, "
        "only ASCII case folding is provided and full Unicode case folding is not claimed");
  } else if constexpr (Policy != case_policy::case_sensitive &&
                       Policy != case_policy::case_insensitive_ascii) {
    return check_fail("magic_enum::explicit_names: unknown case_policy");
  } else {
    return check_ok();
  }
}

template <typename E, std::size_t N>
constexpr bool has_empty_name(const name_entry<E> (&entries)[N]) noexcept {
  for (std::size_t i = 0; i < N; ++i) {
    if (entries[i].name.empty()) {
      return true;
    }
  }
  return false;
}

// True when a canonical-name group spans different enum values.
template <case_policy Policy, typename E, std::size_t N>
constexpr bool has_value_collision(const name_entry<E> (&entries)[N]) noexcept {
  for (std::size_t i = 0; i < N; ++i) {
    for (std::size_t j = i + 1; j < N; ++j) {
      if (!less_name<Policy>(entries[i].name, entries[j].name) &&
          !less_name<Policy>(entries[j].name, entries[i].name) &&
          !(entries[i].value == entries[j].value)) {
        return true;
      }
    }
  }
  return false;
}

// Public constexpr precheck, usable in static_assert and at runtime: returns a
// structured result rather than triggering an assertion itself.
template <case_policy Policy, typename E, std::size_t N>
constexpr table_check validate(const name_entry<E> (&entries)[N]) noexcept {
  constexpr table_check policy_check = validate_policy<Policy>();
  if constexpr (!policy_check.valid) {
    return policy_check;
  } else if (has_empty_name<E, N>(entries)) {
    return check_fail("magic_enum::explicit_names: empty external name is not allowed");
  } else if (has_value_collision<Policy, E, N>(entries)) {
    return check_fail(
        "magic_enum::explicit_names: the same canonical name must not point to different enum values");
  }
  return check_ok();
}

constexpr void add_source(std::string_view* sources, std::size_t& count, std::size_t capacity,
                          std::string_view name) noexcept {
  for (std::size_t s = 0; s < count; ++s) {
    if (sources[s] == name) {
      return;
    }
  }
  if (count < capacity) {
    sources[count++] = name;
  }
}

} // namespace detail

// A compile-time, read-only lookup table. Obtain instances with make_name_table
// (or the MAGIC_ENUM_EXPLICIT_NAME_TABLE macro).
template <typename E, std::size_t Capacity, case_policy Policy = case_policy::case_sensitive>
class name_table {
  static_assert(std::is_enum_v<E>, "magic_enum::explicit_names requires an enum type.");

  static constexpr std::size_t max_sources = 4;

  struct row {
    std::string_view canonical;
    E value{};
    std::size_t source_count = 0;
    std::array<std::string_view, max_sources> sources{};
  };

  std::array<row, Capacity> rows_{};
  std::size_t size_ = 0;

 public:
  constexpr name_table() noexcept = default;

  // Original spellings merged into one row (aliases sharing one value).
  struct source_list {
    const std::string_view* data = nullptr;
    std::size_t size = 0;

    constexpr const std::string_view* begin() const noexcept { return data; }
    constexpr const std::string_view* end() const noexcept { return data + size; }
    constexpr std::string_view operator[](std::size_t i) const { return data[i]; }
  };

  [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }
  [[nodiscard]] constexpr std::string_view canonical_name(std::size_t index) const {
    return rows_[index].canonical;
  }
  [[nodiscard]] constexpr E value_at(std::size_t index) const { return rows_[index].value; }
  [[nodiscard]] constexpr source_list sources_of(std::size_t index) const {
    return {rows_[index].sources.data(), rows_[index].source_count};
  }

  // All original spellings that resolve to the given enum value.
  [[nodiscard]] constexpr std::array<std::string_view, max_sources> sources_of_value(
      E value, std::size_t& out_count) const {
    std::array<std::string_view, max_sources> result{};
    out_count = 0;
    for (std::size_t r = 0; r < size_; ++r) {
      if (rows_[r].value == value) {
        for (std::size_t s = 0; s < rows_[r].source_count && out_count < max_sources; ++s) {
          detail::add_source(result.data(), out_count, result.size(), rows_[r].sources[s]);
        }
      }
    }
    return result;
  }

  // Build the sorted table at compile time, merging equal canonical entries.
  template <std::size_t N>
  static constexpr name_table create(const name_entry<E> (&entries)[N]) noexcept {
    name_table table{};
    std::size_t next = 0;

    for (std::size_t i = 0; i < N; ++i) {
      std::size_t found = detail::no_index;
      for (std::size_t r = 0; r < next; ++r) {
        if (!detail::less_name<Policy>(table.rows_[r].canonical, entries[i].name) &&
            !detail::less_name<Policy>(entries[i].name, table.rows_[r].canonical)) {
          found = r;
          break;
        }
      }
      if (found == detail::no_index) {
        table.rows_[next].canonical = entries[i].name;
        table.rows_[next].value = entries[i].value;
        table.rows_[next].sources[0] = entries[i].name;
        table.rows_[next].source_count = 1;
        ++next;
      } else {
        detail::add_source(table.rows_[found].sources.data(), table.rows_[found].source_count,
                           table.rows_[found].sources.size(), entries[i].name);
      }
    }

    // Constexpr insertion sort; parsing below is a binary search over it.
    for (std::size_t i = 1; i < next; ++i) {
      std::size_t j = i;
      while (j > 0 && detail::less_name<Policy>(table.rows_[j].canonical, table.rows_[j - 1].canonical)) {
        const row tmp = table.rows_[j];
        table.rows_[j] = table.rows_[j - 1];
        table.rows_[j - 1] = tmp;
        --j;
      }
    }
    table.size_ = next;
    return table;
  }
};

// Constexpr precheck invoked while building a table. In C++17 a constexpr
// function parameter is not itself a constant expression, so entry-dependent
// checks cannot be written as in-body static_asserts; evaluating a throw in a
// constant context makes the constexpr initialization ill-formed. The
// MAGIC_ENUM_EXPLICIT_NAME_TABLE macro additionally fires literal-message
// static_asserts at the call site.
template <case_policy Policy, typename E, std::size_t N>
constexpr void check_entries(const name_entry<E> (&entries)[N]) {
  if (detail::has_empty_name<E, N>(entries)) {
    // Empty external names are rejected.
    throw "magic_enum::explicit_names: empty external name is not allowed";
  }
  if (detail::has_value_collision<Policy, E, N>(entries)) {
    // The same canonical name cannot point to different enum values.
    throw "magic_enum::explicit_names: the same canonical name must not point to different enum values";
  }
}

// Compile-time factory: runs the constexpr precheck and then generates the
// sorted lookup table. When used to initialize a constexpr variable, an
// invalid table is rejected at compile time.
template <case_policy Policy = case_policy::case_sensitive, typename E, std::size_t N>
constexpr auto make_name_table(const name_entry<E> (&entries)[N]) {
  static_assert(detail::validate_policy<Policy>().valid,
                "magic_enum::explicit_names: unsupported case_policy (full Unicode case folding is "
                "not provided or claimed; use case_sensitive or case_insensitive_ascii)");
  check_entries<Policy, E, N>(entries);
  return name_table<E, N, Policy>::template create<N>(entries);
}

// Indexed binary search; never linearly rescans the rows. Sets index on a hit.
template <typename E, std::size_t Capacity, case_policy Policy>
constexpr std::optional<E> find_index(const name_table<E, Capacity, Policy>& table,
                                      std::size_t& index,
                                      std::string_view input) noexcept {
  std::size_t lo = 0;
  std::size_t hi = table.size();
  while (lo < hi) {
    const std::size_t mid = lo + (hi - lo) / 2;
    if (detail::less_name<Policy>(table.canonical_name(mid), input)) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  if (lo < table.size() && detail::equal_name<Policy>(table.canonical_name(lo), input)) {
    index = lo;
    return table.value_at(lo);
  }
  index = detail::no_index;
  return std::nullopt;
}

// Compile-time policy parse using the table's own policy; a non-matching input
// is an unknown name.
template <typename E, std::size_t Capacity, case_policy Policy>
constexpr parse_result<E> parse(const name_table<E, Capacity, Policy>& table,
                                std::string_view input) noexcept {
  std::size_t index = detail::no_index;
  if (auto value = find_index(table, index, input)) {
    return {*value, parse_status::ok};
  }
  return {std::nullopt, parse_status::unknown_name};
}

// Compile-time parse with an explicitly requested policy. Asking for a policy
// different from the one the table was built with (including the unsupported
// Unicode scope) is reported as invalid.
template <case_policy Requested, typename E, std::size_t Capacity, case_policy Policy>
constexpr parse_result<E> parse(const name_table<E, Capacity, Policy>& table,
                                std::string_view input) noexcept {
  if constexpr (Requested != Policy) {
    return {std::nullopt, parse_status::invalid_policy};
  } else {
    return parse(table, input);
  }
}

// Runtime policy parse.
template <typename E, std::size_t Capacity, case_policy Policy>
constexpr parse_result<E> parse_with_policy(const name_table<E, Capacity, Policy>& table,
                                            std::string_view input,
                                            case_policy requested) noexcept {
  if (requested != Policy) {
    return {std::nullopt, parse_status::invalid_policy};
  }
  std::size_t index = detail::no_index;
  if (auto value = find_index(table, index, input)) {
    return {*value, parse_status::ok};
  }
  return {std::nullopt, parse_status::unknown_name};
}

} // namespace magic_enum::explicit_names

// Convenience declaration:
//
//   MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, color_names, case_sensitive,
//       {{"RED", Color::RED}, {"GREEN", Color::GREEN}, {"BLUE", Color::BLUE}});
//
// The table is constexpr-validated; an invalid table is rejected at compile
// time. policy is an unqualified magic_enum::explicit_names::case_policy name.
#define MAGIC_ENUM_EXPLICIT_NAME_TABLE(enum_type, variable, policy, ...)                            \
  static constexpr ::magic_enum::explicit_names::name_entry<enum_type> variable##_entries_[] =      \
      __VA_ARGS__;                                                                                  \
  static_assert(                                                                                    \
      ::magic_enum::explicit_names::detail::validate_policy<                                        \
          ::magic_enum::explicit_names::case_policy::policy>()                                      \
          .valid,                                                                                   \
      "magic_enum::explicit_names: unsupported case_policy (full Unicode case folding is not "      \
      "provided or claimed; use case_sensitive or case_insensitive_ascii)");                        \
  static_assert(                                                                                    \
      !::magic_enum::explicit_names::detail::has_empty_name<enum_type>(variable##_entries_),        \
      "magic_enum::explicit_names: empty external name is not allowed");                            \
  static_assert(                                                                                    \
      !::magic_enum::explicit_names::detail::has_value_collision<                                   \
          ::magic_enum::explicit_names::case_policy::policy, enum_type>(variable##_entries_),       \
      "magic_enum::explicit_names: the same canonical name must not point to different enum "       \
      "values");                                                                                    \
  static constexpr auto variable = ::magic_enum::explicit_names::make_name_table<                  \
      ::magic_enum::explicit_names::case_policy::policy>(variable##_entries_)

#endif // NEARGYE_MAGIC_ENUM_EXPLICIT_NAMES_HPP
