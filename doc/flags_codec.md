# Explicit enum-flags encode/decode plan

Header: `<magic_enum/magic_enum_flags_codec.hpp>`, namespace
`magic_enum::flags_codec`. The plan is additive: it does not change
`enum_name`, `enum_flags_name`, `enum_flags_cast` or any other existing API.

```cpp
enum class unknown_bits : unsigned char { reject, preserve }; // compile-time policy

template <unknown_bits Policy = unknown_bits::reject, typename E>
optional<string> encode(E value, char_type sep = '|');

template <unknown_bits Policy = unknown_bits::reject, typename E>
constexpr optional<E> decode(string_view text, char_type sep = '|');
```

## Rules

* **Known combinations map to names.** A value equal to a declared enumerator
  (including a named combination such as `ReadWrite = Read | Write`) encodes
  to that enumerator's name. Any remaining bits decompose into single-bit flag
  names joined by the separator.
* **Unknown bits are never dropped.** Bits without a name are handled by the
  compile-time policy `unknown_bits`:
  * `reject` (default): `encode`/`decode` return an empty optional.
  * `preserve`: the unnamed bits are kept as a decimal integer token of the
    original value (`"Read|8"`), and `decode` accepts such integer tokens
    again, so a round-trip preserves every bit.
* **Zero is not "no name".** The zero combination encodes to the declared zero
  enumerator (`"None"`) or, when none exists, to an engaged empty string. It
  never reports unknown bits. `decode("")` yields the zero combination.
* **Aliases have a fixed display strategy.** The first declared enumerator of
  a bit pattern is canonical; a bit is emitted at most once, so an alias never
  produces the same bit twice.
* **Signed underlying types are range-checked first.** Integer tokens are
  checked against the underlying type before any bit is produced: a negative
  token never fits an unsigned underlying type, and out-of-range magnitudes
  fail. Under `preserve`, a negative value encodes as its original signed
  integer (`"-4"`) and decodes back to the identical bit pattern.

## Round-trip property

For every value `v` of a flags enum `E`:

```cpp
decode<unknown_bits::preserve, E>(encode<unknown_bits::preserve>(v).value()).value() == v
```

including values with unnamed bits and negative values of signed underlying
types.

## Example

See `example/enum_flags_codec_example.cpp`, which encodes and decodes four
combinations (`None`, `Read`, `ReadWrite`, `Read|Write|Execute`) plus an
unknown-bit value under both policies.

## Acceptance test

Focused, deterministic, serial (`-j1`) run of the codec test only:

```sh
cmake -S . -B build -DMAGIC_ENUM_OPT_BUILD_TESTS=ON -DMAGIC_ENUM_OPT_BUILD_EXAMPLES=ON
cmake --build build --target test_flags_codec-cpp17
ctest --test-dir build -j1 -R '^test_flags_codec-cpp17$' --output-on-failure
```
