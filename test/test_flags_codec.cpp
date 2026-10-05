// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.

#include <new>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_flags_codec.hpp>

#include <cstdint>
#include <string>

// Flags enum with a zero enumerator, single-bit flags and a named combination.
enum class Perm : std::uint32_t {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Execute = 1 << 2,
  ReadWrite = Read | Write,
};
template <>
struct magic_enum::customize::enum_range<Perm> {
  static constexpr bool is_flags = true;
};

// Flags enum with alias enumerators sharing the same bits.
enum class Access : std::uint32_t {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
#if defined(MAGIC_ENUM_SUPPORTED_ALIASES)
  ReadAlso = Read,
  WriteAlso = Write,
#endif
};
template <>
struct magic_enum::customize::enum_range<Access> {
  static constexpr bool is_flags = true;
};

// Flags enum with a signed underlying type.
enum class Small : std::int8_t {
  None = 0,
  Low = 1 << 0,
  High = 1 << 1,
};
template <>
struct magic_enum::customize::enum_range<Small> {
  static constexpr bool is_flags = true;
};

// Flags enum without a zero enumerator.
enum class NoZero : std::uint32_t {
  Alpha = 1 << 0,
  Beta = 1 << 1,
};
template <>
struct magic_enum::customize::enum_range<NoZero> {
  static constexpr bool is_flags = true;
};

// Plain enum, not a flags enum: enum_name must stay untouched.
enum class Plain { alpha, beta };

namespace codec = magic_enum::flags_codec;
using magic_enum::flags_codec::unknown_bits;

TEST_CASE("known combination round-trip") {
  // The named combination maps to its own declared name.
  const auto combo = codec::encode(static_cast<Perm>(1 | 2));
  REQUIRE(combo.has_value());
  REQUIRE(*combo == "ReadWrite");

  // A combination without a declared name decomposes into flag names.
  const auto trio = codec::encode(static_cast<Perm>(1 | 2 | 4));
  REQUIRE(trio.has_value());
  REQUIRE(*trio == "Read|Write|Execute");

  // Both forms decode back to exactly the same bits.
  constexpr auto decoded_combo = codec::decode<unknown_bits::reject, Perm>("ReadWrite");
  static_assert(decoded_combo.has_value());
  static_assert(decoded_combo.value() == static_cast<Perm>(1 | 2));
  constexpr auto decoded_trio = codec::decode<unknown_bits::reject, Perm>("Read|Write|Execute");
  static_assert(decoded_trio.has_value());
  static_assert(decoded_trio.value() == static_cast<Perm>(1 | 2 | 4));

  const auto round_trip = codec::decode<unknown_bits::reject, Perm>(*trio);
  REQUIRE(round_trip.has_value());
  REQUIRE(*round_trip == static_cast<Perm>(1 | 2 | 4));
}

TEST_CASE("unknown bits rejected or preserved, never dropped") {
  // Reject policy: encoding and decoding fail on unnamed bits.
  REQUIRE_FALSE(codec::encode(static_cast<Perm>(8)).has_value());
  REQUIRE_FALSE(codec::encode<unknown_bits::reject>(static_cast<Perm>(8)).has_value());
  constexpr auto rejected = codec::decode<unknown_bits::reject, Perm>("8");
  static_assert(!(rejected.has_value()));

  // Preserve policy: the unnamed bit is kept as the original integer.
  const auto preserved = codec::encode<unknown_bits::preserve>(static_cast<Perm>(8));
  REQUIRE(preserved.has_value());
  REQUIRE(*preserved == "8");

  // Known names and preserved integers mix in one encoding.
  const auto mixed = codec::encode<unknown_bits::preserve>(static_cast<Perm>(1 | 2 | 8));
  REQUIRE(mixed.has_value());
  REQUIRE(*mixed == "Read|Write|8");

  // Round-trip preserves every bit, including the unnamed one.
  constexpr auto back = codec::decode<unknown_bits::preserve, Perm>("Read|Write|8");
  static_assert(back.has_value());
  static_assert(back.value() == static_cast<Perm>(1 | 2 | 8));
  const auto round_trip = codec::decode<unknown_bits::preserve, Perm>(*mixed);
  REQUIRE(round_trip.has_value());
  REQUIRE(*round_trip == static_cast<Perm>(1 | 2 | 8));

  // Garbage tokens are rejected under both policies.
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Perm>("Read|Garbage").has_value());
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Perm>("Read||Write").has_value());
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Perm>("99999999999999999999999").has_value());
}

TEST_CASE("zero combination is valid and distinct from no-name") {
  // The declared zero enumerator is used for the zero combination.
  const auto zero = codec::encode(Perm::None);
  REQUIRE(zero.has_value());
  REQUIRE(*zero == "None");

  // Decoding the zero name and the empty string yields the zero combination;
  // zero is never reported as unknown bits.
  constexpr auto from_name = codec::decode<unknown_bits::reject, Perm>("None");
  static_assert(from_name.has_value());
  static_assert(from_name.value() == Perm::None);
  constexpr auto from_empty = codec::decode<unknown_bits::reject, Perm>("");
  static_assert(from_empty.has_value());
  static_assert(from_empty.value() == Perm::None);

  // Without a zero enumerator the zero combination still encodes to an
  // engaged (empty) name, which is distinct from the disengaged "no name".
  const auto no_zero = codec::encode(NoZero{});
  REQUIRE(no_zero.has_value());
  REQUIRE(no_zero->empty());
  REQUIRE_FALSE(codec::encode(static_cast<NoZero>(4)).has_value());
  const auto no_zero_back = codec::decode<unknown_bits::reject, NoZero>(*no_zero);
  REQUIRE(no_zero_back.has_value());
  REQUIRE(*no_zero_back == NoZero{});
}

TEST_CASE("signed underlying types are range-checked first") {
  // A negative value is rejected under the reject policy.
  REQUIRE_FALSE(codec::encode(Small{-4}).has_value());

  // Under the preserve policy the original signed integer is kept.
  const auto negative = codec::encode<unknown_bits::preserve>(Small{-4});
  REQUIRE(negative.has_value());
  REQUIRE(*negative == "-4");
  const auto negative_back = codec::decode<unknown_bits::preserve, Small>(*negative);
  REQUIRE(negative_back.has_value());
  REQUIRE(*negative_back == Small{-4});

  // The most negative value of the underlying type round-trips as well.
  const auto min_value = codec::encode<unknown_bits::preserve>(Small{-128});
  REQUIRE(min_value.has_value());
  REQUIRE(*min_value == "-128");
  const auto min_back = codec::decode<unknown_bits::preserve, Small>(*min_value);
  REQUIRE(min_back.has_value());
  REQUIRE(*min_back == Small{-128});

  // A positive signed value with unnamed high bits keeps the remainder.
  const auto high = codec::encode<unknown_bits::preserve>(Small{127});
  REQUIRE(high.has_value());
  REQUIRE(*high == "Low|High|124");
  const auto high_back = codec::decode<unknown_bits::preserve, Small>(*high);
  REQUIRE(high_back.has_value());
  REQUIRE(*high_back == Small{127});

  // Range checks: a negative token never fits an unsigned underlying type,
  // and out-of-range magnitudes are rejected before any bit is produced.
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Perm>("-4").has_value());
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Small>("-129").has_value());
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Small>("128").has_value());
  REQUIRE(codec::decode<unknown_bits::preserve, Small>("127").has_value());
}

#if defined(MAGIC_ENUM_SUPPORTED_ALIASES)
TEST_CASE("aliases never produce the same bit twice") {
  // Fixed display strategy: aliases share the canonical first declaration, so
  // the shared bit is emitted exactly once.
  const auto read = codec::encode(Access::Read);
  REQUIRE(read.has_value());
  REQUIRE(*read == "Read");

  const auto both = codec::encode(static_cast<Access>(1 | 2));
  REQUIRE(both.has_value());
  REQUIRE(*both == "Read|Write");

  // Alias names are not reflected separately; only the canonical name decodes.
  REQUIRE_FALSE(codec::decode<unknown_bits::reject, Access>("ReadAlso").has_value());
  REQUIRE_FALSE(codec::decode<unknown_bits::preserve, Access>("WriteAlso").has_value());

  const auto round_trip = codec::decode<unknown_bits::reject, Access>(*both);
  REQUIRE(round_trip.has_value());
  REQUIRE(*round_trip == static_cast<Access>(1 | 2));
}
#endif

TEST_CASE("round-trip preserves all bits") {
  constexpr std::uint32_t perm_values[] = {0, 1, 2, 3, 4, 5, 7, 8, 11, 15, 0x80000000u};
  for (const auto bits : perm_values) {
    const auto encoded = codec::encode<unknown_bits::preserve>(static_cast<Perm>(bits));
    REQUIRE(encoded.has_value());
    const auto decoded = codec::decode<unknown_bits::preserve, Perm>(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == static_cast<Perm>(bits));
  }
  constexpr int small_values[] = {0, 1, 2, 3, 8, 127, -1, -4, -128};
  for (const int value : small_values) {
    const auto encoded = codec::encode<unknown_bits::preserve>(static_cast<Small>(value));
    REQUIRE(encoded.has_value());
    const auto decoded = codec::decode<unknown_bits::preserve, Small>(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == static_cast<Small>(value));
  }
}

TEST_CASE("enum_name is unchanged") {
  REQUIRE(magic_enum::enum_name(Perm::Read) == "Read");
  // enum_name keeps its existing flags semantics: the zero enumerator and
  // undeclared combinations still have no name; only the codec is explicit.
  REQUIRE(magic_enum::enum_name(Perm::None).empty());
  REQUIRE(magic_enum::enum_name(static_cast<Perm>(1 | 2)).empty());
  REQUIRE(magic_enum::enum_name(Plain::alpha) == "alpha");
  REQUIRE(magic_enum::enum_name(Plain::beta) == "beta");
  REQUIRE(magic_enum::enum_name(static_cast<Plain>(42)).empty());
}
