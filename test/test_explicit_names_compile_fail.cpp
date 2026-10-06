// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

// Compile-fail cases for the explicit external name table.
// Selected with -DMAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE=N.

#include <magic_enum/magic_enum_explicit_names.hpp>

enum class Color { RED = 1, BLUE = 2, GREEN = 3 };

#if MAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE == 1

// "A" and "a" collide under ASCII case-insensitive folding while pointing at
// different enum values: the constexpr precheck must reject this at compile
// time instead of silently picking one.
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, table, case_insensitive_ascii,
                               {{"A", Color::RED}, {"a", Color::BLUE}, {"GREEN", Color::GREEN}});

#elif MAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE == 2

// The unsupported Unicode scope must be rejected at compile time.
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, table, unicode_case_insensitive,
                               {{"RED", Color::RED}, {"BLUE", Color::BLUE}});

#elif MAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE == 3

// Exact duplicate spelling pointing at different values is rejected too.
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, table, case_sensitive,
                               {{"RED", Color::RED}, {"RED", Color::BLUE}});

#elif MAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE == 4

// Empty external names are rejected.
MAGIC_ENUM_EXPLICIT_NAME_TABLE(Color, table, case_sensitive, {{"", Color::RED}});

#else
#  error Unknown MAGIC_ENUM_EXPLICIT_NAMES_COMPILE_FAIL_CASE.
#endif

int main() { return static_cast<int>(table.size()); }
