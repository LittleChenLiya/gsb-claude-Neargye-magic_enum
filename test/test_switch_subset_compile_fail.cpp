// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

#define MAGIC_ENUM_RANGE_MIN -128
#define MAGIC_ENUM_RANGE_MAX 256
#include <magic_enum/magic_enum_switch.hpp>

enum class SparseSubset : int { Low = -100, Mid = 0, High = 250 };

#if MAGIC_ENUM_SWITCH_SUBSET_COMPILE_FAIL_CASE == 1

// Subset member outside the reflected enum range.
const auto result = magic_enum::enum_switch_subset<static_cast<SparseSubset>(1000)>(
    [](auto val) { return magic_enum::enum_integer(val()); }, SparseSubset::Low);

#elif MAGIC_ENUM_SWITCH_SUBSET_COMPILE_FAIL_CASE == 2

// Duplicated subset member.
const auto result = magic_enum::enum_switch_subset<SparseSubset::Low, SparseSubset::Low>(
    [](auto val) { return magic_enum::enum_integer(val()); }, SparseSubset::Low);

#else
#  error Unknown MAGIC_ENUM_SWITCH_SUBSET_COMPILE_FAIL_CASE.
#endif

int main() { return static_cast<int>(result.has_value()); }
