
// Created: 2024-10-11 20:26:35

#pragma once

#include "platform.hpp"
#include "macro_helpers.hpp"

// collection of utilities i regularly use of but don't want to include the headers of

namespace std
{
  // i hate this type with a passion
  template<typename T>
  class initializer_list
  {
  public:
    using value_type      = T;
    using reference       = const T &;
    using const_reference = const T &;
    using size_type       = usize;

    using iterator        = const T *;
    using const_iterator  = const T *;

    constexpr initializer_list() noexcept = default;
    template<auto Size>
    constexpr initializer_list(const T (&rawArray)[Size]) noexcept : data_{ rawArray }, size_{ Size } { }
    
    constexpr const_iterator data() const noexcept { return data_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr const_iterator begin() const noexcept { return data_; }
    constexpr const_iterator end() const noexcept { return begin() + size(); }

    const_reference operator[](usize index) const noexcept { COMPLEX_ASSERT(index < size_); return data_[index]; }

  private:
    iterator data_{};
    size_type size_{};

    // private constructors
  #ifdef _MSC_VER
  public:
    constexpr initializer_list(const T *begin, const T *end) noexcept : data_{ begin }, size_{ size_type(end - begin) } { }
  #else
    constexpr initializer_list(const_iterator data, size_type size) : data_{ data }, size_{ size } { }
  #endif
  };

  template<typename T> constexpr const T *begin(initializer_list<T> list) noexcept { return list.begin(); }
  template<typename T> constexpr const T *end(initializer_list<T> list) noexcept { return list.end(); }

  // forward declaring things for manually defined structured bindings to work

  template<typename T>
  struct tuple_size;

  template<usize I, typename T>
  struct tuple_element;

#ifndef _MSC_VER

  // definition for gcc and clang is mandatory because it won't be able to find the __impl type
  struct source_location
  {
    // The names source_location::__impl, _M_file_name, _M_function_name, _M_line, and _M_column
    // are hard-coded in the compiler and must not be changed here.
    struct __impl
    {
      const char *_M_file_name;
      const char *_M_function_name;
      unsigned _M_line;
      unsigned _M_column;
    };

    // GCC returns the type 'const void*' from the builtin, while clang returns
    // `const __impl*`. Per C++ [expr.const], casts from void* are not permitted
    // in constant evaluation, so we don't want to use `void*` as the argument
    // type unless the builtin returned that, anyhow, and the invalid cast is
    // unavoidable.
    using T = decltype(__builtin_source_location());
  };

#endif
}

namespace utils
{
  using nullptr_t = decltype(nullptr);
  template<typename T> struct type_identity { using type = T; };

  template<bool Test, typename True, typename False>
  struct conditional { using type = True; };
  template<typename True, typename False>
  struct conditional<false, True, False> { using type = False; };
  template<bool Test, typename True, typename False>
  using conditional_t = typename conditional<Test, True, False>::type;

  template<typename T, typename = void> struct add_reference { using lvalue = T; using rvalue = T; };
  template<typename T> requires requires(T &a) { a; } struct add_reference<T> { using lvalue = T &; using rvalue = T &&; };
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

  template<typename T> struct remove_pointer { using type = T; };
  template<typename T> struct remove_pointer<T *> { using type = T; };
  template<typename T> using remove_pointer_t = typename remove_pointer<T>::type;

  template<typename T> struct underlying_type { using type = __underlying_type(T); };
  template<typename T> using underlying_type_t = typename underlying_type<T>::type;

  #define COMPLEX_FWD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)
  #define COMPLEX_MOVE(...) static_cast<::utils::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)
  #define COMPLEX_SWAP(x, y) { auto temp_____ = COMPLEX_MOVE(x); (x) = COMPLEX_MOVE(y); y = COMPLEX_MOVE(temp_____); }
  #define COMPLEX_SWAP_MEMBERS(variable, otherObject, /*thisObject*/...) \
    auto _##variable##__ = (otherObject).variable;                   \
    (otherObject).variable = __VA_OPT__((__VA_ARGS__).)variable; \
    __VA_OPT__((__VA_ARGS__).)variable = _##variable##__;

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
  inline constexpr bool is_copy_constructible_v = __is_constructible(T, const T &);

  template<typename T>
  inline constexpr bool is_default_constructible_v = __is_constructible(T);

  template<typename T>
  inline constexpr bool is_trivially_copy_constructible_v = __is_trivially_constructible(T, const T &);
  template<typename T>
  inline constexpr bool is_trivially_move_constructible_v = __is_trivially_constructible(T, T);

  template<typename T>
  inline constexpr bool is_trivially_destructible_v =
#if COMPLEX_GCC
  #if __has_builtin(__is_trivially_destructible)
    __is_trivially_destructible(T);
  #else
    __has_trivial_destructor(T);
  #endif
#else
    __is_trivially_destructible(T);
#endif

  template<typename T>
  inline constexpr bool is_trivially_copy_assignable_v = __is_trivially_assignable(T &, const T &);
  template<typename T>
  inline constexpr bool is_trivially_move_assignable_v = __is_trivially_assignable(T &, T);

  template<class Base, class Derived>
  inline constexpr bool is_base_of_v = __is_base_of(Base, Derived);
  template<typename From, typename To>
  inline constexpr bool is_convertible_v =
#if COMPLEX_GCC
    __is_convertible(From, To);
#else
    __is_convertible_to(From, To);
#endif
  template<class T, template<typename ...> class Template>
  inline constexpr bool is_specialization_v = false;
  template<template<typename ...> class Template, typename ... Ts>
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

  template<typename T>
  inline constexpr bool is_enum_v = __is_enum(T);

  template<typename T> // only function types and reference types can't be const qualified
  inline constexpr bool is_function_v = !is_const_v<const T> && !is_reference_v<T>;

  template<typename Return, typename Callable, typename ... Args>
  inline constexpr bool is_invocable_r_v = requires(Callable & callable, Args ... args)
  {
    requires is_same_v<Return, decltype(callable(COMPLEX_FWD(args)...))>;
  };

  template<typename T>
  inline constexpr T min_limit = (T(-1) < T(0)) ? T(T(1) << (sizeof(T) * 8 - 1)) : T(0);
  template<typename T>
  inline constexpr T max_limit = (T(-1) < T(0)) ? T(T(-1) ^ (T(1) << (sizeof(T) * 8 - 1))) : T(-1);

  template<class T, T ... Is>
  struct integer_sequence
  {
    using value_type = T;
    [[nodiscard]] static constexpr usize size() noexcept { return sizeof...(Is); }
  };

  template<class T, T Size>
  using make_integer_sequence =
#if defined(__GNUC__) && !defined(__clang__)
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

  struct align_val_t
  {
    usize value{};

    // based on the definition of std::align_val_t
    // https://en.cppreference.com/w/cpp/memory/new/align_val_t
    template<typename T> requires utils::is_enum_v<T> &&
      utils::is_same_v<utils::underlying_type_t<T>, usize>
    constexpr operator T() { return T(value); }
  };

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

  namespace detail
  {
    template<typename Sig>
    struct signature;

    template<typename Ret, typename T>
    struct signature<Ret(T)> { using type = T; };

    template<typename Ret, typename Obj, typename T>
    struct signature<Ret(Obj:: *)(T)> { using type = T; };

    template<typename Ret, typename Obj, typename T>
    struct signature<Ret(Obj:: *)(T) const> { using type = T; };
  }

  [[nodiscard]] constexpr bool is_constant_evaluated() noexcept { return __builtin_is_constant_evaluated(); }

  template<typename To, typename From> requires (sizeof(To) == sizeof(From) && __is_trivially_copyable(To) && __is_trivially_copyable(From))
  [[nodiscard]] constexpr To bit_cast(const From &value) noexcept { return __builtin_bit_cast(To, value); }

  template<typename T>
  [[nodiscard]] inline constexpr T *
  launder(T *pointer) noexcept
  {
    static_assert(!is_function_v<T>, "can't launder functions");
    static_assert(!is_void_v<T>, "can't launder cv-void");
    return __builtin_launder(pointer);
  }

  template<usize N, typename Fn>
  constexpr void unroll(const Fn &fn)
  {
    if constexpr (N >= 1)
    {
      [&]<usize ... Is>(const index_sequence<Is...> &)
      {
        if constexpr (requires(Fn fn, usize i) { fn(i); })
          [[maybe_unused]] auto dummy = ((fn(Is), Is) + ...);
        else if constexpr (requires(Fn fn) { fn(); })
          [[maybe_unused]] auto dummy = ((fn(), Is) + ...);
        else COMPLEX_ASSERT_FALSE("Couldn't find correct overload, consider adding it here");
      }(make_index_sequence<N>());
    }
  }

  template<usize Iterations>
  strict_inline void longPause() noexcept
  {
    unroll<Iterations>([]()
      {
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
        COMPLEX_PAUSE();
      });
  }

  template<typename T>
  constexpr T lerp(T a, T b, T t) noexcept { return a + t * (b - a); }
  template<typename T>
  constexpr T (min)(T one, T two) noexcept { return (one < two) ? one : two; }
  template<typename T>
  constexpr T (max)(T one, T two) noexcept { return (one >= two) ? one : two; }
  template<typename T>
  constexpr T clamp(T value, T min, T max) noexcept
  { return (value > max) ? max : (value < min) ? min : value; }
  template<typename T>
  constexpr T abs(const T x) noexcept
  {
    if constexpr (utils::is_floating_point_v<T>)
      return x == T(0) ? T(0) : x < T(0) ?  -x : x;
    else
      return x < T(0) ? -x : x;
  }

  constexpr usize
  getStringSize(const char *string) noexcept
  {
    if constexpr (requires(const char *s) { __builtin_strlen(s); })
      return __builtin_strlen(string);
    else
    {
      usize count = 0;
      while (*string != char())
      {
        ++count;
        ++string;
      }
      return count;
    }
  }

  constexpr int
  compareStrings(const char *lhs, usize lhsSize,
    const char *rhs, usize rhsSize) noexcept
  {
    int result = __builtin_memcmp(lhs, rhs,
      (lhsSize < rhsSize) ? lhsSize : rhsSize);

    if (result != 0)
      return result;

    if (lhsSize < rhsSize)
      return -1;

    if (lhsSize > rhsSize)
      return 1;

    return 0;
  }

  constexpr int
  compareCaseInsensitive(const char *lhs, usize lhsSize,
    const char *rhs, usize rhsSize) noexcept
  {
    if (lhsSize > rhsSize)
      return 1;
    else if (lhsSize < rhsSize)
      return -1;

    for (usize i = 0; i < lhsSize; ++i)
    {
      int left  = (lhs[i] >= 'A' && lhs[i] < 'Z') ? lhs[i] | (1 << 5) : lhs[i];
      int right = (rhs[i] >= 'A' && rhs[i] < 'Z') ? rhs[i] | (1 << 5) : rhs[i];
      if (int result = left - right; result != 0)
        return result;
    }

    return 0;
  }

  template<typename T, typename U>
  struct pair
  {
    T first;
    U second;

    // not constexpr because some clang builds fail if types don't have constexpr operator==
    // might update in the future
    friend bool operator==(const pair &lhs, const pair &rhs) noexcept = default;
  };

  template<typename T, typename U>
  pair(T, U) -> pair<T, U>;

  // singly-linked list
  template<typename T>
  struct sll
  {
    T object;
    sll *next{};
  };

  // doubly-linked list
  template<typename T>
  struct dll
  {
    T object;
    dll *previous{};
    dll *next{};
  };

  // inserts a node into a doubly-linked list
  // half connected - first element is connected to the last but not the other way around
  template<typename T>
  constexpr void insertDllHalfConnected(T *toInsert, T *insertBefore, T *&startingElement)
  {
    toInsert->next = nullptr;
    toInsert->previous = toInsert;

    if (!startingElement)
    {
      startingElement = toInsert;
      return;
    }

    if (!insertBefore)
    {
      toInsert->previous = startingElement->previous;
      startingElement->previous->next = toInsert;
      startingElement->previous = toInsert;
    }
    else
    {
      toInsert->previous = insertBefore->previous;
      toInsert->next = insertBefore;

      if (insertBefore->previous->next)
        insertBefore->previous->next = toInsert;
      insertBefore->previous = toInsert;

      if (insertBefore == startingElement)
        startingElement = toInsert;
    }
  }

  // removes a node from a doubly-linked list
  // half connected - first element is connected to the last but not the other way around
  template<typename T>
  constexpr void removeDllHalfConnected(T *toRemove, T *&startingElement)
  {
    if (toRemove->next)
      toRemove->next->previous = toRemove->previous;
    else
      startingElement->previous = toRemove->previous;

    if (toRemove->previous->next)
      toRemove->previous->next = toRemove->next;
    else
      startingElement = toRemove->next;

    toRemove->previous = nullptr;
    toRemove->next = nullptr;
  }

  // indexes a singly/doubly-linked list
  template<typename T>
  constexpr T *
  indexSll(T *start, usize index, T *sentinel = nullptr)
  {
    for (usize i = 0; i < index && start != sentinel; (++i), (start = start->next)) { }
    return start;
  }

  template<typename T>
  constexpr T *
  findIf(T *start, const auto &predicate, T *sentinel = nullptr)
  {
    for (; start != sentinel && !predicate(start); start = start->next) { }
    return start;
  }

#define COMPLEX_INTERNAL_DEFINE_ENUM_MAPPING(name, ...) name = __VA_ARGS__
#define COMPLEX_ENUM(name, /*valueIdPairs*/...) \
  namespace name { enum Value : uuid { COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_DEFINE_ENUM_MAPPING, (), (,), __VA_ARGS__) }; \
  inline constexpr auto values = utils::array{ COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_GET_1, (), (,), __VA_ARGS__) }; }
#define COMPLEX_ENUM_LOCAL(name, /*valueIdPairs*/...) \
  enum name : uuid { COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE, COMPLEX_INTERNAL_DEFINE_ENUM_MAPPING, (), (,), __VA_ARGS__) }; \
  static constexpr auto values##name = utils::array{ COMPLEX_FOR_EACH(COMPLEX_INTERNAL_ITERATE_EXCLUSIVE, COMPLEX_INTERNAL_GET_1, (name::), (,), __VA_ARGS__) };

  template<typename T, usize Size>
  class array
  {
  #define ASSERT_NOT_ZERO_SIZED(x) static_assert(Size != 0, "Calling" #x "is not valid for zero-sized array.")
    using internal_value_type = conditional_t<Size == 0 && !requires { T{}; }, char, T>;
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

    template<auto ArraySize> requires (Size > 0 && ArraySize == Size)
    constexpr void fill(const T(&values)[ArraySize])
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
      for (size_type i = 0; i < size(); ++i)
        if (storage[i] != other.storage[i])
          return false;

      return true;
    }

    internal_value_type storage[(Size == 0) ? 1 : Size]{};
  #undef ASSERT_NOT_ZERO_SIZED
  };

  template <typename First, typename ... Rest> requires (is_same_v<First, Rest> && ...)
  array(First, Rest...) -> array<First, sizeof...(Rest) + 1>;

  template<typename T>
  class span
  {
  public:
    using element_type = T;
    using value_type = remove_cv_t<T>;
    using size_type = usize;
    using difference_type = isize;
    using iterator = T *;
    using const_iterator = const T *;

    static constexpr auto npos = size_type(-1);

    T *data_{};
    size_type size_{};

    constexpr span() noexcept = default;
    constexpr span(const span &other) noexcept = default;
    constexpr span &operator=(const span &other) noexcept = default;

    template<typename It>
    constexpr span(It first, size_type count) noexcept : data_{ &(*first) }, size_{ count } { }

    template<typename It>
    constexpr span(It first, It pastLast) noexcept : data_{ &(*first) }, 
      size_{ (size_type)(pastLast - first) } { COMPLEX_ASSERT(pastLast > first); }

    template<auto Size>
    constexpr span(T (&rawArray)[Size]) noexcept : data_{ rawArray }, size_{ (size_type)Size } { }

    constexpr span(auto &range, size_type start = 0, size_type length = npos) noexcept
      requires requires { range.data(); range.size(); } : data_{ range.data() + start },
      size_{ utils::min((size_type)range.size() - start, length) } { }

    template<typename U> requires is_convertible_v<U(*)[], element_type(*)[]>
    constexpr span(const span<U> &other) noexcept : data_{ other.data() }, size_{ other.size() } { }

    [[nodiscard]] constexpr span 
    removeLast(size_type count) const noexcept
    {
      COMPLEX_ASSERT(count <= size_, "Count out of range in span::removeLast(count)");
      return { data_, count };
    }
    [[nodiscard]] constexpr span 
    removeFirst(size_type count) const noexcept
    {
      COMPLEX_ASSERT(count <= size_, "Count out of range in span::removeFirst(count)");
      return { data_ + (size_ - count), count };
    }
    [[nodiscard]] constexpr span 
    subrange(size_type position, size_type count = npos) const noexcept
    {
      COMPLEX_ASSERT(position <= size_, "Position out of range in span::subrange(position, count)");
      COMPLEX_ASSERT(count == npos || count <= size_ - position,
        "Count out of range in span::subrange(position, count)");

      return { data_ + position, utils::min(size_ - position, count) };
    }

    [[nodiscard]] constexpr size_type size() const noexcept { return size_; }
    [[nodiscard]] constexpr size_type sizeBytes() const noexcept { return size_ * sizeof(element_type); }
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] constexpr T *data() const noexcept { return data_; }
    [[nodiscard]] constexpr T &
    operator[](const size_type position) const noexcept
    {
      COMPLEX_ASSERT(position < size_, "Span index out of range");
      return data_[position];
    }
    [[nodiscard]] constexpr T &
    front() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Front of empty span");
      return data_[0];
    }
    [[nodiscard]] constexpr T &
    back() const noexcept
    {
      COMPLEX_ASSERT(size_ > 0, "Back of empty span");
      return data_[size_ - 1];
    }

    [[nodiscard]] constexpr iterator begin() noexcept { return (size_ == 0) ? iterator{} : iterator(data_); }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_); }
    [[nodiscard]] constexpr iterator end() noexcept { return (size_ == 0) ? iterator{} : iterator(data_ + size_); }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return (size_ == 0) ? const_iterator{} : const_iterator(data_ + size_); }
  };

  // deduction guide for raw arrays
  template<typename T, auto Size>
  span(T (&)[Size]) -> span<T>;

  // deduction guide for pointer/iterator pairs
  template<typename It>
  span(It, It) -> span<remove_reference_t<decltype(*declval<It>())>>;

  // deduction guide for pointer/iterator with size
  template<typename It>
  span(It, usize) -> span<remove_reference_t<decltype(*declval<It>())>>;

  // deduction guide for contiguous containers
  template<typename Range>
  span(Range &) -> span<remove_reference_t<decltype(declval<Range>()[0])>>;

  template<typename T>
  constexpr void copy(utils::span<T> destination, utils::span<const T> source)
  {
    auto length = utils::min(destination.size(), source.size());
    for (usize i = 0; i < length; ++i)
      destination[i] = source[i];
  }

  class string_view : public span<const char>
  {
  public:
    using span<const char>::span;

    template<auto Size>
    constexpr string_view(const char (&rawArray)[Size]) noexcept : span{ rawArray, Size - 1 } { }

    [[nodiscard]] constexpr size_type 
    find(const char *substring, size_type size, size_type position = 0) const noexcept
    {
      COMPLEX_ASSERT(size <= size_, "Size of substring is larger than the searched string in string_view::find(substring, size, position)");
      COMPLEX_ASSERT(position + size <= size_, "Position out of range in string_view::find(substring, size, position)");

      for (; position + size <= size_; ++position)
        if (utils::compareStrings(data_ + position, size, substring, size) == 0)
          return position;

      return npos;
    }
    [[nodiscard]] constexpr size_type 
    find(string_view substring, size_type position = 0) const noexcept
    { return find(substring.data(), substring.size(), position); }
    template<auto Size>
    [[nodiscard]] constexpr size_type 
    find(const char(&substring)[Size], size_type position = 0) const noexcept
    { return find(substring, Size, position); }

    [[nodiscard]] constexpr size_type 
    rfind(const char *substring, size_type size, size_type position = npos) const noexcept
    {
      COMPLEX_ASSERT(size <= size_, "Size of substring is larger than the searched string in string_view::rfind(substring, size, position)");
      COMPLEX_ASSERT(position + size <= size_, "Position out of range in string_view::rfind(substring, size, position)");
      
      if (position == npos)
        position = size_ - size;

      while (true)
      {
        if (utils::compareStrings(data_ + position, size, substring, size) == 0)
          return position;

        if (position == 0)
          return npos;

        --position;
      }
    }
    [[nodiscard]] constexpr size_type 
    rfind(string_view substring, size_type position = npos) const noexcept
    { return rfind(substring.data(), substring.size(), position); }
    template<auto Size>
    [[nodiscard]] constexpr size_type
    rfind(const char(&substring)[Size], size_type position = npos) const noexcept
    { return rfind(substring, Size, position); }
    

    [[nodiscard]] constexpr int
    compare(string_view other) const noexcept
    { return utils::compareStrings(data(), size(), other.data(), other.size()); }

    [[nodiscard]] constexpr int
    compareCaseInsensitive(string_view other) const noexcept
    { return utils::compareCaseInsensitive(data(), size(), other.data(), other.size()); }

    [[nodiscard]] friend constexpr bool 
    operator==(string_view lhs, string_view rhs) noexcept
    { return lhs.size() == rhs.size() && (lhs.data() == rhs.data() || lhs.compare(rhs) == 0); }

    [[nodiscard]] friend constexpr bool 
    operator<(string_view lhs, string_view rhs) noexcept
    { return lhs.compare(rhs) < 0; }
  };

  struct typeInfo
  {
    string_view p{};

    template<typename T>
    static constexpr typeInfo 
    create()
    {
    #ifdef _MSC_VER
      return typeInfo{ .p = __FUNCSIG__ };
    #else
      return typeInfo{ .p = __PRETTY_FUNCTION__ };
    #endif
    }

    constexpr bool operator==(const typeInfo &other) const = default;
    constexpr bool operator<(const typeInfo &other) const { return p < other.p; }
  };

  #define typeId(T) ::utils::typeInfo::create<T>()

  struct sourceLocation
  {
    // The defaulted arguments are necessary so that the builtins are evaluated
    // in the context of the caller. Explicit values should never be provided.

  #ifdef _MSC_VER

    static consteval sourceLocation
    current(u32 line = __builtin_LINE(), u32 column = __builtin_COLUMN(),
      const char *file = __builtin_FILE(), const char *function = __builtin_FUNCSIG())
    {
      return { file, function, line, column };
    }

  #else

    static consteval sourceLocation
    current(std::source_location::T pointer = __builtin_source_location()) noexcept
    {
      auto *data = static_cast<const std::source_location::__impl *>(pointer);
      return { data->_M_file_name, data->_M_function_name, data->_M_line, data->_M_column };
    }

  #endif

    const char *fileName{};
    const char *functionName{};
    u32 line{};
    u32 column{};
  };

  // ghetto unique_ptr implementation
  template<typename T>
  class up
  {
    static constexpr bool is_array = is_array_v<T>;
    using V = remove_extent_t<T>;
  public:
    constexpr up() noexcept = default;
    constexpr up(nullptr_t) noexcept {}
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
      COMPLEX_SWAP_MEMBERS(object_, other);
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
    constexpr V &operator[](usize index) const noexcept requires is_array { return object_[index]; }

    explicit constexpr operator bool() const noexcept { return object_; }

  private:
    V *object_ = nullptr;

    template<typename U> friend class up;
  };

  template<typename T> up(T *pointer) -> up<T>;

  template<typename T, typename U>
  constexpr bool operator==(const up<T> &one, const up<U> &two) noexcept { return one.get() == two.get(); }
  template<typename T>
  constexpr bool operator==(const up<T> &one, nullptr_t) noexcept { return one.get() == nullptr; }
  template<typename T>
  constexpr bool operator==(nullptr_t, const up<T> &two) noexcept { return two.get() == nullptr; }

  // checked array
  // thin abstraction intended to be zero-cost in release builds 
  // but provide checked access for debug ones
  template<typename T>
  struct ca
  {
    T *pointer = nullptr;
  #if COMPLEX_DEBUG
  private:
    [[maybe_unused]] usize size = 0;
  public:
  #endif

    constexpr ca() = default;
    constexpr ca(T *data, [[maybe_unused]] usize dataSize = 0) : pointer(data)
    #if COMPLEX_DEBUG
      , size(dataSize)
    #endif
    { }

    constexpr ca offset(usize offset, [[maybe_unused]] usize explicitSize = 0) noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(offset < size);
      COMPLEX_ASSERT(explicitSize == 0 || explicitSize <= size - offset);
      return ca
      {
        pointer + offset 
      #if COMPLEX_DEBUG
        , (explicitSize) ? explicitSize : size - offset
      #endif
      };
    }

    constexpr T &operator[](usize index) noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(index < size);
      return pointer[index];
    }

    constexpr const T &operator[](usize index) const noexcept
    {
      COMPLEX_ASSERT(pointer);
      COMPLEX_ASSERT(index < size);
      return pointer[index];
    }

    constexpr operator T *() noexcept { return pointer; }
  };

  template<typename T, auto Size>
  constexpr auto 
  find(const T (&array)[Size], const T &element)
  {
    decltype(Size) i = 0;
    for (; i < Size; ++i)
      if (array[i] == element)
        break;

    return &array[i];
  }

  template<typename T, auto Size>
  constexpr auto 
  findIf(const T(&array)[Size], const auto &predicate)
  {
    decltype(Size) i = 0;
    for (; i < Size; ++i)
      if (predicate(array[i]))
        break;

    return &array[i];
  }

  constexpr auto 
  find(auto &container, const auto &element)
  {
    auto begin = container.begin();
    auto end = container.end();
    for (; begin != end; ++begin)
      if (*begin == element)
        break;

    return begin;
  }

  constexpr auto 
  findIf(auto &container, const auto &predicate)
  {
    auto begin = container.begin();
    auto end = container.end();
    for (; begin != end; ++begin)
      if (predicate(*begin))
        break;

    return begin;
  }

  constexpr usize 
  findIndex(auto &container, const auto &element)
  { return (usize)(utils::find(container, element) - container.begin()); }

  constexpr usize 
  findIndexIf(auto &container, const auto &predicate)
  { return (usize)(utils::findIf(container, predicate) - container.begin()); }

  constexpr bool 
  allOf(auto &container, const auto &predicate)
  {
    auto begin = container.begin();
    auto end = container.end();
    for (; begin != end; ++begin)
      if (!predicate(*begin))
        return false;

    return true;
  }

  constexpr bool
  noneOf(auto &container, const auto &predicate)
  {
    auto begin = container.begin();
    auto end = container.end();
    for (; begin != end; ++begin)
      if (predicate(*begin))
        return false;

    return true;
  }

  constexpr void quicksort(auto &container, usize lower, usize upper, const auto &predicate)
  {
    if (lower >= upper)
      return;
    
    COMPLEX_SWAP(container[lower], container[(upper + lower) / 2]);
    const auto &pivot = container[lower];
    usize	m = lower;

    for (usize i = lower + 1; i <= upper; ++i)
    {
      if (predicate(container[i], pivot))
      {
        ++m;
        COMPLEX_SWAP(container[m], container[i]);
      }
    }

    COMPLEX_SWAP(container[lower], container[m]);
    quicksort(container, lower, m - 1, predicate);
    quicksort(container, m + 1, upper, predicate);
  }

  constexpr void quicksort(auto &container, const auto &predicate) 
  { quicksort(container, 0, container.size(), predicate); }


#if COMPLEX_MSVC
  extern "C"
  {
    unsigned char _BitScanReverse(unsigned long *Index, unsigned long Mask);
    unsigned char _BitScanReverse64(unsigned long *Index, unsigned __int64 Mask);
    unsigned char _BitScanForward(unsigned long *Index, unsigned long Mask);
    unsigned char _BitScanForward64(unsigned long *Index, unsigned __int64 Mask);
  #pragma intrinsic(_BitScanReverse, _BitScanReverse64, _BitScanForward, _BitScanForward64)
  }
#endif

  strict_inline u32
  log2(u32 value) noexcept
  {
  #if COMPLEX_GCC || COMPLEX_CLANG
    constexpr u32 kMaxBitIndex = sizeof(u32) * 8 - 1;
    return kMaxBitIndex - __builtin_clz(max(value, u32(1)));
  #elif COMPLEX_MSVC
    unsigned long result = 0;
    _BitScanReverse(&result, value);
    return result;
  #else
    u32 num = 0;
    while (value >>= 1)
      num++;
    return num;
  #endif
  }

  strict_inline usize 
  log2(usize value) noexcept
  {
    COMPLEX_ASSERT(value != 0);
  #if COMPLEX_GCC || COMPLEX_CLANG
    constexpr usize kMaxBitIndex = sizeof(value) * 8 - 1;
    return kMaxBitIndex - __builtin_clzll(utils::max(value, usize(1)));
  #elif COMPLEX_MSVC
    unsigned long result = 0;
    _BitScanReverse64(&result, value);
    return result;
  #else
    usize num = 0;
    while (value >>= 1)
      num++;
    return num;
  #endif
  }

  float sin(float arg);
  float cos(float arg);

  constexpr bool 
  isPowerOfTwo(utils::integral auto value) noexcept
  { return (value & (value - 1)) == 0; }

  template<utils::integral T>
  constexpr T 
  roundUpToMultiple(T i, T factor) noexcept
  { return ((i + factor - 1) / factor) * factor; }

  strict_inline usize 
  getAlignment(const void *pointer) noexcept
  {
    COMPLEX_ASSERT(pointer);
  #if COMPLEX_GCC || COMPLEX_CLANG
    return usize(1) << __builtin_ctzll((usize)pointer);
  #elif COMPLEX_MSVC
    unsigned long result = 0;
    _BitScanForward64(&result, (usize)pointer);
    return usize(1) << result;
  #else
    usize value = (usize)pointer;
    usize num = 0;
    while ((value & 1) == 0)
    {
      value >>= 1;
      num++;
    }
    return usize(1) << num;
  #endif
  }

  template <typename T> 
  constexpr int signum(T val, bool zeroAtCenter = true) noexcept 
  {
    if (zeroAtCenter)
      return (T(0) < val) - (val < T(0));
    else
      return (T(0) <= val) - (val < T(0));
  }

  // returns the starting position of the centered element relative to container
  template<typename T>
  constexpr T 
  centerAxis(T elementRange, T containerRange) noexcept
  { return (containerRange - elementRange) / T(2); }

  template<utils::floating_point T>
  constexpr T 
  unsignFloat(T &value) noexcept
  {
    int sign = signum(value);
    value *= (T)sign;
    return (T)sign;
  }

  template<utils::signed_integral T>
  constexpr T
  unsignInt(T &value) noexcept
  {
    int sign = signum(value);
    value *= sign;
    return (T)sign;
  }

  template<typename T, typename U>
  constexpr T *
  as(U *pointer)
  {
  #if 0
    auto *castPointer = dynamic_cast<T *>(pointer);
    COMPLEX_ASSERT(castPointer, "Unsuccessful cast");
    return castPointer;
  #else
    return static_cast<T *>(pointer);
  #endif
  }

  // malloc replacement
  byte *allocate(usize size, usize alignment = alignof(void *), bool clean = false);
  // free replacement
  void deallocate(const void *memory);

  // acquires a contiguous block of virtual memory
  // cannot be used as a resource yet
  // 
  // size is rounded up to the nearest page size
  byte *reserveMemory(usize &size);
  // assigns pages to a given reserved virtual address space
  // can be used as a resource now
  //
  // size is rounded up to the nearest page size
  // starting at memory, rounded up to the nearest page
  void commitMemory(void *memory, usize &size);
  // unassign ceil(size / pageSize) number of pages,
  // starting at memory, rounded up to the nearest page
  void decommitMemory(void *memory, usize size);
  // relinquish contiguous block of virtual memory
  // 
  // memory MUST point to a block returned by reserveMemory
  // (because of windows VirtualFree)
  // and size MUST be equal to the size, modified by reserveMemory
  // (because of posix munmap)
  void releaseMemory(void *memory, usize size);

  void shrinkWorkingSet();
}
