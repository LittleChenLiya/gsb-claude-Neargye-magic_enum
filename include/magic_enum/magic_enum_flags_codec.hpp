//  __  __             _        ______                          _____
// |  \/  |           (_)      |  ____|                        / ____|_     _
// | \  / | __ _  __ _ _  ___  | |__   _ __  _   _ _ __ ___   | |   _| |_ _| |_
// | |\/| |/ _` |/ _` | |/ __| |  __| | '_ \| | | | '_ ` _ \  | |  |_   _|_   _|
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

#ifndef NEARGYE_MAGIC_ENUM_FLAGS_CODEC_HPP
#define NEARGYE_MAGIC_ENUM_FLAGS_CODEC_HPP

#include "magic_enum.hpp"

#include <limits>
#include <type_traits>

namespace magic_enum {

// Explicit encode/decode plan for flag enums.
//
// The plan is explicit about the parts enum_flags_name/enum_flags_cast leave
// implicit:
//  * Known combinations map to enumerator names; bits are never dropped.
//  * Bits without a name are handled by a compile-time policy: they are either
//    rejected (encoding/decoding fails) or preserved as the original integer,
//    so a round-trip always keeps every bit.
//  * The zero combination is a valid value of its own, distinct from "no
//    name": it encodes to the declared zero enumerator (or to an empty, but
//    valid, name) and never reports unknown bits.
//  * Aliases have a fixed display strategy: the first declared enumerator is
//    canonical and a bit is never emitted twice.
//  * Signed underlying types are range-checked before their bits are read as
//    unsigned; negative values are handled according to the policy.
//
// This header is additive: it does not change enum_name or any other
// existing API.
namespace flags_codec {

// Compile-time policy for bits that have no enumerator name.
enum class unknown_bits : unsigned char {
  reject,   // Encoding or decoding fails when unnamed bits are present.
  preserve, // Unnamed bits are kept as a decimal integer token, so no bit is lost.
};

} // namespace magic_enum::flags_codec

namespace detail {

// Appends the base-10 representation of an unsigned value.
template <typename U>
inline void flags_codec_append_uint(string& out, U value) {
  static_assert(std::is_unsigned_v<U>, "magic_enum::flags_codec requires an unsigned integer.");
  char_type digits[std::numeric_limits<U>::digits10 + 1];
  std::size_t count = 0;
  do {
    digits[count++] = static_cast<char_type>(char_type{'0'} + static_cast<int>(value % 10));
    value = static_cast<U>(value / 10);
  } while (value != U{0});
  while (count > 0) {
    out.append(1, digits[--count]);
  }
}

// Parses a base-10 integer token into the unsigned bit pattern of the
// underlying type T. The token is range-checked against T before the bits are
// produced: a negative token never fits an unsigned underlying type, and the
// magnitude must fit the signed/unsigned range of T.
template <typename T, typename U>
constexpr bool flags_codec_parse_integer(string_view token, U& out) noexcept {
  static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>, "magic_enum::flags_codec requires an integral underlying type.");
  static_assert(std::is_unsigned_v<U>, "magic_enum::flags_codec requires an unsigned integer.");
  std::size_t pos = 0;
  auto negative = false;
  if (token[pos] == char_type{'-'}) {
    if constexpr (!std::is_signed_v<T>) {
      return false; // Range check first: a negative integer never fits an unsigned underlying type.
    }
    negative = true;
    if (++pos == token.size()) {
      return false; // A lone sign is not an integer.
    }
  }
  auto magnitude = U{0};
  for (; pos < token.size(); ++pos) {
    const auto c = token[pos];
    if (c < char_type{'0'} || c > char_type{'9'}) {
      return false; // Not a decimal integer token.
    }
    const auto digit = static_cast<U>(c - char_type{'0'});
    if (magnitude > (std::numeric_limits<U>::max() - digit) / U{10}) {
      return false; // Overflow: the token does not fit the underlying type.
    }
    magnitude = static_cast<U>(magnitude * 10 + digit);
  }
  if (negative) {
    constexpr auto limit = static_cast<U>(std::numeric_limits<T>::max()) + U{1};
    if (magnitude > limit) {
      return false; // Range check against the signed underlying type.
    }
    out = static_cast<U>(U{0} - magnitude);
  } else {
    if (magnitude > static_cast<U>(std::numeric_limits<T>::max())) {
      return false; // Range check against the underlying type.
    }
    out = magnitude;
  }
  return true;
}

// Appends the canonical names of every single-bit enumerator fully contained
// in `bits`. Fixed display strategy: enumerators are visited in declaration
// order, the first declaration of a bit wins, and a bit is emitted at most
// once, so aliases never produce the same bit twice. Returns the covered bits.
template <typename E, typename U>
inline auto flags_codec_append_names(string& out, U bits, char_type sep) -> U {
  using D = std::decay_t<E>;
  constexpr auto S = enum_subtype::flags;
  auto covered = U{0};
  for (std::size_t i = 0; i < count_v<D, S>; ++i) {
    if (const auto v = static_cast<U>(values_v<D, S>[i]); v != U{0} && (bits & v) == v && (covered & v) != v) {
      if (!out.empty()) {
        out.append(1, sep);
      }
      const auto n = names_v<D, S>[i];
      out.append(n.data(), n.size());
      covered = static_cast<U>(covered | v);
    }
  }
  return covered;
}

// Returns the name of the first declared enumerator whose value equals
// `bits`, or an empty string_view when no enumerator matches. The common
// subtype is queried because it reflects every declared enumerator, including
// the zero enumerator and named combinations that the flags subtype omits.
template <typename E, typename U>
constexpr auto flags_codec_find_exact(U bits) noexcept -> string_view {
  using D = std::decay_t<E>;
  constexpr auto S = enum_subtype::common;
  for (std::size_t i = 0; i < count_v<D, S>; ++i) {
    if (static_cast<U>(values_v<D, S>[i]) == bits) {
      return names_v<D, S>[i];
    }
  }
  return {};
}

} // namespace magic_enum::detail

namespace flags_codec {

// Encodes a flag enum value to its explicit name.
// Returns optional containing the name; an engaged empty string is the valid
// encoding of the zero combination when no zero enumerator is declared.
// Returns an empty optional when unnamed bits are present and the policy is
// unknown_bits::reject.
template <unknown_bits Policy = unknown_bits::reject, typename E>
[[nodiscard]] auto encode(E value, char_type sep = char_type{'|'}) -> detail::enable_if_t<E, optional<string>> {
  using D = std::decay_t<E>;
  using T = underlying_type_t<D>;
  using U = detail::make_unsigned_t<T>;
  constexpr auto S = detail::enum_subtype::flags;
  static_assert(detail::is_reflected_v<D, S>, "magic_enum::flags_codec requires enum implementation and valid max and min.");

  const auto bits = static_cast<U>(value);
  if (bits == U{0}) {
    // The zero combination is valid and never reported as unknown: use the
    // declared zero enumerator when present, otherwise an empty (but valid)
    // name, which stays distinct from the disengaged "no name" result.
    string name;
    if (const auto zero = detail::flags_codec_find_exact<D>(bits); !zero.empty()) {
      name.append(zero.data(), zero.size());
    }
    return name;
  }

  // A known combination maps to its own declared name; the first declaration
  // of a value wins, so aliases never produce the same bit pattern twice.
  if (const auto exact = detail::flags_codec_find_exact<D>(bits); !exact.empty()) {
    string name;
    name.append(exact.data(), exact.size());
    return name;
  }

  string name;
  const auto covered = detail::flags_codec_append_names<D>(name, bits, sep);
  const auto remainder = static_cast<U>(bits & ~covered);
  if (remainder == U{0}) {
    return name; // Fully known combination, mapped to enumerator names.
  }

  if constexpr (Policy == unknown_bits::reject) {
    return {}; // Unnamed bits are rejected.
  } else {
    if constexpr (std::is_signed_v<T>) {
      // Range check first for signed underlying types: a negative value is
      // preserved as its original signed integer, keeping every bit.
      if (bits > static_cast<U>(std::numeric_limits<T>::max())) {
        string negative;
        negative.append(1, char_type{'-'});
        detail::flags_codec_append_uint(negative, static_cast<U>(U{0} - bits));
        return negative;
      }
    }
    // Preserve the unnamed bits as their original integer; nothing is dropped.
    if (!name.empty()) {
      name.append(1, sep);
    }
    detail::flags_codec_append_uint(name, remainder);
    return name;
  }
}

// Decodes an explicit flag name back to the flag enum value.
// Returns optional containing the flag enum value.
// The empty string decodes to the zero combination. Integer tokens produced
// by encode with unknown_bits::preserve are accepted only under the same
// policy and are range-checked against the underlying type, so a round-trip
// preserves every bit.
template <unknown_bits Policy = unknown_bits::reject, typename E>
[[nodiscard]] constexpr auto decode(string_view text, char_type sep = char_type{'|'}) -> detail::enable_if_t<E, optional<std::decay_t<E>>> {
  using D = std::decay_t<E>;
  using T = underlying_type_t<D>;
  using U = detail::make_unsigned_t<T>;
  constexpr auto S = detail::enum_subtype::flags;
  static_assert(detail::is_reflected_v<D, S>, "magic_enum::flags_codec requires enum implementation and valid max and min.");

  if (text.empty()) {
    return D{0}; // The zero combination is valid and never reported as unknown.
  }

  auto result = U{0};
  for (std::size_t first = 0; first < text.size();) {
    auto last = first;
    while (last < text.size() && text[last] != sep) {
      ++last;
    }
    const auto token = text.substr(first, last - first);
    if (token.empty()) {
      return {}; // An empty token (leading, trailing or doubled separator) is invalid.
    }

    auto matched = false;
    // Named enumerators first: the common subtype reflects the zero
    // enumerator and named combinations, the flags subtype reflects every
    // single-bit flag. A name is matched at most once per token.
    for (std::size_t i = 0; !matched && i < detail::count_v<D, detail::enum_subtype::common>; ++i) {
      if (detail::cmp_equal(token, detail::names_v<D, detail::enum_subtype::common>[i], std::equal_to<>{})) {
        result = static_cast<U>(result | static_cast<U>(detail::values_v<D, detail::enum_subtype::common>[i]));
        matched = true;
      }
    }
    for (std::size_t i = 0; !matched && i < detail::count_v<D, S>; ++i) {
      if (detail::cmp_equal(token, detail::names_v<D, S>[i], std::equal_to<>{})) {
        result = static_cast<U>(result | static_cast<U>(detail::values_v<D, S>[i]));
        matched = true;
      }
    }
    if (!matched) {
      if constexpr (Policy == unknown_bits::reject) {
        return {}; // Unnamed bits are rejected.
      } else {
        auto bits = U{0};
        if (!detail::flags_codec_parse_integer<T>(token, bits)) {
          return {}; // Not a name and not an in-range integer token.
        }
        result = static_cast<U>(result | bits);
      }
    }
    first = (last < text.size()) ? last + 1 : last;
  }

  return static_cast<D>(result);
}

} // namespace magic_enum::flags_codec

} // namespace magic_enum

#endif // NEARGYE_MAGIC_ENUM_FLAGS_CODEC_HPP
