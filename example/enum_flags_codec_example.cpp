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

#include <cstdint>
#include <iostream>

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_flags_codec.hpp>

enum class FileAccess : std::uint32_t {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Execute = 1 << 2,
  ReadWrite = Read | Write,
};
// Add specialization `is_flags` to define that enum are flags.
template <>
struct magic_enum::customize::enum_range<FileAccess> {
  static constexpr bool is_flags = true;
};

namespace codec = magic_enum::flags_codec;
using magic_enum::flags_codec::unknown_bits;

int main() {
  // Four combinations: zero, single flag, named combination, decomposed flags.
  constexpr FileAccess combos[] = {
      FileAccess::None,                        // 0: the zero combination.
      FileAccess::Read,                        // 1: a single flag.
      FileAccess::ReadWrite,                   // 1|2: a named combination.
      static_cast<FileAccess>(1 | 2 | 4),      // 1|2|4: decomposed into flag names.
  };
  for (const auto combo : combos) {
    const auto encoded = codec::encode(combo);
    const auto decoded = codec::decode<unknown_bits::reject, FileAccess>(*encoded);
    std::cout << "bits=" << magic_enum::enum_integer(combo) << " encode=\"" << *encoded << "\""
              << " round-trip=" << (decoded.has_value() && *decoded == combo ? "ok" : "FAILED") << std::endl;
  }
  // bits=0 encode="None" round-trip=ok
  // bits=1 encode="Read" round-trip=ok
  // bits=3 encode="ReadWrite" round-trip=ok
  // bits=7 encode="Read|Write|Execute" round-trip=ok

  // Unknown bits are rejected by default...
  const auto unknown = static_cast<FileAccess>(1 | 8);
  if (!codec::encode(unknown).has_value()) {
    std::cout << "bits=9 encode rejected (unknown bit 8)" << std::endl;
  }

  // ...or preserved as the original integer, so no bit is ever dropped.
  const auto preserved = codec::encode<unknown_bits::preserve>(unknown);
  const auto restored = codec::decode<unknown_bits::preserve, FileAccess>(*preserved);
  std::cout << "bits=9 encode=\"" << *preserved << "\""
            << " round-trip=" << (restored.has_value() && *restored == unknown ? "ok" : "FAILED") << std::endl;
  // bits=9 encode="Read|8" round-trip=ok

  return 0;
}
