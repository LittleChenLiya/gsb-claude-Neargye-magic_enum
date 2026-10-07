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

#ifndef MAGIC_ENUM_SWITCH_COMPILE_FAIL_CASE
#  error MAGIC_ENUM_SWITCH_COMPILE_FAIL_CASE must select compile-fail scenario.
#endif

#include <magic_enum/magic_enum_switch.hpp>

enum class Sparse { Low = -3, Mid = 2, High = 40 };

#if MAGIC_ENUM_SWITCH_COMPILE_FAIL_CASE == 1

// Subset member is not a reflected enumerator of the enum.
constexpr auto out_of_subset_result = magic_enum::enum_switch_subset<Sparse::Low, static_cast<Sparse>(7)>([](auto) { return 0; }, Sparse::Low);
static_assert(out_of_subset_result == 0);

#elif MAGIC_ENUM_SWITCH_COMPILE_FAIL_CASE == 2

// Subset members must be distinct.
constexpr auto duplicate_subset_result = magic_enum::enum_switch_subset<Sparse::Low, Sparse::Low>([](auto) { return 0; }, Sparse::Low);
static_assert(duplicate_subset_result == 0);

#else

#  error Unknown MAGIC_ENUM_SWITCH_COMPILE_FAIL_CASE.

#endif

int main() {}
