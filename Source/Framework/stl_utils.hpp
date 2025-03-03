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
  
  template<typename ...> using void_t = void;
  template<typename T> struct type_identity { using type = T; };

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
  { static constexpr bool is_ref = true; static constexpr bool is_lvalue_or_rvalue_ref = false; using type = T; };
  template<typename T> struct remove_reference<T &&>
  { static constexpr bool is_ref = true; static constexpr bool is_lvalue_or_rvalue_ref = true; using type = T; };
  template<typename T> using remove_reference_t = typename remove_reference<T>::type;

  template<typename T> struct remove_cv
  { static constexpr bool is_const = false; static constexpr bool is_volatile = false; using type = T; };
  template<typename T> struct remove_cv<const T>
  { static constexpr bool is_const = true; static constexpr bool is_volatile = false; using type = T; };
  template<typename T> struct remove_cv<volatile T>
  { static constexpr bool is_const = false; static constexpr bool is_volatile = true; using type = T; };
  template<typename T> struct remove_cv<const volatile T>
  { static constexpr bool is_const = true; static constexpr bool is_volatile = true; using type = T; };
  template<typename T> using remove_cv_t = typename remove_cv<T>::type;
  template<typename T> using remove_cvref_t = remove_cv_t<remove_reference_t<T>>;

  template<typename T> struct remove_extent { using type = T; };
  template<typename T> struct remove_extent<T[]> { using type = T; };
  template<typename T, auto N> struct remove_extent<T[N]> { using type = T; };
  template<typename T> using remove_extent_t = typename remove_extent<T>::type;

  #define COMPLEX_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
  #define COMPLEX_MOVE(...) static_cast<::utils::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
  #define COMPLEX_IS_ALIGNED_TO(type, variable, alignment) (((alignof(type) + offsetof(type, variable)) % alignment) == 0)

  template<typename, typename> inline constexpr bool is_same_v = false;
  template<typename T> inline constexpr bool is_same_v<T, T> = true;

  template<typename T> inline constexpr bool is_const_v = remove_cv<T>::is_const;

  template<typename T> inline constexpr bool is_void_v = is_same_v<remove_cv_t<T>, void>;

  template<typename T> inline constexpr bool is_reference_v = remove_reference<T>::is_ref;
  template<typename T> inline constexpr bool is_lvalue_reference_v = remove_reference<T>::is_ref && !remove_reference<T>::is_lvalue_or_rvalue_ref;
  template<typename T> inline constexpr bool is_rvalue_reference_v = remove_reference<T>::is_ref && remove_reference<T>::is_lvalue_or_rvalue_ref;

  template<typename T> inline constexpr bool is_pointer_v                    = false;
  template<typename T> inline constexpr bool is_pointer_v<T *              > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *const         > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *volatile      > =  true;
  template<typename T> inline constexpr bool is_pointer_v<T *const volatile> =  true;

  template<typename T> inline constexpr bool is_array_v = false;
  template<typename T> inline constexpr bool is_array_v<T[]> = true;
  template<typename T, auto N> inline constexpr bool is_array_v<T[N]> = true;

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

  template<typename T> // only function types and reference types can't be const qualified
  inline constexpr bool is_function_v = !is_const_v<const T> && !is_reference_v<T>;

  using type_id_t = const void *;
  template<typename T>
  inline constexpr type_id_t type_id = []<typename U>()
  {
  #ifdef _MSC_VER
    return __FUNCSIG__;
  #else
    return __PRETTY_FUNCTION__;
  #endif
  }.template operator()<T>();

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

  struct ignore_t { constexpr const ignore_t &operator=(const auto &) const noexcept { return *this; } };
  inline constexpr ignore_t ignore{};

  struct uninitialised_t { };
  inline constexpr uninitialised_t uninitialised{};

  template<typename T>
  add_rvalue_reference_t<T> declval() noexcept;

  template<typename T>
  concept integral = is_integral_v<T>;

  template<typename T>
  concept signed_integral = integral<T> && static_cast<T>(-1) < static_cast<T>(0);

  template<typename T>
  concept unsigned_integral = integral<T> && !signed_integral<T>;

  template<typename T>
  concept floating_point = is_floating_point_v<T>;

  template<typename Derived, typename Base>
  concept derived_from = is_base_of_v<Base, Derived> &&
    is_convertible_v<const volatile Derived *, const volatile Base *>;

  template<typename T>
  [[nodiscard]] inline constexpr T *launder(T *pointer) noexcept
  {
    static_assert(!is_function_v<T>, "can't launder functions");
    static_assert(!is_void_v<T>, "can't launder cv-void");
    return __builtin_launder(pointer);
  }

  template<usize N, typename Fn> requires requires(Fn fn) { fn(); }
  inline void unroll(const Fn &fn)
  {
    if constexpr (N >= 1)
    {
      [&]<usize ... Is>(const index_sequence<Is...> &)
      {
        [[maybe_unused]] auto dummy = ((fn(), Is) + ...);
      }(make_index_sequence<N>());
    }
  }
  template<usize N, typename Fn> requires requires(Fn fn, usize i) { fn(i); }
  inline void unroll(const Fn &fn)
  {
    if constexpr (N >= 1)
    {
      [&]<usize ... Is>(const index_sequence<Is...> &)
      {
        [[maybe_unused]] auto dummy = ((fn(Is), Is) + ...);
      }(make_index_sequence<N>());
    }
  }

  template<typename T>
  constexpr strict_inline T lerp(T a, T b, T t) noexcept { return a + t * (b - a); }
  template<typename T>
  constexpr strict_inline T (min)(T one, T two) noexcept { return (one < two) ? one : two; }
  template<typename T>
  constexpr strict_inline T (max)(T one, T two) noexcept { return (one >= two) ? one : two; }
  template<typename T>
  constexpr strict_inline T clamp(T value, T min, T max) noexcept
  { return (value > max) ? max : (value < min) ? min : value; }

  template<typename T, typename U>
  struct pair
  {
    T first;
    U second;

    friend constexpr bool operator==(const pair &lhs, const pair &rhs) noexcept = default;
  };

  template <class T, class U>
  pair(T, U) -> pair<T, U>;

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

    constexpr void fill(const T(&values)[Size])
    {
      for (usize i = 0; i < Size; ++i)
        storage[i] = values[i];
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (Size == 0) ? iterator{} : iterator(storage); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (Size == 0) ? const_iterator{} : const_iterator(storage); }
    [[nodiscard]] constexpr iterator end() noexcept { return (Size == 0) ? iterator{} : iterator(storage + Size); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (Size == 0) ? const_iterator{} : const_iterator(storage + Size); }

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

    [[nodiscard]] constexpr bool operator==(const array &other) const noexcept
    {
      for (usize i = 0; i < size(); ++i)
        if (storage[i] != other.storage[i])
          return false;

      return true;
    }

    internal_value_type storage[(Size == 0) ? 1 : Size];
  #undef ASSERT_NOT_ZERO_SIZED
  };

  template <typename First, typename ... Rest> requires (is_same_v<First, Rest> && ...)
  array(First, Rest...) -> array<First, sizeof...(Rest) + 1>;

  template <typename T>
  class span
  {
  public:
    using element_type = T;
    using value_type = remove_cv_t<T>;
    using size_type = usize;
    using difference_type = isize;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;

    constexpr span() noexcept = default;
    constexpr span(const span &other) noexcept = default;

    template <typename It>
    constexpr span(It first, size_type count) noexcept : data_{ &(*first) }, size_{ count } { }

    template <typename It>
    constexpr span(It first, It last) noexcept : data_{ &(*first) }, 
      size_{ static_cast<size_type>(last - first) } { COMPLEX_ASSERT(last > first); }

    template <auto Size>
    constexpr span(type_identity<element_type>::type (&rawArray)[Size]) noexcept : 
      data_{ rawArray }, size_{ static_cast<size_type>(Size) } { }

    constexpr span(auto &&range) requires requires { range.data(); range.size(); } :
      data_{ range.data() }, size_{ static_cast<size_type>(range.size()) } { }

    template <typename U> requires is_convertible_v<U(*)[], element_type(*)[]>
    constexpr span(const span<U> &other) noexcept : data_{ other.data() }, size_{ other.size() } { }

    [[nodiscard]] constexpr auto first(const size_type count) const noexcept
    {
      COMPLEX_ASSERT(count <= size_, "Count out of range in span::first(count)");
      return span<element_type>{ size_, count };
    }
    [[nodiscard]] constexpr auto last(const size_type count) const noexcept
    {
      COMPLEX_ASSERT(count <= size_, "Count out of range in span::last(count)");
      return span<element_type>{ data_ + (size_ - count), count };
    }
    [[nodiscard]] constexpr auto subspan(const size_type position, const size_type count = size_type(-1)) const noexcept
    {
      COMPLEX_ASSERT(position <= size_, "Position out of range in span::subspan(position, count)");
      COMPLEX_ASSERT(count == size_type(-1) || count <= size_ - position,
        "Count out of range in span::subspan(position, count)");

      return span<element_type>{ data_ + position, count == size_type(-1) ? size_ - position : count };
    }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept { return size_ * sizeof(element_type); }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr pointer data() const noexcept { return data_; }
    [[nodiscard]] constexpr reference operator[](const size_type position) const noexcept
    {
      COMPLEX_ASSERT(position < size_, "Span index out of range");
      return data_[position];
    }
    [[nodiscard]] constexpr reference front() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Front of empty span");
      return data_[0];
    }
    [[nodiscard]] constexpr reference back() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Back of empty span");
      return data_[size_ - 1];
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (size_ == 0) ? iterator{} : iterator(data_); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_); }
    [[nodiscard]] constexpr iterator end() noexcept { return (size_ == 0) ? iterator{} : iterator(data_ + size_); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_ + size_); }

  protected:
    pointer data_ = nullptr;
    size_type size_ = 0;
  };

  // deduction guide for raw arrays
  template<typename T, auto Size>
  span(T (&)[Size]) -> span<T>;

  // deduction guide for pointer/iterator pairs
  template<typename It>
  span(It, It) -> span<remove_reference_t<decltype(*declval<It>())>>;

  // deduction guide for contiguous containers
  template<typename Range>
  span(Range &&) -> span<remove_reference_t<decltype(declval<Range>()[0])>>;

  class string_view
  {
  public:
    using value_type = char;
    using size_type = usize;
    using difference_type = isize;
    using pointer = char *;
    using const_pointer = const char *;
    using reference = char &;
    using const_reference = const char &;
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr auto npos = size_type(-1);

    constexpr string_view() noexcept = default;
    constexpr string_view(const string_view &other) noexcept = default;
    constexpr string_view(const char *string) : string_view{ string, get_size(string) } { }
    constexpr string_view(const char *string, size_type size) noexcept : data_{ string }, size_{ size } { }
    template <auto Size>
    constexpr string_view(type_identity<char>::type (&rawArray)[Size]) noexcept : string_view{ rawArray, Size } { }
    template<template<typename...> class Stringish, typename ... Ts>
    constexpr string_view(const Stringish<Ts...> &range) requires requires { range.data(); range.size(); } : 
      string_view{ range.data(), range.size() } { }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type length() const noexcept { return size(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr const_pointer data() const noexcept { return data_; }
    [[nodiscard]] constexpr const_reference operator[](const size_type position) const noexcept
    {
      COMPLEX_ASSERT(position < size_, "View index out of range");
      return data_[position];
    }
    [[nodiscard]] constexpr const_reference front() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Front of empty view");
      return data_[0];
    }
    [[nodiscard]] constexpr const_reference back() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Back of empty view");
      return data_[size_ - 1];
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (size_ == 0) ? iterator{} : iterator(data_); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_); }
    [[nodiscard]] constexpr iterator end() noexcept { return (size_ == 0) ? iterator{} : iterator(data_ + size_); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_ + size_); }

    constexpr void remove_prefix(const size_type count) noexcept
    {
      COMPLEX_ASSERT(size_ >= count, "Count out of range in string_view::remove_prefix(count)");
      data_ += count;
      size_ -= count;
    }
    constexpr void remove_suffix(const size_type count) noexcept
    {
      COMPLEX_ASSERT(size_ >= count, "Count out of range in string_view::remove_suffix(count)");
      size_ -= count;
    }
    [[nodiscard]] constexpr string_view substr(const size_type position = 0, size_type count = npos) const noexcept
    {
      COMPLEX_ASSERT(position <= size_, "Position out of range in string_view::substr(position, count)");
      COMPLEX_ASSERT(count == npos || count <= size_ - position,
        "Count out of range in string_view::substr(position, count)");

      return string_view{ data_ + position, count == npos ? size_ - position : count };
    }

    [[nodiscard]] constexpr size_type find(const char *substring, size_type size, size_type position = 0) const noexcept
    {
      COMPLEX_ASSERT(size <= size_, "Size of substring is larger than the searched string in string_view::find(substring, size, position)");
      COMPLEX_ASSERT(position + size <= size_, "Position out of range in string_view::find(substring, size, position)");

      for (; position + size <= size_; ++position)
        if (compare_strings(data_ + position, size, substring, size) == 0)
          return position;

      return npos;
    }
    [[nodiscard]] constexpr size_type find(string_view substring, size_type position = 0) const noexcept
    { return find(substring.data(), substring.size(), position); }
    [[nodiscard]] constexpr size_type find(const char *substring, size_type position = 0) const noexcept
    { return find(substring, get_size(substring), position); }

    [[nodiscard]] constexpr size_type rfind(const char *substring, size_type size, size_type position = npos) const noexcept
    {
      COMPLEX_ASSERT(size <= size_, "Size of substring is larger than the searched string in string_view::rfind(substring, size, position)");
      COMPLEX_ASSERT(position + size <= size_, "Position out of range in string_view::rfind(substring, size, position)");
      
      if (position == npos)
        position = size_ - size;

      while (true)
      {
        if (compare_strings(data_ + position, size, substring, size) == 0)
          return position;

        if (position == 0)
          return npos;

        --position;
      }
    }
    [[nodiscard]] constexpr size_type rfind(string_view substring, size_type position = npos) const noexcept
    { return rfind(substring.data(), substring.size(), position); }
    [[nodiscard]] constexpr size_type rfind(const char *substring, size_type position = npos) const noexcept
    { return rfind(substring, get_size(substring), position); }
    

    [[nodiscard]] constexpr int compare(const string_view other) const noexcept
    { return compare_strings(data(), size(), other.data(), other.size()); }

    [[nodiscard]] friend constexpr bool operator==(string_view lhs, string_view rhs) noexcept
    { return lhs.size() == rhs.size() && (lhs.data() == rhs.data() || lhs.compare(rhs) == 0); }

    [[nodiscard]] friend constexpr bool operator<(string_view lhs, string_view rhs) noexcept
    { return lhs.compare(rhs) < 0; }

  private:
    const_pointer data_ = nullptr;
    size_type size_ = 0;

    static constexpr size_type get_size(const char *string)
    {
      size_type count = 0;
      while (*string != char())
      {
        ++count;
        ++string;
      }
      return count;
    }
    static constexpr int compare_strings(const char *lhs, usize lhs_size,
      const char *rhs, usize rhs_size) noexcept
    {
      int result = __builtin_memcmp(lhs, rhs, 
        (lhs_size < rhs_size) ? lhs_size : rhs_size);

      if (result != 0)
        return result;

      if (lhs_size < rhs_size)
        return -1;

      if (lhs_size > rhs_size)
        return 1;

      return 0;
    }
  };

  // ghetto unique_ptr implementation
  template<typename T>
  class up
  {
    static constexpr bool is_array = is_array_v<T>;
    using V = remove_extent_t<T>;
  public:
    constexpr up() noexcept = default;
    constexpr up(std::nullptr_t) noexcept {}
    constexpr up(T *pointer) : object_{ pointer } { }
    constexpr up(const up &other) noexcept = delete;
    constexpr up(up &&other) noexcept
    {
      object_ = other.object_;
      other.object_ = nullptr;
    }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires (!is_array) && (derived_from<U, T> || derived_from<T, U>)
    constexpr up(up<U> &&other) noexcept
    {
      object_ = static_cast<V *>(other.object_);
      other.object_ = nullptr;
    }
    constexpr up &operator=(const up &other) noexcept = delete;
    constexpr up &operator=(up &&other) noexcept { up<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    // allows up- AND downcasting, check types before doing this
    template<typename U> requires utils::derived_from<U, T> || utils::derived_from<T, U>
    constexpr up &operator=(up<U> &&other) noexcept { up<T>{ COMPLEX_MOVE(other) }.swap(*this); return *this; }
    constexpr ~up() noexcept
    {
      if constexpr (is_array)
        delete[] object_;
      else
        delete object_;

      object_ = nullptr;
    }

    static constexpr up<T> create(usize size) requires is_array
    {
        up unique;
        unique.object_ = new V[size]{};
        return unique;
    }
    static constexpr up<T> create(auto &&... args) requires (!is_array)
    {
      up unique;
      unique.object_ = new T{ COMPLEX_FWD(args)... };
      return unique;
    }

    constexpr void swap(up &other) noexcept
    {
      auto *otherObject = other.object_;
      other.object_ = object_; 
      object_ = otherObject;
    }

    constexpr void reset() noexcept { up{}.swap(*this); }
    template<typename U> requires (!is_array) && (derived_from<U, T> || derived_from<T, U>)
    constexpr void reset(U *pointer) { up<T>{ pointer }.swap(*this); }

    constexpr V *release() noexcept
    {
      auto *pointer = object_;
      object_ = nullptr;
      return pointer;
    }

    constexpr V *get() const noexcept { return object_; }
    constexpr V *operator->() const noexcept { return object_; }
    constexpr V &operator*() const noexcept { return *object_; }
    constexpr V &operator[](size_t index) const noexcept { return object_[index]; }

    explicit constexpr operator bool() const noexcept { return object_; }

  private:
    V *object_ = nullptr;

    template<typename U> friend class up;
  };

  template<typename T> up(T *pointer) -> up<T>;

  template<typename T, typename U>
  constexpr bool operator==(const up<T> &one, const up<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const up<T> &one, std::nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(std::nullptr_t, const up<T> &two) noexcept { return two.get() == nullptr; }

}
