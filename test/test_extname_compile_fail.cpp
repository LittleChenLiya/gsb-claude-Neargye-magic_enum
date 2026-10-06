// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 - 2026 Daniil Goncharov <neargye@gmail.com>.

#if MAGIC_ENUM_EXTNAME_COMPILE_FAIL_CASE == 1

// "A" and "a" canonicalize to the same name under insensitive_ascii but map
// to different enum values: the compile plan must be rejected.
#  include <magic_enum/magic_enum_extname.hpp>

enum class Collision { a = 1, b = 2 };

inline constexpr auto conflicting = magic_enum::extname::make_table<Collision>(
    magic_enum::extname::case_policy::insensitive_ascii,
    magic_enum::extname::entry<Collision>{"A", Collision::a},
    magic_enum::extname::entry<Collision>{"a", Collision::b});

#elif MAGIC_ENUM_EXTNAME_COMPILE_FAIL_CASE == 2

// An invalid case policy is rejected when the table is built.
#  include <magic_enum/magic_enum_extname.hpp>

enum class Collision { a = 1, b = 2 };

inline constexpr auto bad_policy = magic_enum::extname::make_table<Collision>(
    static_cast<magic_enum::extname::case_policy>(7),
    magic_enum::extname::entry<Collision>{"a", Collision::a});

#elif MAGIC_ENUM_EXTNAME_COMPILE_FAIL_CASE == 3

// Empty external names are rejected when the table is built.
#  include <magic_enum/magic_enum_extname.hpp>

enum class Collision { a = 1, b = 2 };

inline constexpr auto empty_name = magic_enum::extname::make_table<Collision>(
    magic_enum::extname::case_policy::sensitive,
    magic_enum::extname::entry<Collision>{"", Collision::a});

#else
#  error Unknown MAGIC_ENUM_EXTNAME_COMPILE_FAIL_CASE.
#endif

int main() {}
