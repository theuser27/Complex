/*
  ==============================================================================

    stl_utils.hpp
    Created: 11 Oct 2024 8:26:35pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "platform_definitions.hpp"

// collection of utilities i regularly use of but don't want to include the headers of
namespace utils
{
  [[nodiscard]] constexpr bool is_constant_evaluated() noexcept { return __builtin_is_constant_evaluated(); }

  template<typename To, typename From> requires (sizeof(To) == sizeof(From) && __is_trivially_copyable(To) && __is_trivially_copyable(From))
  [[nodiscard]] constexpr To bit_cast(const From &value) noexcept { return __builtin_bit_cast(To, value); }

  template<typename, typename>
  inline constexpr bool is_same_v = false;
  template<typename T>
  inline constexpr bool is_same_v<T, T> = true;
  
  template<typename ...> using void_t = void;

  template<bool Test, typename True, typename False>
  struct conditional { using type = True; };
  template<typename True, typename False>
  struct conditional<false, True, False> { using type = False; };
  template<bool Test, typename True, typename False>
  using conditional_t = typename conditional<Test, True, False>::type;

  template<typename T, typename = void> struct add_reference { using lvalue = T; using rvalue = T; };
  template<typename T> struct add_reference<T, void_t<T &>> { using lvalue = T &; using rvalue = T &&; };
  template<typename T> using add_lvalue_reference_t = typename add_reference<T>::lvalue;
  template<typename T> using add_rvalue_reference_t = typename add_reference<T>::rvalue;

  template<typename T> struct remove_reference
  { static constexpr bool is_ref = false; static constexpr bool is_lvalue_or_rvalue_ref = false; using type = T; };
  template<typename T> struct remove_reference<T &>
  { static constexpr bool is_ref =  true; static constexpr bool is_lvalue_or_rvalue_ref = false; using type = T; };
  template<typename T> struct remove_reference<T &&>
  { static constexpr bool is_ref = true; static constexpr bool is_lvalue_or_rvalue_ref = true; using type = T; };
  template<typename T> using remove_reference_t = typename remove_reference<T>::type;

  template<typename T> struct remove_cv                   { using type = T; };
  template<typename T> struct remove_cv<const T>          { using type = T; };
  template<typename T> struct remove_cv<volatile T>       { using type = T; };
  template<typename T> struct remove_cv<const volatile T> { using type = T; };
  template<typename T> using remove_cv_t = typename remove_cv<T>::type;
  template<typename T> using remove_cvref_t [[msvc::known_semantics]] = remove_cv_t<remove_reference_t<T>>;

  #define COMPLEX_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
  #define COMPLEX_MOV(...) static_cast<::utils::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
  #define COMPLEX_IS_ALIGNED_TO(type, variable, alignment) (((alignof(type) + offsetof(type, variable)) % alignment) == 0)


  template<typename T> inline constexpr bool is_reference_v = remove_reference<T>::is_ref;
  template<typename T> inline constexpr bool is_lvalue_reference_v = remove_reference<T>::is_ref && !remove_reference<T>::is_lvalue_or_rvalue_ref;
  template<typename T> inline constexpr bool is_rvalue_reference_v = remove_reference<T>::is_ref && remove_reference<T>::is_lvalue_or_rvalue_ref;

  template<typename T> inline constexpr bool is_pointer_v                    = false;
  template<typename T> inline constexpr bool is_pointer_v<T *              > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *const         > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *volatile      > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *const volatile> =  true;

  template<typename T, typename ... Args>
  inline constexpr bool is_constructible_v = __is_constructible(T, Args...);

  template<typename T>
  inline constexpr bool is_copy_constructible_v = __is_constructible(T, add_lvalue_reference_t<const T>);

  template<typename T>
  inline constexpr bool is_default_constructible_v = __is_constructible(T);

  template<class Base, class Derived>
  inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);
  template<typename From, typename To>
  inline constexpr bool is_convertible_v =
#if COMPLEX_GCC
    __is_convertible(From, To);
#else
    __is_convertible_to(From, To);
#endif
  template<class T, template <typename ...> class Template>
  inline constexpr bool is_specialization_v = false;
  template<template <class...> class Template, class ... Ts>
  inline constexpr bool is_specialization_v<Template<Ts...>, Template> = true;

  template<typename T, typename ... Us>
  inline constexpr bool is_any_of_v = (is_same_v<T, Us> || ...);

  template<typename T>
  inline constexpr bool is_integral_v = is_any_of_v<remove_cv_t<T>, bool, char, signed char, unsigned char, wchar_t,
#ifdef __cpp_char8_t
    char8_t,
#endif
    char16_t, char32_t, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long>;

  template<typename T>
  inline constexpr bool is_floating_point_v = is_any_of_v<remove_cv_t<T>, float, double, long double>;


  template<class T, T ... Is>
  struct integer_sequence
  {
    using value_type = T;
    [[nodiscard]] static constexpr usize size() noexcept { return sizeof...(Is); }
  };

  template<class T, T Size>
  using make_integer_sequence =
#if COMPLEX_GCC
    integer_sequence<T, __integer_pack(Size)...>;
#else
    __make_integer_seq<integer_sequence, T, Size>;
#endif
  template<usize ... Is>
  using index_sequence = integer_sequence<usize, Is...>;
  template<usize Size>
  using make_index_sequence = make_integer_sequence<usize, Size>;

  template<usize N, typename Fn> requires requires(Fn fn) { fn(); }
  inline void unroll(Fn fn)
  {
    if constexpr (N >= 1)
    {
      [&]<usize ... Is>(const index_sequence<Is...> &)
      {
        [[maybe_unused]] auto dummy = ((fn(), Is) + ...);
      }(make_index_sequence<N>());
    }
  }

  template<typename T, typename U>
  struct pair
  {
    T first;
    U second;
  };

  template<typename T, usize Size>
  class array
  {
  #define ASSERT_NOT_ZERO_SIZED(x) static_assert(Size != 0, "Calling" #x "is not valid for zero-sized array.")
    using internal_value_type = conditional_t<Size == 0 && !__is_constructible(T), char, T>;
  public:
    using value_type = T;
    using size_type = usize;
    using difference_type = isize;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;

    constexpr void fill(const T &value)
    {
      for (usize i = 0; i < Size; ++i)
        storage[i] = value;
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (Size == 0) ? iterator() : iterator(storage); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (Size == 0) ? const_iterator() : const_iterator(storage); }
    [[nodiscard]] constexpr iterator end() noexcept { return (Size == 0) ? iterator() : iterator(storage + Size - 1); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (Size == 0) ? const_iterator() : const_iterator(storage + Size - 1); }

    [[nodiscard]] constexpr reference front() noexcept { ASSERT_NOT_ZERO_SIZED(front()); return storage[0]; }
    [[nodiscard]] constexpr const_reference front() const noexcept { ASSERT_NOT_ZERO_SIZED(front()); return storage[0]; }
    [[nodiscard]] constexpr reference back() noexcept { ASSERT_NOT_ZERO_SIZED(back()); return storage[Size - 1]; }
    [[nodiscard]] constexpr const_reference back() const noexcept { ASSERT_NOT_ZERO_SIZED(back()); return storage[Size - 1]; }

    [[nodiscard]] constexpr reference operator[](size_type index) noexcept
    {
      COMPLEX_ASSERT(index < Size, "Array index out of bounds.");
      return storage[index];
    }
    [[nodiscard]] constexpr const_reference operator[](size_type index) const noexcept
    {
      COMPLEX_ASSERT(index < Size, "Array index out of bounds.");
      return storage[index];
    }

    [[nodiscard]] constexpr pointer data() noexcept { return (Size == 0) ? nullptr : storage; }
    [[nodiscard]] constexpr const_pointer data() const noexcept { return (Size == 0) ? nullptr : storage; }

    [[nodiscard]] static constexpr size_type size() noexcept { return Size; }

    internal_value_type storage[(Size == 0) ? 1 : Size];
  #undef ASSERT_NOT_ZERO_SIZED
  };

  struct ignore_t { constexpr const ignore_t &operator=(const auto &) const noexcept { return *this; } };
  inline constexpr ignore_t ignore{};

  template <typename First, typename ... Rest> requires (is_same_v<First, Rest> && ...)
  array(First, Rest...) -> array<First, sizeof...(Rest) + 1>;

  template <typename T>
  concept integral = is_integral_v<T>;

  template <typename T>
  concept signed_integral = integral<T> && static_cast<T>(-1) < static_cast<T>(0);

  template <typename T>
  concept unsigned_integral = integral<T> && !signed_integral<T>;

  template <typename T>
  concept floating_point = is_floating_point_v<T>;

}
