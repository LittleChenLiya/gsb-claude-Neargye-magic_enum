// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.

#include <new>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#define MAGIC_ENUM_RANGE_MIN -128
#define MAGIC_ENUM_RANGE_MAX 256
#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_switch.hpp>

#include <optional>
#include <type_traits>
#include <utility>

// Sparse enum: values are far apart and not contiguous.
enum class Sparse : int { Low = -100, Mid = 0, High = 250 };

using namespace magic_enum;

template <typename... Ts>
struct overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

TEST_CASE("enum_switch_subset") {
  SUBCASE("hit invokes exactly one branch and preserves underlying value") {
    constexpr auto integer = [](auto val) { return enum_integer(val()); };
    static_assert(std::is_same_v<decltype(enum_switch_subset<Sparse::Low, Sparse::High>(integer, Sparse::Low)), std::optional<int>>);
    static_assert(enum_switch_subset<Sparse::Low, Sparse::High>(integer, Sparse::Low).value() == -100);
    static_assert(enum_switch_subset<Sparse::Low, Sparse::High>(integer, Sparse::High).value() == 250);
    constexpr auto identity = [](auto val) { return val(); };
    static_assert(enum_switch_subset<Sparse::Low, Sparse::High>(identity, Sparse::High).value() == Sparse::High);

    int calls = 0;
    int seen = 0;
    const auto counter = [&calls, &seen](auto val) {
      ++calls;
      seen = enum_integer(val());
      return seen;
    };
    const auto result = enum_switch_subset<Sparse::Low, Sparse::High>(counter, Sparse::High);
    REQUIRE(result.has_value());
    REQUIRE(result.value() == 250);
    REQUIRE(calls == 1);
    REQUIRE(seen == 250);
  }

  SUBCASE("value outside the subset is not invoked") {
    constexpr auto integer = [](auto val) { return enum_integer(val()); };
    // Reflected member that is not part of the subset.
    static_assert(!enum_switch_subset<Sparse::Low, Sparse::High>(integer, Sparse::Mid).has_value());
    // Value outside the reflected range entirely.
    static_assert(!enum_switch_subset<Sparse::Low, Sparse::High>(integer, static_cast<Sparse>(7)).has_value());

    int calls = 0;
    const auto counter = [&calls](auto val) {
      ++calls;
      return enum_integer(val());
    };
    REQUIRE_FALSE(enum_switch_subset<Sparse::Low, Sparse::High>(counter, Sparse::Mid).has_value());
    REQUIRE(calls == 0);
    REQUIRE_FALSE(enum_switch_subset<Sparse::Low, Sparse::High>(counter, static_cast<Sparse>(12345)).has_value());
    REQUIRE(calls == 0);
  }

  SUBCASE("void callback reports whether it was invoked") {
    int calls = 0;
    const auto sink = [&calls](auto) { ++calls; };
    static_assert(std::is_same_v<decltype(enum_switch_subset<Sparse::Low, Sparse::High>(sink, Sparse::Low)), bool>);
    REQUIRE(enum_switch_subset<Sparse::Low, Sparse::High>(sink, Sparse::Low));
    REQUIRE(calls == 1);
    REQUIRE_FALSE(enum_switch_subset<Sparse::Low, Sparse::High>(sink, Sparse::Mid));
    REQUIRE(calls == 1);
  }

  SUBCASE("callback is only instantiated for subset branches") {
    int calls = 0;
    // Only invocable with enum_constant<Sparse::Low>; any other branch
    // instantiation would be a hard error without the subset constraint.
    const auto partial = overloaded{[&calls](enum_constant<Sparse::Low>) {
      ++calls;
      return 1;
    }};
    static_assert(std::is_same_v<decltype(enum_switch_subset<Sparse::Low, Sparse::High>(partial, Sparse::Low)), std::optional<int>>);
    REQUIRE(enum_switch_subset<Sparse::Low, Sparse::High>(partial, Sparse::Low).value() == 1);
    REQUIRE(calls == 1);
    // Sparse::High is in the subset but not invocable: falls back to "not invoked".
    REQUIRE_FALSE(enum_switch_subset<Sparse::Low, Sparse::High>(partial, Sparse::High).has_value());
    REQUIRE(calls == 1);
    REQUIRE_FALSE(enum_switch_subset<Sparse::Low, Sparse::High>(partial, Sparse::Mid).has_value());
    REQUIRE(calls == 1);
  }

  SUBCASE("constexpr and consteval usage") {
    constexpr auto name_of = [](auto val) { return enum_name(val()); };
    static_assert(enum_switch_subset<Sparse::Low, Sparse::High>(name_of, Sparse::Low).value() == "Low");
    static_assert(enum_switch_subset<Sparse::Low, Sparse::High>(name_of, Sparse::High).value() == "High");
    static_assert(!enum_switch_subset<Sparse::Low, Sparse::High>(name_of, Sparse::Mid).has_value());
  }
}

TEST_CASE("enum_switch default interface regression") {
  SUBCASE("default result") {
    constexpr auto member = &enum_constant<Sparse::Mid>::operator();
    static_assert(enum_switch(member, Sparse::Mid) == Sparse::Mid);
    static_assert(enum_switch(member, Sparse::Low) == Sparse{});
    static_assert(enum_switch(member, Sparse::Mid, Sparse::High) == Sparse::Mid);
    static_assert(enum_switch(member, Sparse::Low, Sparse::High) == Sparse::High);
    static_assert(enum_switch(member, static_cast<Sparse>(7), Sparse::High) == Sparse::High);
  }

  SUBCASE("fallback value") {
    constexpr auto eq_low = [](auto val) { return val() == Sparse::Low; };
    constexpr bool fallback = true;
    static_assert(std::is_same_v<decltype(enum_switch(eq_low, Sparse::High, fallback)), bool>);
    static_assert(!enum_switch(eq_low, Sparse::High, fallback));
    static_assert(enum_switch(eq_low, Sparse::Low, fallback));
    static_assert(enum_switch(eq_low, static_cast<Sparse>(7), fallback));

    constexpr auto integer = [](auto) { return 7; };
    static_assert(std::is_same_v<decltype(enum_switch(integer, Sparse::Low, -1L)), long>);
    REQUIRE(enum_switch(integer, Sparse::Low, -1L) == 7L);
    REQUIRE(enum_switch(integer, static_cast<Sparse>(7), -1L) == -1L);
  }

  SUBCASE("partial invocable with fallback") {
    int calls = 0;
    const int fallback = -1;
    const auto switcher = overloaded{
      [&calls](enum_constant<Sparse::Low>) { ++calls; return 1; },
      [&calls](enum_constant<Sparse::High>) { ++calls; return 2; }
    };

    REQUIRE(enum_switch(switcher, Sparse::Low, fallback) == 1);
    REQUIRE(calls == 1);
    REQUIRE(enum_switch(switcher, Sparse::High, fallback) == 2);
    REQUIRE(calls == 2);
    REQUIRE(enum_switch(switcher, Sparse::Mid, fallback) == -1);
    REQUIRE(enum_switch(switcher, static_cast<Sparse>(7), fallback) == -1);
    REQUIRE(calls == 2);
  }
}
