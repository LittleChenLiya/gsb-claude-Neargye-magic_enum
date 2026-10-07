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

#ifndef NEARGYE_MAGIC_ENUM_SWITCH_HPP
#define NEARGYE_MAGIC_ENUM_SWITCH_HPP

#include "magic_enum.hpp"

namespace magic_enum {

namespace detail {

struct default_result_type {};

template <typename T>
struct identity {
  using type = T;
};

struct nonesuch {};

template <typename F, typename V, bool = std::is_invocable_v<F, V>>
struct invoke_result : identity<nonesuch> {};

template <typename F, typename V>
struct invoke_result<F, V, true> : std::invoke_result<F, V> {};

template <typename F, typename V>
using invoke_result_t = typename invoke_result<F, V>::type;

template <typename E, enum_subtype S, typename F, std::size_t... J>
constexpr auto common_invocable(std::index_sequence<J...>) noexcept {
  static_assert(std::is_enum_v<E>, "magic_enum::detail::invocable_index requires enum type.");

  if constexpr (count_v<E, S> == 0) {
    return identity<nonesuch>{};
  } else {
    return std::common_type<invoke_result_t<F, enum_constant<values_v<E, S>[J]>>...>{};
  }
}

template <typename E, enum_subtype S, typename Result, typename F, bool HasResult>
constexpr auto result_type() noexcept {
  static_assert(std::is_enum_v<E>, "magic_enum::detail::result_type requires enum type.");

  constexpr auto seq = std::make_index_sequence<count_v<E, S>>{};
  using R = std::decay_t<typename decltype(common_invocable<E, S, F>(seq))::type>;
  using D = std::decay_t<Result>;
  if constexpr (std::is_same_v<Result, default_result_type>) {
    if constexpr (std::is_same_v<R, nonesuch>) {
      return identity<void>{};
    } else {
      return identity<R>{};
    }
  } else {
    if constexpr (std::is_convertible_v<R, D> && (!HasResult || std::is_convertible_v<Result, D>)) {
      return identity<D>{};
    } else if constexpr (std::is_convertible_v<Result, R>) {
      return identity<R>{};
    } else {
      return identity<nonesuch>{};
    }
  }
}

template <typename E, enum_subtype S, typename Result, typename F, bool HasResult = false, typename D = std::decay_t<E>, typename R = typename decltype(result_type<D, S, Result, F, HasResult>())::type>
using result_t = std::enable_if_t<std::is_enum_v<D> && !std::is_same_v<R, nonesuch>, R>;

template <typename E, enum_subtype S, typename Result, typename F>
using result_with_fallback_t = result_t<E, S, Result, F, true>;

#if !defined(MAGIC_ENUM_ENABLE_HASH) && !defined(MAGIC_ENUM_ENABLE_HASH_SWITCH)

template <typename T = void>
inline constexpr auto default_result_type_lambda = []() noexcept(std::is_nothrow_default_constructible_v<T>) { return T{}; };

template <>
inline constexpr auto default_result_type_lambda<void> = []() noexcept {};

template <std::size_t J, std::size_t End, typename R, typename E, enum_subtype S, typename F, typename Def>
constexpr decltype(auto) linear_switch_impl(F&& f, E value, Def&& def) {
  if constexpr (J < End) {
    using V = enum_constant<enum_value<E, J, S>()>;
    if (enum_value_equal(value, V::value)) {
      if constexpr (std::is_invocable_r_v<R, F, V>) {
        return static_cast<R>(detail::invoke_constant(std::forward<F>(f), V{}));
      } else {
        return def();
      }
    } else {
      return linear_switch_impl<J + 1, End, R, E, S>(std::forward<F>(f), value, std::forward<Def>(def));
    }
  } else {
    return def();
  }
}

template <typename R, typename E, enum_subtype S, typename F, typename Def>
constexpr decltype(auto) linear_switch(F&& f, E value, Def&& def) {
  static_assert(is_enum_v<E>, "magic_enum::detail::linear_switch requires enum type.");

  if constexpr (count_v<E, S> == 0) {
    return def();
  } else {
    return linear_switch_impl<0, count_v<E, S>, R, E, S>(std::forward<F>(f), value, std::forward<Def>(def));
  }
}
#endif

template <typename D, auto... Vs>
inline constexpr bool subset_same_enum_v = (std::is_same_v<std::decay_t<decltype(Vs)>, D> && ...);

template <typename D, enum_subtype S, auto V>
constexpr bool subset_value_reflected() noexcept {
  if constexpr (!std::is_same_v<std::decay_t<decltype(V)>, D>) {
    return false;
  } else {
    for (const auto e : values_v<D, S>) {
      if (enum_value_equal(e, V)) {
        return true;
      }
    }
    return false;
  }
}

template <typename D, enum_subtype S, auto... Vs>
inline constexpr bool subset_reflected_v = (subset_value_reflected<D, S, Vs>() && ...);

template <typename D, auto... Vs>
constexpr bool subset_unique() noexcept {
  if constexpr (!subset_same_enum_v<D, Vs...> || sizeof...(Vs) < 2) {
    return true; // Mismatched types are diagnosed by the same-enum check.
  } else {
    constexpr std::size_t size = sizeof...(Vs);
    const D values[size] = {Vs...};
    for (std::size_t i = 0; i < size; ++i) {
      for (std::size_t j = i + 1; j < size; ++j) {
        if (enum_value_equal(values[i], values[j])) {
          return false;
        }
      }
    }
    return true;
  }
}

template <typename D, auto... Vs>
inline constexpr bool subset_unique_v = subset_unique<D, Vs...>();

template <typename D, enum_subtype S, auto... Vs>
inline constexpr bool subset_valid_v = subset_same_enum_v<D, Vs...> && subset_reflected_v<D, S, Vs...> && subset_unique_v<D, Vs...>;

template <typename F, auto V, bool = std::is_enum_v<std::decay_t<decltype(V)>>>
struct subset_invoke_result : identity<nonesuch> {};

template <typename F, auto V>
struct subset_invoke_result<F, V, true> : invoke_result<F, enum_constant<V>> {};

template <typename F, auto V>
using subset_invoke_result_t = typename subset_invoke_result<F, V>::type;

template <typename F, auto... Vs>
constexpr auto subset_common_invocable() noexcept {
  if constexpr (sizeof...(Vs) == 0) {
    return identity<nonesuch>{};
  } else {
    return std::common_type<subset_invoke_result_t<F, Vs>...>{};
  }
}

template <typename E, typename Result, typename F, bool HasResult, auto... Vs>
constexpr auto subset_result_type() noexcept {
  static_assert(std::is_enum_v<E>, "magic_enum::detail::subset_result_type requires enum type.");

  using R = std::decay_t<typename decltype(subset_common_invocable<F, Vs...>())::type>;
  using D = std::decay_t<Result>;
  if constexpr (std::is_same_v<Result, default_result_type>) {
    if constexpr (std::is_same_v<R, nonesuch>) {
      return identity<void>{};
    } else {
      return identity<R>{};
    }
  } else {
    if constexpr (std::is_convertible_v<R, D> && (!HasResult || std::is_convertible_v<Result, D>)) {
      return identity<D>{};
    } else if constexpr (std::is_convertible_v<Result, R>) {
      return identity<R>{};
    } else {
      return identity<nonesuch>{};
    }
  }
}

template <typename E, typename Result, typename F, bool HasResult, auto... Vs>
struct subset_result_identity : identity<typename decltype(subset_result_type<std::decay_t<E>, Result, F, HasResult, Vs...>())::type> {};

template <typename E, typename Result, typename F, bool HasResult = false, auto... Vs>
using subset_result_t = std::enable_if_t<std::is_enum_v<std::decay_t<E>> && !std::is_same_v<typename subset_result_identity<E, Result, F, HasResult, Vs...>::type, nonesuch>, typename subset_result_identity<E, Result, F, HasResult, Vs...>::type>;

template <typename E, typename Result, typename F, auto... Vs>
using subset_result_with_fallback_t = subset_result_t<E, Result, F, true, Vs...>;

template <typename R, typename E, typename F, typename Def>
constexpr decltype(auto) subset_switch_impl(F&&, E, Def&& def) {
  return def();
}

template <typename R, typename E, typename F, typename Def, auto V, auto... Rest>
constexpr decltype(auto) subset_switch_impl(F&& f, E value, Def&& def) {
  using V2 = enum_constant<V>;
  if (enum_value_equal(value, V)) {
    if constexpr (std::is_invocable_r_v<R, F, V2>) {
      return static_cast<R>(detail::invoke_constant(std::forward<F>(f), V2{}));
    } else {
      return def();
    }
  } else {
    return subset_switch_impl<R, E, F, Def, Rest...>(std::forward<F>(f), value, std::forward<Def>(def));
  }
}

template <typename R, typename E, auto... Vs, typename F, typename Def>
constexpr decltype(auto) subset_switch(F&& f, E value, Def&& def) {
  static_assert(is_enum_v<E>, "magic_enum::detail::subset_switch requires enum type.");

  return subset_switch_impl<R, E, F, Def, Vs...>(std::forward<F>(f), value, std::forward<Def>(def));
}

} // namespace magic_enum::detail

template <typename Result = detail::default_result_type, typename E, detail::enum_subtype S = detail::subtype_v<E>, typename F, typename R = detail::result_t<E, S, Result, F>>
constexpr decltype(auto) enum_switch(F&& f, E value) {
  using D = std::decay_t<E>;
  static_assert(std::is_enum_v<D>, "magic_enum::enum_switch requires enum type.");
  static_assert(detail::is_reflected_v<D, S>, "magic_enum requires enum implementation and valid max and min.");

#if defined(MAGIC_ENUM_ENABLE_HASH) || defined(MAGIC_ENUM_ENABLE_HASH_SWITCH)
  return detail::hash_switch_values<D, S, detail::case_call_t::value>(
      std::forward<F>(f),
      value,
      detail::default_result_type_lambda<R>,
      [](D lhs, D rhs) { return detail::enum_value_equal(lhs, rhs); });
#else
  return detail::linear_switch<R, D, S>(
      std::forward<F>(f),
      value,
      detail::default_result_type_lambda<R>);
#endif
}

template <typename Result = detail::default_result_type, detail::enum_subtype S, typename E, typename F, typename R = detail::result_t<E, S, Result, F>>
constexpr decltype(auto) enum_switch(F&& f, E value) {
  return enum_switch<Result, E, S>(std::forward<F>(f), value);
}

template <typename Result, typename E, detail::enum_subtype S = detail::subtype_v<E>, typename F, typename R = detail::result_with_fallback_t<E, S, Result, F>>
constexpr decltype(auto) enum_switch(F&& f, E value, Result&& result) {
  using D = std::decay_t<E>;
  static_assert(std::is_enum_v<D>, "magic_enum::enum_switch requires enum type.");
  static_assert(detail::is_reflected_v<D, S>, "magic_enum requires enum implementation and valid max and min.");

#if defined(MAGIC_ENUM_ENABLE_HASH) || defined(MAGIC_ENUM_ENABLE_HASH_SWITCH)
  return detail::hash_switch_values<D, S, detail::case_call_t::value>(
      std::forward<F>(f),
      value,
      [&result]() -> R { return std::forward<Result>(result); },
      [](D lhs, D rhs) { return detail::enum_value_equal(lhs, rhs); });
#else
  return detail::linear_switch<R, D, S>(
      std::forward<F>(f),
      value,
      [&result]() -> R { return std::forward<Result>(result); });
#endif
}

template <typename Result, detail::enum_subtype S, typename E, typename F, typename R = detail::result_with_fallback_t<E, S, Result, F>>
constexpr decltype(auto) enum_switch(F&& f, E value, Result&& result) {
  return enum_switch<Result, E, S>(std::forward<F>(f), value, std::forward<Result>(result));
}

// Explicit enum subset dispatch: dispatches only on the enumerators listed in Vs....
// Subset values must be distinct reflected enumerators of the same enum type.
// The callable is instantiated only for the listed subset branches.
template <auto... Vs, typename E, typename F, typename R = detail::subset_result_t<E, detail::default_result_type, F, false, Vs...>>
constexpr decltype(auto) enum_switch_subset(F&& f, E value) {
  using D = std::decay_t<E>;
  constexpr auto S = detail::subtype_v<D>;
  static_assert(std::is_enum_v<D>, "magic_enum::enum_switch_subset requires enum type.");
  static_assert(detail::is_reflected_v<D, S>, "magic_enum requires enum implementation and valid max and min.");
  static_assert(detail::subset_same_enum_v<D, Vs...>, "magic_enum::enum_switch_subset requires subset values of the same enum type.");
  static_assert(detail::subset_reflected_v<D, S, Vs...>, "magic_enum::enum_switch_subset requires subset values to be reflected enum values.");
  static_assert(detail::subset_unique_v<D, Vs...>, "magic_enum::enum_switch_subset requires distinct subset values.");

  if constexpr (detail::subset_valid_v<D, S, Vs...>) {
    return detail::subset_switch<R, D, Vs...>(
        std::forward<F>(f),
        value,
        detail::default_result_type_lambda<R>);
  } else {
    return detail::default_result_type_lambda<R>();
  }
}

template <auto... Vs, typename Result, typename E, typename F, typename R = detail::subset_result_with_fallback_t<E, Result, F, Vs...>>
constexpr decltype(auto) enum_switch_subset(F&& f, E value, Result&& result) {
  using D = std::decay_t<E>;
  constexpr auto S = detail::subtype_v<D>;
  static_assert(std::is_enum_v<D>, "magic_enum::enum_switch_subset requires enum type.");
  static_assert(detail::is_reflected_v<D, S>, "magic_enum requires enum implementation and valid max and min.");
  static_assert(detail::subset_same_enum_v<D, Vs...>, "magic_enum::enum_switch_subset requires subset values of the same enum type.");
  static_assert(detail::subset_reflected_v<D, S, Vs...>, "magic_enum::enum_switch_subset requires subset values to be reflected enum values.");
  static_assert(detail::subset_unique_v<D, Vs...>, "magic_enum::enum_switch_subset requires distinct subset values.");

  if constexpr (detail::subset_valid_v<D, S, Vs...>) {
    return detail::subset_switch<R, D, Vs...>(
        std::forward<F>(f),
        value,
        [&result]() -> R { return std::forward<Result>(result); });
  } else {
    return static_cast<R>(std::forward<Result>(result));
  }
}

} // namespace magic_enum

template <>
struct std::common_type<magic_enum::detail::nonesuch, magic_enum::detail::nonesuch> : magic_enum::detail::identity<magic_enum::detail::nonesuch> {};

template <typename T>
struct std::common_type<T, magic_enum::detail::nonesuch> : magic_enum::detail::identity<T> {};

template <typename T>
struct std::common_type<magic_enum::detail::nonesuch, T> : magic_enum::detail::identity<T> {};

#endif // NEARGYE_MAGIC_ENUM_SWITCH_HPP
