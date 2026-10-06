//  __  __             _        ______                          _____
// |  \/  |           (_)      |  ____|                        / ____|_     _
// | \  / | __ _  __ _ _  ___  | |__   _ __  _   _ _ __ ___   | |   _| |_ _| |_
// | |\/| |/ _` |/ _` | |/ __| |  __| | ._ \| | | | ._ ` _ \  | |  |_   _|_   _|
// | |  | | (_| | (_| | | (__  | |____| | | | |_| | | | | | | | |____|_|   |_|
// |_|  |_|\__,_|\__, |_|\___| |______|_| |_|\__,_|_| |_| |_|  \_____|
//                __/ | https://github.com/Neargye/magic_enum
//               |___/  version 0.9.8
//
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.
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
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef NEARGYE_MAGIC_ENUM_EXTNAME_HPP
#define NEARGYE_MAGIC_ENUM_EXTNAME_HPP

#include "magic_enum.hpp"

#ifndef MAGIC_ENUM_USE_STD_MODULE
#  include <array>
#  include <cstddef>
#endif

// Constexpr pre-checks of external name tables are enforced at compile time.
// With C++20 consteval the table is always built during compilation; on C++17
// the checks fire whenever the table is used in a constant expression.
#if defined(__cpp_consteval) && __cpp_consteval >= 201811L
#  define MAGIC_ENUM_EXTNAME_CONSTEVAL consteval
#else
#  define MAGIC_ENUM_EXTNAME_CONSTEVAL constexpr
#endif

namespace magic_enum {

// Explicit external name tables with constexpr pre-checks, a declared
// case-folding policy and a compile-time generated sorted lookup table.
// This facility is independent from the standard magic_enum enum-name
// mapping, which is left unchanged.
namespace extname {

// Case-folding policy of an external name table.
enum class case_policy : int {
  sensitive = 0,        // Names match byte-for-byte.
  insensitive_ascii = 1 // Only ASCII letters A-Z/a-z are folded.
};

// Declared Unicode support range of case-insensitive matching.
// Case folding covers ASCII letters A-Z/a-z only. Every non-ASCII character
// is compared byte-wise; full Unicode case folding is NOT supported and is
// NOT claimed. Non-ASCII names are still allowed, they just never fold.
inline constexpr string_view unicode_support_range = "ASCII letters A-Z/a-z only; non-ASCII characters are compared byte-wise, full Unicode case folding is not supported";

// Distinct parse outcomes: unknown input and invalid policy are reported separately.
enum class parse_error : int {
  none = 0,          // Name found, value is valid.
  unknown_name = 1,  // Name not present in the table; never falls back to a zero value.
  invalid_policy = 2 // The supplied case policy is not a valid case_policy enumerator.
};

// One source entry of an external name table.
template <typename E>
struct entry {
  string_view name;
  E value;
};

// One canonical row of the compiled lookup table.
// Same-value aliases merge their sources into a single row.
template <typename E>
struct merged_entry {
  string_view name;    // Representative spelling (first source entry).
  E value;             // Enum value shared by every merged source.
  std::size_t sources; // Number of source entries merged into this row.
};

// Result of extname::parse; the error channel keeps unknown names and
// invalid policies apart instead of falling back to a zero enum value.
template <typename E>
struct parse_result {
  E value;
  parse_error error;

  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return error == parse_error::none;
  }
};

namespace detail {

constexpr bool extname_valid_policy(case_policy policy) noexcept {
  return policy == case_policy::sensitive || policy == case_policy::insensitive_ascii;
}

constexpr char_type extname_ascii_lower(char_type c) noexcept {
  return c >= static_cast<char_type>('A') && c <= static_cast<char_type>('Z')
             ? static_cast<char_type>(c + (static_cast<char_type>('a') - static_cast<char_type>('A')))
             : c;
}

// Three-way comparison on canonical (policy-folded) spellings.
constexpr int extname_canonical_compare(string_view lhs, string_view rhs, case_policy policy) noexcept {
  const auto lhs_size = lhs.size();
  const auto rhs_size = rhs.size();
  const auto common = lhs_size < rhs_size ? lhs_size : rhs_size;
  for (std::size_t i = 0; i < common; ++i) {
    auto lc = lhs[i];
    auto rc = rhs[i];
    if (policy == case_policy::insensitive_ascii) {
      lc = extname_ascii_lower(lc);
      rc = extname_ascii_lower(rc);
    }
    if (lc < rc) {
      return -1;
    }
    if (rc < lc) {
      return 1;
    }
  }
  if (lhs_size < rhs_size) {
    return -1;
  }
  if (lhs_size > rhs_size) {
    return 1;
  }
  return 0;
}

} // namespace detail

// Compile-time generated lookup table over an explicit external name list.
// Rows are canonicalized, merged and sorted when the table is built, so
// lookups binary-search and never linearly re-enumerate the source entries.
template <typename E, std::size_t N>
class table {
  static_assert(std::is_enum_v<E>, "magic_enum::extname::table requires an enum type.");
  static_assert(N > 0, "magic_enum::extname::table requires at least one entry.");

 public:
  using enum_type = E;
  static constexpr std::size_t capacity = N;

  // Constexpr pre-check of the external name table:
  // * the case policy must be valid;
  // * names must not be empty;
  // * one canonical name must not map to different enum values (rejected);
  // * same-value aliases of one canonical name merge their sources;
  // * rows are sorted by canonical name for binary-search lookup.
  MAGIC_ENUM_EXTNAME_CONSTEVAL table(case_policy policy, const entry<E> (&entries)[N]) : policy_{policy} {
    if (!detail::extname_valid_policy(policy)) {
      throw "magic_enum::extname: invalid case policy";
    }
    std::size_t count = 0;
    for (std::size_t i = 0; i < N; ++i) {
      const auto& e = entries[i];
      if (e.name.empty()) {
        throw "magic_enum::extname: external name must not be empty";
      }
      bool merged = false;
      for (std::size_t j = 0; j < count; ++j) {
        if (detail::extname_canonical_compare(rows_[j].name, e.name, policy_) == 0) {
          if (rows_[j].value != e.value) {
            throw "magic_enum::extname: canonical name maps to different enum values";
          }
          ++rows_[j].sources; // Same-value aliases merge their sources.
          merged = true;
          break;
        }
      }
      if (!merged) {
        rows_[count++] = merged_entry<E>{e.name, e.value, 1};
      }
    }
    // Insertion sort by canonical name, so find() can binary-search.
    for (std::size_t i = 1; i < count; ++i) {
      const auto row = rows_[i];
      std::size_t j = i;
      while (j > 0 && detail::extname_canonical_compare(row.name, rows_[j - 1].name, policy_) < 0) {
        rows_[j] = rows_[j - 1];
        --j;
      }
      rows_[j] = row;
    }
    size_ = count;
  }

  [[nodiscard]] constexpr case_policy policy() const noexcept {
    return policy_;
  }

  // Number of canonical rows after merging same-value aliases.
  [[nodiscard]] constexpr std::size_t size() const noexcept {
    return size_;
  }

  [[nodiscard]] constexpr const merged_entry<E>* begin() const noexcept {
    return rows_.data();
  }

  [[nodiscard]] constexpr const merged_entry<E>* end() const noexcept {
    return rows_.data() + size_;
  }

  [[nodiscard]] constexpr const merged_entry<E>& operator[](std::size_t index) const noexcept {
    return rows_[index];
  }

  // Binary search over the compile-time sorted rows; never linearly
  // re-enumerates the table. Returns nullptr for an invalid policy or a miss.
  [[nodiscard]] constexpr const merged_entry<E>* find(string_view name, case_policy policy) const noexcept {
    if (!detail::extname_valid_policy(policy)) {
      return nullptr;
    }
    std::size_t lo = 0;
    std::size_t hi = size_;
    while (lo < hi) {
      const std::size_t mid = lo + (hi - lo) / 2;
      const int cmp = detail::extname_canonical_compare(rows_[mid].name, name, policy);
      if (cmp < 0) {
        lo = mid + 1;
      } else if (cmp > 0) {
        hi = mid;
      } else {
        return &rows_[mid];
      }
    }
    return nullptr;
  }

  // Binary search with the table's own policy.
  [[nodiscard]] constexpr const merged_entry<E>* find(string_view name) const noexcept {
    return find(name, policy_);
  }

 private:
  std::array<merged_entry<E>, N> rows_{};
  std::size_t size_{};
  case_policy policy_{};
};

// Builds a compile-time checked lookup table from explicit external names.
template <typename E, std::size_t N>
[[nodiscard]] MAGIC_ENUM_EXTNAME_CONSTEVAL table<E, N> make_table(case_policy policy, const entry<E> (&entries)[N]) {
  return table<E, N>{policy, entries};
}

// Variadic overload: make_table<E>(policy, entry<E>{...}, entry<E>{...}, ...).
template <typename E, typename... Entries, std::enable_if_t<(sizeof...(Entries) > 0) && (std::is_same_v<Entries, entry<E>> && ...), int> = 0>
[[nodiscard]] MAGIC_ENUM_EXTNAME_CONSTEVAL table<E, sizeof...(Entries)> make_table(case_policy policy, Entries... entries) {
  const entry<E> arr[sizeof...(Entries)] = {entries...};
  return table<E, sizeof...(Entries)>{policy, arr};
}

// Parses a name with an explicit policy. Unknown names and invalid policies
// are reported as distinct errors; neither falls back to a zero enum value.
template <typename E, std::size_t N>
[[nodiscard]] constexpr parse_result<E> parse(const table<E, N>& t, string_view name, case_policy policy) noexcept {
  if (!detail::extname_valid_policy(policy)) {
    return parse_result<E>{E{}, parse_error::invalid_policy};
  }
  if (const auto* row = t.find(name, policy)) {
    return parse_result<E>{row->value, parse_error::none};
  }
  return parse_result<E>{E{}, parse_error::unknown_name};
}

// Parses a name with the table's own policy.
template <typename E, std::size_t N>
[[nodiscard]] constexpr parse_result<E> parse(const table<E, N>& t, string_view name) noexcept {
  return parse(t, name, t.policy());
}

} // namespace extname

} // namespace magic_enum

#undef MAGIC_ENUM_EXTNAME_CONSTEVAL

#endif // NEARGYE_MAGIC_ENUM_EXTNAME_HPP
