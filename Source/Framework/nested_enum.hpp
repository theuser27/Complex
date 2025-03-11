#ifndef NESTED_ENUM_HPP
#define NESTED_ENUM_HPP

#define NESTED_ENUM_VERSION_MAJOR 0
#define NESTED_ENUM_VERSION_MINOR 4
#define NESTED_ENUM_VERSION_PATCH 0

// underlying enum type when none is specified
#ifndef NESTED_ENUM_DEFAULT_UNDERLYING_TYPE
  #define NESTED_ENUM_DEFAULT_UNDERLYING_TYPE int
#endif

// user defined type to be used as std::array replacement
// necessary methods: operator[]
#ifndef NESTED_ENUM_ARRAY_TYPE
  #include <array>
  #define NESTED_ENUM_ARRAY_TYPE ::std::array
#endif

// user defined type to be used as std::pair replacement
// necessary methods: none
#ifndef NESTED_ENUM_PAIR_TYPE
  #include <utility>
  #define NESTED_ENUM_PAIR_TYPE ::std::pair
#endif

#ifndef NESTED_ENUM_STRING_VIEW_TYPE
  #include <string_view>
  #define NESTED_ENUM_STRING_VIEW_TYPE ::std::string_view
#endif

#ifndef NESTED_ENUM_OPTIONAL_TYPE
  #include <optional>
  #define NESTED_ENUM_OPTIONAL_TYPE ::std::optional
#endif

#ifndef NESTED_ENUM_TUPLE_TYPE
  #include <tuple>
  #define NESTED_ENUM_TUPLE_TYPE ::std::tuple
#endif

namespace nested_enum
{
  namespace detail
  {
    using size_t = decltype(sizeof(0));
  }

  template<typename T> struct type_identity { using type = T; };

  template <detail::size_t N>
  struct fixed_string
  {
    constexpr fixed_string() = default;
    constexpr fixed_string(const char(&array)[N + 1]) noexcept
    {
      for (detail::size_t i = 0; i < N; ++i)
        storage[i] = array[i];

      storage[N] = '\0';
    }

    constexpr fixed_string(NESTED_ENUM_STRING_VIEW_TYPE view) noexcept
    {
      for (detail::size_t i = 0; i < N; ++i)
        storage[i] = view[i];

      storage[N] = '\0';
    }

    template<detail::size_t M>
    [[nodiscard]] constexpr auto append(fixed_string<M> other) const noexcept
    {
      fixed_string<N + M> result;
      result.storage[N + M] = '\0';

      for (detail::size_t i = 0; i < N; ++i)
        result.storage[i] = storage[i];

      for (detail::size_t i = 0; i < M; ++i)
        result.storage[N + i] = other.storage[i];

      return result;
    }

    // concatenates with null terminator
    template<detail::size_t M>
    [[nodiscard]] constexpr auto append_full(fixed_string<M> other) const noexcept
    {
      fixed_string<N + 1 + M> result;

      for (detail::size_t i = 0; i < N; ++i)
        result.storage[i] = storage[i];
      result.storage[N] = '\0';

      for (detail::size_t i = 0; i < M; ++i)
        result.storage[N + 1 + i] = other.storage[i];
      result.storage[N + 1 + M] = '\0';

      return result;
    }

    [[nodiscard]] constexpr auto operator<=>(const fixed_string &) const = default;
    [[nodiscard]] constexpr operator NESTED_ENUM_STRING_VIEW_TYPE() const noexcept { return { storage, N }; }
    [[nodiscard]] constexpr char *data() noexcept { return storage; }
    [[nodiscard]] constexpr const char *data() const noexcept { return storage; }
    [[nodiscard]] static constexpr detail::size_t size() noexcept { return N; }

    char storage[N + 1];
  };

  template<detail::size_t N>
  fixed_string(const char(&)[N]) -> fixed_string<N - 1>;

  namespace detail
  {
    struct nested_enum_tag {};

    template<typename, typename>
    inline constexpr bool is_same_v = false;
    template<typename T>
    inline constexpr bool is_same_v<T, T> = true;

    template<bool Test, typename True, typename False>
    struct conditional { using type = True; };
    template<typename True, typename False>
    struct conditional<false, True, False> { using type = False; };

    template<typename T> struct remove_reference { using type = T; };
    template<typename T> struct remove_reference<T &> { using type = T; };
    template<typename T> struct remove_reference<T &&> { using type = T; };

    template<typename T> struct remove_cv { using type = T; };
    template<typename T> struct remove_cv<const T> { using type = T; };
    template<typename T> struct remove_cv<volatile T> { using type = T; };
    template<typename T> struct remove_cv<const volatile T> { using type = T; };

    template<typename T> using remove_cvref_t = typename remove_cv<typename remove_reference<T>::type>::type;

    template<class T, template <typename ...> class Template>
    inline constexpr bool is_specialization_v = false;
    template<template<typename...> class Template, class ... Ts>
    inline constexpr bool is_specialization_v<Template<Ts...>, Template> = true;

    template<typename T, typename ... Us>
    inline constexpr bool is_any_of_v = (is_same_v<T, Us> || ...);

    template<typename T>
    inline constexpr bool is_integral_v = is_any_of_v<typename remove_cv<T>::type, bool, char, signed char, unsigned char, wchar_t,
    #ifdef __cpp_char8_t
      char8_t,
    #endif
      char16_t, char32_t, short, unsigned short, int, unsigned int, long, unsigned long, long long, unsigned long long>;

    template<typename T>
    inline constexpr bool is_floating_point_v = is_any_of_v<typename remove_cv<T>::type, float, double, long double>;

    template<typename T, T ... Is>
    struct integer_sequence
    {
      using value_type = T;
      [[nodiscard]] static constexpr size_t size() noexcept { return sizeof...(Is); }
    };

    template<size_t Size>
    using make_index_sequence =
  #if defined (__GNUC__) && !defined (__clang__)
      integer_sequence<size_t, __integer_pack(Size)...>;
  #else
      __make_integer_seq<integer_sequence, size_t, Size>;
  #endif
    template<size_t ... Is>
    using index_sequence = integer_sequence<size_t, Is...>;

    template<typename T>
    struct tuple_size;
    template<typename ... Ts>
    struct tuple_size<NESTED_ENUM_TUPLE_TYPE<Ts...>> { static constexpr auto size = sizeof...(Ts); };
    template<typename T, auto N>
    struct tuple_size<NESTED_ENUM_ARRAY_TYPE<T, N>> { static constexpr auto size = N; };

    template<typename T>
    concept Enum = __is_enum(T);

    template<class T>
    inline constexpr bool is_complete_type_v = requires{ sizeof(T); };

    template<typename ... Ts>
    struct type_list
    {
      static constexpr auto size = sizeof...(Ts);

      template<typename ... Us>
      constexpr auto operator+(type_list<Us...>) const noexcept
      {
        return type_list<Ts..., Us...>{};
      }

      template<typename ... Us>
      constexpr bool operator==(type_list<Us...>) const noexcept
      {
        return is_same_v<type_list<Ts...>, type_list<Us...>>;
      }
    };

    template<typename T, typename ... Ts>
    T get_first_type(type_list<T, Ts...>) { return T{}; }

    template<typename T = int>
    struct opt { bool isInitialised = false; T value = 0; };

    inline constexpr fixed_string scopeResolutionString = "::";

    template<class Container, typename T>
    constexpr auto find_index(const Container &container, const T &value)
    {
      static_assert(requires{ typename Container::value_type; }, "Array type must provide a value_type type-alias");

      size_t index = 0;
      size_t size = container.size();

      if constexpr (is_same_v<typename Container::value_type, NESTED_ENUM_OPTIONAL_TYPE<remove_cvref_t<T>>>)
      {
        for (; index != size; ++index)
          if (container[index].has_value() && container[index].value() == value)
            return NESTED_ENUM_OPTIONAL_TYPE<size_t>{ index };
      }
      else
      {
        static_assert(is_same_v<typename Container::value_type, remove_cvref_t<T>>);

        for (; index != size; ++index)
          if (container[index] == value)
            return NESTED_ENUM_OPTIONAL_TYPE<size_t>{ index };
      }

      return NESTED_ENUM_OPTIONAL_TYPE<size_t>{};
    }

    template<typename E, typename T, opt<T> ... values>
    consteval auto get_array_of_values()
    {
      NESTED_ENUM_ARRAY_TYPE<E, sizeof...(values)> enumValues{};
      opt<T> rawValues[]{ values... };

      size_t index = 0;
      T previous = 0;

      for (auto current : rawValues)
      {
        if (current.isInitialised)
          previous = current.value;
        
        enumValues[index++] = (E)(previous++);
      }

      return enumValues;
    }

    template<fixed_string type, fixed_string value, fixed_string ... values>
    consteval auto get_string_values()
    {
      static_assert(type.size() > 0);
      constexpr auto current = type.append(scopeResolutionString.append(value));

      if constexpr (sizeof...(values) == 0)
        return current.append_full(fixed_string{ "" });
      else
        return current.append_full(get_string_values<type, values...>());
    }

    // bless their soul https://stackoverflow.com/a/54932626
    namespace tuple_magic
    {
      template<typename> struct is_tuple { static constexpr bool value = false; };
      template<typename ... Ts> struct is_tuple<NESTED_ENUM_TUPLE_TYPE<Ts...>> { static constexpr bool value = true; };

      template<typename T>
      constexpr decltype(auto) as_tuple(T t)
      {
        return NESTED_ENUM_TUPLE_TYPE{ t };
      }

      template<typename ...Ts>
      constexpr decltype(auto) as_tuple(NESTED_ENUM_TUPLE_TYPE<Ts...> t)
      {
        return t;
      }

      constexpr decltype(auto) flatten(auto t)
      {
        return t;
      }

      template<typename T>
      constexpr decltype(auto) flatten(NESTED_ENUM_TUPLE_TYPE<T> t)
      {
        return flatten(get<0>(t));
      }

      template<typename ... Ts> requires (!(is_tuple<Ts>::value || ...))
      constexpr decltype(auto) flatten(NESTED_ENUM_TUPLE_TYPE<Ts...> t)
      {
        return t;
      }

      template<typename ... Ts> requires (is_tuple<Ts>::value || ...)
      constexpr decltype(auto) flatten(NESTED_ENUM_TUPLE_TYPE<Ts...> t)
      {
        return [&]<size_t ... Is>(const index_sequence<Is...> &)
        {
          return [](auto...ts) { return flatten(tuple_cat(as_tuple(flatten(ts))...)); }(get<Is>(static_cast<decltype(t) &&>(t))...);
        }(make_index_sequence<sizeof...(Ts)>{});
      }
    }

    template<typename T, auto N>
    constexpr auto tuple_of_arrays_to_array(const NESTED_ENUM_ARRAY_TYPE<T, N> &array) { return array; }

    template<typename ... Ts>
    constexpr auto tuple_of_arrays_to_array(const NESTED_ENUM_TUPLE_TYPE<Ts...> &tupleOfArrays)
    {
      auto function = []<typename T, size_t M, typename ... Args>(NESTED_ENUM_ARRAY_TYPE<T, M> first, Args ... args)
      {
        if constexpr (sizeof...(Args) == 0)
          return first;
        else
        {
          constexpr size_t N = M + (args.size() + ...);

          NESTED_ENUM_ARRAY_TYPE<T, N> newArray;
          size_t index = 0;

          auto iterate = [&index, &newArray](const auto &current)
          {
            for (size_t i = 0; i < current.size(); i++)
              newArray[index + i] = current[i];

            index += current.size();
          };

          iterate(first);
          (iterate(args), ...);

          return newArray;
        }
      };

      return [&]<size_t ... Is>(const detail::index_sequence<Is...> &)
      {
        return function(get<Is>(tupleOfArrays)...);
      }(detail::make_index_sequence<sizeof...(Ts)>{});
    }

    constexpr auto get_substring(NESTED_ENUM_STRING_VIEW_TYPE allStrings, size_t index, bool clean) noexcept
    {
      size_t end = 0;
      size_t count = 0;
      while (count < index)
        if (allStrings[end++] == '\0')
          ++count;

      auto view = NESTED_ENUM_STRING_VIEW_TYPE{ &allStrings[end] };

      if (clean)
      {
        NESTED_ENUM_STRING_VIEW_TYPE scope = "::";
        view.remove_prefix(view.rfind(scope) + scope.length());
      }

      return view;
    }

    template<typename T, typename E>
    concept this_underlying_type = is_same_v<T, typename E::underlying_type>;
  }

  #define TEST_INCLUSIVENESS(selection, type) (selection == All || \
    (!detail::is_complete_type_v<type> && selection == Outer) ||   \
    (detail::is_complete_type_v<type> && ((type::internalIsLeaf_ && selection == Outer) || (!type::internalIsLeaf_ && selection == Inner))))

  // Inner - enum values that are themselves enums
  // Outer - enum values that are NOT enums
  // All   - both inner and outer enum values 
  enum InnerOuterAll { Inner, Outer, All };

  // requires a class be a nested_enum type
  template<class E>
  concept NestedEnum = requires
  {
    typename E::nested_enum_tag;
    requires detail::is_same_v<typename E::nested_enum_tag, detail::nested_enum_tag>;
  };

  template<class E>
  struct nested_enum
  {
    using type = E;

    template<class>
    friend struct nested_enum;
    using nested_enum_tag = detail::nested_enum_tag;

    // returns the reflected type name of the enum 
    static constexpr auto name(bool clean = false) noexcept -> NESTED_ENUM_STRING_VIEW_TYPE
    {
      NESTED_ENUM_STRING_VIEW_TYPE name = E::internalName_;
      if (clean)
      {
        NESTED_ENUM_STRING_VIEW_TYPE scope = "::";
        name.remove_prefix(name.rfind(scope) + scope.length());
      }
      return name;
    }
    // returns the id of the type name (if it has one)
    static constexpr auto id() noexcept -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      if constexpr (requires { typename E::parent; })
        return E::parent::internalEnumIds_[E::internalEnumIndex_];
      else
        return {};
    }
    template<bool UnwrapId = false>
    static constexpr auto name_and_id(bool clean = false)
    {
      if constexpr (UnwrapId)
        return NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, 
          NESTED_ENUM_STRING_VIEW_TYPE>{ name(clean), id().value() };
      else
        return NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, 
          NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>>{ name(clean), id() };
    }
    // returns the integer of the type name (if it has one)
    static constexpr auto integer() noexcept
    {
      return static_cast<typename E::underlying_type>(E::value());
    }
    // returns an instance of this type with the given value if it exist
    template<typename T> requires detail::is_integral_v<T> || detail::is_floating_point_v<T>
    static constexpr auto make_enum(T t) -> NESTED_ENUM_OPTIONAL_TYPE<E>
    {
      return enum_value(static_cast<typename E::underlying_type>(t));
    }


    // returns the string of the currently held value
    constexpr auto enum_name(bool clean = false) const noexcept -> NESTED_ENUM_STRING_VIEW_TYPE
    {
      return enum_name(static_cast<const E &>(*this), clean).value();
    }
    // returns the id of the currently held value
    constexpr auto enum_id() const noexcept -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      return enum_id(static_cast<const E &>(*this));
    }
    // returns the string and id of the currently held value
    template<bool UnwrapId = false>
    constexpr auto enum_name_and_id(bool clean = false) const noexcept
    {
      auto value = enum_name_and_id(static_cast<const E &>(*this), clean).value();
      if constexpr (UnwrapId)
        return NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, 
          NESTED_ENUM_STRING_VIEW_TYPE>{ value.first, value.second.value() };
      else
        return value;
    }
    // returns the integer representation of the currently held value
    constexpr auto enum_integer() const noexcept
    {
      return (typename E::underlying_type)static_cast<const E &>(*this).internal_value;
    }

  protected:
    template<detail::size_t N>
    static constexpr auto create_name(fixed_string<N> name)
    {
      if constexpr (requires { typename E::parent; })
        return E::parent::internalName_.append(detail::scopeResolutionString.append(name));
      else
        return name;
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<auto Filter, typename ... Ts>
    static constexpr auto get_needed_values(detail::type_list<Ts...>) noexcept
    {
      constexpr detail::size_t size = ((Filter.template operator()<Ts>() ? 
        detail::size_t(1) : detail::size_t(0)) + ...);
      detail::size_t index = 0;
      detail::size_t counter = 0;
      NESTED_ENUM_ARRAY_TYPE<detail::size_t, size> valuesNeeded{};

      auto getValues = [&]<typename T>()
      {
        if (Filter.template operator()<T>())
          valuesNeeded[counter++] = index;
        index++;
      };
      (getValues.template operator()<Ts>(), ...);

      return valuesNeeded;
    }
  public:
    // returns the values inside this enum that satisfy the Selection
    template<auto FilterPredicate>
    static constexpr auto enum_values_filter() noexcept
    {
      if constexpr (E::internalEnumValues_.size() == 0)
        return NESTED_ENUM_ARRAY_TYPE<E, 0>{};
      else
      {
        constexpr auto valuesNeeded = get_needed_values<FilterPredicate>(E::internalSubtypes_);
        constexpr auto result = [&]<detail::size_t ... I>(const detail::index_sequence<I...> &)
        {
          return []<detail::size_t ... J>()
          {
            return NESTED_ENUM_ARRAY_TYPE{ E{ E::internalEnumValues_[J] }... };
          }.template operator()<valuesNeeded[I]...>();
        }(detail::make_index_sequence<valuesNeeded.size()>{});

        return result;
      }
    }
    // returns the values inside this enum that satisfy the Selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_values() noexcept
    {
      if constexpr (Selection == All)
      {
        constexpr auto enumValues = []<detail::size_t ... I>(const detail::index_sequence<I...> &)
        {
          return NESTED_ENUM_ARRAY_TYPE{ E{ E::internalEnumValues_[I] }... };
        }(detail::make_index_sequence<E::internalEnumValues_.size()>{});
        return enumValues;
      }
      else 
        return enum_values_filter<[]<typename T>() { return TEST_INCLUSIVENESS(Selection, T); }>();
    }
    // returns the values count inside this enum that satisfy the selection
    static constexpr auto enum_count(InnerOuterAll selection = All) noexcept -> detail::size_t
    {
      if constexpr (E::internalEnumValues_.size() == 0)
        return 0;
      else
      {
        constexpr auto getSize = []<typename ... Ts>(InnerOuterAll innerSelection, const detail::type_list<Ts...> &)
        {
          return ((TEST_INCLUSIVENESS(innerSelection, Ts) ? detail::size_t(1) : detail::size_t(0)) + ...);
        };
        constexpr auto innerSize = getSize(Inner, E::internalSubtypes_);
        constexpr auto outerSize = getSize(Outer, E::internalSubtypes_);

        if (selection == Inner)
          return innerSize;
        if (selection == Outer)
          return outerSize;

        return E::internalEnumValues_.size();
      }
    }
    // returns the values count inside this enum that satisfy the selection
    static constexpr auto enum_count_filter(const auto &FilterPredicate) noexcept -> detail::size_t
    {
      if constexpr (E::internalEnumValues_.size() == 0)
        return 0;
      else
      {
        return [&]<typename ... Ts>(const detail::type_list<Ts...> &)
        {
          return ((FilterPredicate.template operator()<Ts>() ? detail::size_t(1) : detail::size_t(0)) + ...);
        }(E::internalSubtypes_);
      }      
    }    
    // returns the underlying integers of enum values that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_integers() noexcept
    {
      constexpr auto integerArray = []()
      {
        auto enumValues = enum_values<Selection>();
        NESTED_ENUM_ARRAY_TYPE<typename E::underlying_type, enumValues.size()> result;
        for (detail::size_t i = 0; i < result.size(); i++)
          result[i] = (typename E::underlying_type)enumValues[i].internal_value;

        return result;
      }();

      return integerArray;
    }
    template<auto FilterPredicate>
    static constexpr auto enum_integers_filter() noexcept
    {
      constexpr auto integerArray = []()
      {
        auto enumValues = enum_values_filter<FilterPredicate>();
        NESTED_ENUM_ARRAY_TYPE<typename E::underlying_type, enumValues.size()> result;
        for (detail::size_t i = 0; i < result.size(); i++)
          result[i] = (typename E::underlying_type)enumValues[i].internal_value;

        return result;
      }();

      return integerArray;
    }
    template<auto FilterPredicate, bool UnwrapIds = false>
    static constexpr auto enum_ids_filter() noexcept
    {
      using id_type = typename detail::conditional<UnwrapIds, NESTED_ENUM_STRING_VIEW_TYPE, 
        NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>>::type;

      if constexpr (E::internalEnumIds_.size() == 0)
      {
        if constexpr (UnwrapIds)
        {
          // msvc will fail if we immediately return an array...
          auto values = NESTED_ENUM_ARRAY_TYPE<id_type, 0>{};
          return values;
        }
        else
          return E::internalEnumIds_;
      }
      else
      {
        constexpr auto result = []()
        {
          constexpr auto valuesNeeded = get_needed_values<FilterPredicate>(E::internalSubtypes_);
          NESTED_ENUM_ARRAY_TYPE<id_type, valuesNeeded.size()> values;
          for (detail::size_t i = 0; i < values.size(); i++)
          {
            if constexpr (UnwrapIds)
              values[i] = E::internalEnumIds_[valuesNeeded[i]].value();
            else
              values[i] = E::internalEnumIds_[valuesNeeded[i]];
          }
          return values;
        }();

        return result;
      }
    }
    // returns the ids of enum values that satisfy the selection
    template<InnerOuterAll Selection = All, bool UnwrapIds = false>
    static constexpr auto enum_ids() noexcept
    {
      if constexpr (Selection == All && !UnwrapIds)
        return E::internalEnumIds_;
      else
        return enum_ids_filter<[]<typename T>() { return TEST_INCLUSIVENESS(Selection, T); }, UnwrapIds>();
    }
    template<auto FilterPredicate>
    static constexpr auto enum_names_filter(bool clean = false) noexcept
    {
      if constexpr (enum_count_filter(FilterPredicate) == 0)
      {
        // if this isn't assigned to a constexpr variable msvc won't recognise the function as constexpr
        constexpr auto enumNames = NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, 0>{};
        return enumNames;
      }
      else
      {
        constexpr auto generateStrings = [](bool cleanString)
        {
          constexpr auto valuesNeeded = get_needed_values<FilterPredicate>(E::internalSubtypes_);
          NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, valuesNeeded.size()> values;
          for (detail::size_t i = 0; i < values.size(); i++)
            values[i] = detail::get_substring(E::internalEnumNames_, valuesNeeded[i], cleanString);
          
          return values;
        };

        constexpr auto enumNames = generateStrings(false);
        constexpr auto enumNamesClean = generateStrings(true);
        if (!clean)
          return enumNames;
        else
          return enumNamesClean;
      }
    }
    // returns the reflected strings of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_names(bool clean = false) noexcept
    {
      if constexpr (Selection == All)
        return enum_names_filter<[]<typename T>() { return true; }>(clean);
      else
        return enum_names_filter<[]<typename T>() { return TEST_INCLUSIVENESS(Selection, T); }>(clean);
    }
    template<auto FilterPredicate, bool UnwrapIds = false>
    static constexpr auto enum_names_and_ids_filter(bool clean = false) noexcept
    {
      using id_type = typename detail::conditional<UnwrapIds, NESTED_ENUM_STRING_VIEW_TYPE, 
        NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>>::type;

      if constexpr (enum_count_filter(FilterPredicate) == 0)
        return NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, id_type>, 0>{};
      else
      {
        constexpr auto stringsAndIds = []()
        {
          constexpr auto valuesNeeded = get_needed_values<FilterPredicate>(E::internalSubtypes_);

          NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, id_type>, valuesNeeded.size()> values{};
          for (detail::size_t i = 0; i < values.size(); i++)
          {
            if constexpr (UnwrapIds)
              values[i] = { detail::get_substring(E::internalEnumNames_, valuesNeeded[i], false), E::internalEnumIds_[valuesNeeded[i]].value() };
            else
              values[i] = { detail::get_substring(E::internalEnumNames_, valuesNeeded[i], false), E::internalEnumIds_[valuesNeeded[i]] };
          }

          return values;
        }();

        constexpr auto stringsAndIdsClean = [&]()
        {
          NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, id_type>, stringsAndIds.size()> values;

          for (detail::size_t i = 0; i < stringsAndIds.size(); ++i)
          {
            auto string = stringsAndIds[i].first;
            string.remove_prefix(string.rfind(detail::scopeResolutionString) + detail::scopeResolutionString.size());
            values[i] = NESTED_ENUM_PAIR_TYPE{ string, stringsAndIds[i].second };
          }

          return values;
        }();

        if (!clean)
          return stringsAndIds;
        else
          return stringsAndIdsClean;
      }
    }
    // returns the reflected strings and ids of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = All, bool UnwrapIds = false>
    static constexpr auto enum_names_and_ids(bool clean = false) noexcept
    {
      if constexpr (Selection == All)
        return enum_names_and_ids_filter<[]<typename T>() { return true; }, UnwrapIds>(clean);
      else
        return enum_names_and_ids_filter<[]<typename T>() { return TEST_INCLUSIVENESS(Selection, T); }, UnwrapIds>(clean);
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<auto FilterPredicate>
    static constexpr auto enum_subtypes_filter() noexcept
    {
      if constexpr (E::internalEnumValues_.size() == 0)
        return NESTED_ENUM_TUPLE_TYPE<>{};
      else
      {
        constexpr auto list = []<typename ... Ts>(detail::type_list<Ts...>)
        {
          constexpr auto filter = []<typename T>()
          {
            if constexpr (FilterPredicate.template operator()<T>())
              return detail::type_list<T>{};
            else
              return detail::type_list<>{};
          };
          return (filter.template operator()<Ts>() + ...);
        }(E::internalSubtypes_);

        return []<typename ... Ts>(detail::type_list<Ts...>)
        {
          return NESTED_ENUM_TUPLE_TYPE<type_identity<Ts>...>{};
        }(list);
      }
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_subtypes() noexcept
    {
      return enum_subtypes_filter<[]<typename T>() { return TEST_INCLUSIVENESS(Selection, T); }>();
    }

    static constexpr detail::size_t enum_count_filter_recursive(const auto &filterPredicate) noexcept
    {
      auto recurse = [&]<typename ... Ts>([[maybe_unused]] const auto &self, detail::type_list<Ts...>)
      {
        if constexpr (sizeof...(Ts) == 0)
          return 0;
        else
        {
          auto subtypesSizes = [&]<typename T>() -> detail::size_t
          {
            detail::size_t isIncluded = filterPredicate.template operator()<T>(); 

            if constexpr (detail::is_complete_type_v<T>)
              return isIncluded + self(self, T::internalSubtypes_);
            else 
              return isIncluded;
          };

          return (subtypesSizes.template operator()<Ts>() + ...);
        }
      };
      return recurse(recurse, E::internalSubtypes_);
    }
    // returns are recursive count of all enum values in the subtree that satisfy the selection
    static constexpr detail::size_t enum_count_recursive(InnerOuterAll selection = All) noexcept
    {
      return enum_count_filter_recursive([&]<typename T>()
        {
          if (selection == All)
            return detail::size_t(1);

          if constexpr (detail::is_complete_type_v<T>)
          {
            if (!T::internalIsLeaf_)
              return detail::size_t(selection == Inner);
          }
          
          return detail::size_t(selection == Outer);
        });
    }
  private:
    template<auto Predicate, auto FilterPredicate>
    static constexpr auto return_recursive_internal() noexcept
    {
      if constexpr (E::internalEnumValues_.size() == 0)
        return NESTED_ENUM_TUPLE_TYPE<>{};
      else
      {
        constexpr auto values = Predicate.template operator()<E>();

        // getting an array of values we need
        constexpr auto subtypesToQuery = [&]<typename ... Ts>(detail::type_list<Ts...>)
        {
          // new tuple of only the inner enum nodes
          auto checkQueries = []<typename T>()
          {
            // does the enum exist
            if constexpr (!detail::is_complete_type_v<T>)
              return detail::type_list<>{};
            else
            {
              // does the enum have any values
              if constexpr (T::internalSubtypes_.size == 0)
                return detail::type_list<>{};
              else if constexpr (FilterPredicate.template operator()<T>())
                return detail::type_list<T>{};
              else
                return detail::type_list<>{};
            }
          };

          return (checkQueries.template operator()<Ts>() + ...);
        }(E::internalSubtypes_);

        // returning only the values if we don't have any subtypes to query
        if constexpr (subtypesToQuery.size == 0)
          return NESTED_ENUM_TUPLE_TYPE{ values };
        else
        {
          constexpr auto subtypesTuples = []<typename ... Ts>(detail::type_list<Ts...>)
          {
            auto expand = []<typename T>() { return T::template return_recursive_internal<Predicate, FilterPredicate>(); };
            return NESTED_ENUM_TUPLE_TYPE{ expand.template operator()<Ts>()... };
          }(subtypesToQuery);

          if constexpr (detail::tuple_size<detail::remove_cvref_t<decltype(values)>>::size == 0)
            return subtypesTuples;
          else
            return tuple_cat(NESTED_ENUM_TUPLE_TYPE{ values }, subtypesTuples);
        }
      }
    }
    template<auto Predicate, InnerOuterAll Selection>
    static constexpr auto return_recursive_selection() noexcept
    {
      return return_recursive_internal<Predicate, []<typename T>()
      {
        // different branches based on what we want
        if constexpr ((Selection == All) || (Selection == Outer))
          return true;
        else
        {
          auto checkInner = []<typename ... Us>(detail::type_list<Us...>)
          {
            auto checkIndividual = []<typename U>() -> bool
            {
              if constexpr (detail::is_complete_type_v<U>)
                return !U::internalIsLeaf_;
              else
                return false;
            };
            return false || (checkIndividual.template operator()<Us>() || ...);
          };

          if constexpr (checkInner(T::internalSubtypes_))
            return true;
          else
            return false;
        }
      }>();
    }
  public:
    // returns NESTED_ENUM_TUPLE_TYPE<NESTED_ENUM_ARRAY_TYPE<Enum, ?>...> 
    // of all enum values in the subtree that satisfy the selection
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_values_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_values<Selection>();
      };

      constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>());
      return tuple;
    }
    template<auto FilterPredicate>
    static constexpr auto enum_values_filter_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_values_filter<FilterPredicate>();
      };

      constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_internal<predicate, 
        []<typename T>(){ return true; }>());
      return tuple;
    }
    // returns NESTED_ENUM_TUPLE_TYPE<NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, ?>...> 
    // of the reflected names of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all strings inside
    template<InnerOuterAll Selection = All, bool FlattenTuple = true, bool Clean = false>
    static constexpr auto enum_names_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names<Selection>(Clean);
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>());
        return tuple;
      }
    }
    template<auto FilterPredicate, bool FlattenTuple = true, bool Clean = false>
    static constexpr auto enum_names_filter_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names_filter<FilterPredicate>(Clean);
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>());
        return tuple;
      }
    }
    // returns NESTED_ENUM_TUPLE_TYPE<NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>, ?>...> 
    // of the ids of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all strings inside
    template<InnerOuterAll Selection = All, bool FlattenTuple = true>
    static constexpr auto enum_ids_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_ids<Selection>();
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>());
        return tuple;
      }
    }
    template<auto FilterPredicate, bool FlattenTuple = true>
    static constexpr auto enum_ids_filter_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_ids_filter<FilterPredicate>();
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>());
        return tuple;
      }
    }
    // returns NESTED_ENUM_TUPLE_TYPE<NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>>, ?>...>
    // of the ids and reflected strings of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all pairs of strings inside
    template<InnerOuterAll Selection = All, bool FlattenTuple = true, bool Clean = false>
    static constexpr auto enum_names_and_ids_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names_and_ids<Selection>(Clean);
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(
          detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>());
        return tuple;
      }
    }
    template<auto FilterPredicate, bool FlattenTuple = true, bool Clean = false>
    static constexpr auto enum_names_and_ids_filter_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_names_and_ids_filter<FilterPredicate>(Clean);
      };

      if constexpr (FlattenTuple)
      {
        constexpr auto flattenedTuple = detail::tuple_of_arrays_to_array(detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>()));
        return flattenedTuple;
      }
      else
      {
        constexpr auto tuple = detail::tuple_magic::flatten(
          return_recursive_internal<predicate, []<typename T>(){ return true; }>());
        return tuple;
      }
    }
    template<InnerOuterAll Selection = All>
    static constexpr auto enum_subtypes_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_subtypes<Selection>();
      };

      constexpr auto tuple = detail::tuple_magic::flatten(return_recursive_selection<predicate, Selection>());
      return tuple;
    }
    template<auto FilterPredicate>
    static constexpr auto enum_subtypes_filter_recursive() noexcept
    {
      constexpr auto predicate = []<typename T>()
      {
        return T::template enum_subtypes_filter<FilterPredicate>();
      };

      constexpr auto tuple = detail::tuple_magic::flatten(
        return_recursive_internal<predicate, []<typename T>(){ return true; }>());
      return tuple;
    }

    // returns the reflected string of an enum value of this type
    static constexpr auto enum_name(E value, bool clean = false) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      auto index = detail::find_index(E::internalEnumValues_, value.internal_value);
      if (!index.has_value())
        return {};

      return enum_names<All>(clean)[index.value()];
    }
    // returns the reflected string of an enum value of this type, specified by its id
    static constexpr auto enum_name_by_id(NESTED_ENUM_STRING_VIEW_TYPE id, bool clean = false) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      auto index = detail::find_index(E::internalEnumIds_, id);
      if (!index.has_value())
        return {};

      return enum_names(clean)[index.value()];
    }
    // returns the id of an enum value of this type
    static constexpr auto enum_id(E value) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      auto index = detail::find_index(E::internalEnumValues_, value.internal_value);
      if (!index.has_value())
        return {};

      return enum_ids<All>()[index.value()];
    }
    // returns the id of an enum value of this type, specified by its reflected string
    static constexpr auto enum_id(NESTED_ENUM_STRING_VIEW_TYPE enumName) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      constexpr auto strings = enum_names<All>();
      auto index = detail::find_index(strings, enumName);
      if (!index.has_value())
        return {};

      return enum_ids<All>()[index.value()];
    }
    // returns the reflected string and id of an enum value of this type
    template<bool UnwrapId = false>
    static constexpr auto enum_name_and_id(E value, bool clean = false)
    {
      using id_type = typename detail::conditional<UnwrapId, NESTED_ENUM_STRING_VIEW_TYPE, 
        NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>>::type;

      auto index = detail::find_index(E::internalEnumValues_, value.internal_value);
      if (!index.has_value())
        return NESTED_ENUM_PAIR_TYPE<NESTED_ENUM_STRING_VIEW_TYPE, id_type>{};

      return enum_names_and_ids<All, UnwrapId>(clean)[index.value()];
    }
    // returns the underlying integer of an enum value of this type
    static constexpr auto enum_integer(E value)
    {
      return NESTED_ENUM_OPTIONAL_TYPE{ (typename E::underlying_type)value.internal_value };
    }
    // returns the underlying integer of an enum value of this type, specified by its reflected string
    static constexpr auto enum_integer(NESTED_ENUM_STRING_VIEW_TYPE enumName)
    {
      constexpr auto enumNames = enum_names<All>(false);
      auto index = detail::find_index(enumNames, enumName);
      if (!index.has_value())
        return NESTED_ENUM_OPTIONAL_TYPE<typename E::underlying_type>{};

      return NESTED_ENUM_OPTIONAL_TYPE{ (typename E::underlying_type)E::internalEnumValues_[index.value()] };
    }
    // returns the underlying integer of an enum value of this type, specified by its id
    static constexpr auto enum_integer_by_id(NESTED_ENUM_STRING_VIEW_TYPE id)
    {
      auto index = detail::find_index(E::internalEnumIds_, id);
      if (!index.has_value())
        return NESTED_ENUM_OPTIONAL_TYPE<typename E::underlying_type>{};

      return NESTED_ENUM_OPTIONAL_TYPE{ (typename E::underlying_type)E::internalEnumValues_[index.value()] };
    }
    // returns the underlying integer of an enum value of this type
    static constexpr auto enum_index(E value) -> NESTED_ENUM_OPTIONAL_TYPE<detail::size_t>
    {
      return detail::find_index(E::internalEnumValues_, value);
    }
    // returns the underlying integer of an enum value of this type, specified by its reflected string
    static constexpr auto enum_index(NESTED_ENUM_STRING_VIEW_TYPE enumName) -> NESTED_ENUM_OPTIONAL_TYPE<detail::size_t>
    {
      return detail::find_index(enum_names<All>(false), enumName);
    }
    // returns the underlying integer of an enum value of this type, specified by its id
    static constexpr auto enum_index_by_id(NESTED_ENUM_STRING_VIEW_TYPE id) -> NESTED_ENUM_OPTIONAL_TYPE<detail::size_t>
    {
      return detail::find_index(E::internalEnumIds_, id);
    }
    // returns the enum value of this type, specified by an integer
    static constexpr auto enum_value(detail::this_underlying_type<E> auto integer)
    {
      for (auto value : E::internalEnumValues_)
        if (value == integer)
          return NESTED_ENUM_OPTIONAL_TYPE<E>{ value };

      return NESTED_ENUM_OPTIONAL_TYPE<E>{};
    }
    // returns the enum value of this type, specified by its reflected string
    static constexpr auto enum_value(NESTED_ENUM_STRING_VIEW_TYPE enumName)
    {
      constexpr auto enumNames = enum_names<All>(false);
      auto index = detail::find_index(enumNames, enumName);
      if (!index.has_value())
        return NESTED_ENUM_OPTIONAL_TYPE<E>{};

      return NESTED_ENUM_OPTIONAL_TYPE<E>{ E::internalEnumValues_[index.value()] };
    }
    // returns the enum value of this type, specified by its id
    static constexpr auto enum_value_by_id(NESTED_ENUM_STRING_VIEW_TYPE id)
    {
      auto index = detail::find_index(E::internalEnumIds_, id);
      if (!index.has_value())
        return NESTED_ENUM_OPTIONAL_TYPE<E>{};

      return NESTED_ENUM_OPTIONAL_TYPE<E>{ E::internalEnumValues_[index.value()] };
    }
  private:
    // predicate is a lambda that takes the current nested_enum struct as type template parameter and
    // returns a bool whether this is the searched for type (i.e. the enum value searched for is contained inside)
    template<auto Predicate, typename ... Ts>
    static constexpr auto find_type_recursive_internal(detail::type_list<Ts...>)
    {
      if constexpr (Predicate.template operator()<E>())
        return detail::type_list<E>{};
      else
      {
        if constexpr (sizeof...(Ts) == 0)
          return detail::type_list<>{};
        else
        {
          constexpr auto getResults = []<typename T>()
          {
            if constexpr (detail::is_complete_type_v<T>)
              return T::template find_type_recursive_internal<Predicate>(T::internalSubtypes_);
            else
              return detail::type_list<>{};
          };
          
          // for some reason msvc expands this branch even if sizeof...(Ts) IS 0
          // so we need to guard this expansion behind another function
          // just msvc things i guess *sigh*
        #ifdef _MSC_VER
          constexpr auto subtypesResult = [&]<typename ... Us>()
          {
            if constexpr (sizeof...(Us) == 0)
              return detail::type_list<>{};
            else
              return (getResults.template operator()<Us>() + ...);
          }.template operator()<Ts...>();
        #else
          constexpr auto subtypesResult = (getResults.template operator()<Ts>() + ...);
        #endif

          if constexpr (subtypesResult.size > 0)
          {
          #ifndef NESTED_ENUM_ALLOW_MULTIPLE_RESULTS
            static_assert(subtypesResult.size == 1, "Multiple results found for query, if this is expected \
              define NESTED_ENUM_SUPPRESS_MULTIPLE_RESULTS_ASSERT to 1 before including the header");
          #endif

            return subtypesResult;
          }
          else
            return detail::type_list<>{};
        }
      }
    }
  public:
    // returns the reflected string of an enum value that is located somewhere in the subtree
    static constexpr auto enum_name_recursive(detail::Enum auto value, bool clean = false) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        return detail::is_same_v<decltype(value), typename T::Value>;
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return {};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_name(value, clean);
      }
    }
    static constexpr auto enum_name_recursive(NestedEnum auto value, bool clean = false) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      return enum_name_recursive(value.internal_value, clean);
    }
    // returns the reflected string of an enum with the specified id that is located somewhere in the subtree
    // the algorithm is a top-down DFS (in case you have repeating ids, which is not a good idea)
    static constexpr auto enum_name_by_id_recursive(NESTED_ENUM_STRING_VIEW_TYPE id, bool clean = false) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      if (auto value = enum_name_by_id(id, clean); value.has_value())
        return value;

      if constexpr (E::internalEnumValues_.size() == 0)
        return {};
      else
      {
        return []<typename ... Ts>(NESTED_ENUM_STRING_VIEW_TYPE id, bool clean, detail::type_list<Ts...>)
        {
          constexpr auto recurse = []<auto Self, typename U, typename ... Us>(NESTED_ENUM_STRING_VIEW_TYPE id, bool clean)
          {
            auto value = U::enum_name_by_id_recursive(id, clean);
            if (value.has_value())
              return value;

            if constexpr (sizeof...(Us) > 0)
              return Self.template operator()<Self, Us...>(id, clean);
            else
              return NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>{};
          };

          return recurse.template operator()<recurse, Ts...>(id, clean);
        }(id, clean, E::internalSubtypes_);
      }
    }
    // returns the id of an enum value that is located somewhere in the subtree
    static constexpr auto enum_id_recursive(detail::Enum auto value) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        return detail::is_same_v<decltype(value), typename T::Value>;
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return {};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_id(value);
      }
    }
    static constexpr auto enum_id_recursive(NestedEnum auto value) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      return enum_id_recursive(value.internal_value);
    }
    // returns the id of an enum with the specified reflected string that is located somewhere in the subtree
    static constexpr auto enum_id_recursive(NESTED_ENUM_STRING_VIEW_TYPE enumName) -> NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>
    {
      if (auto value = enum_id(enumName); value.has_value())
        return value;

      if constexpr (E::internalEnumValues_.size() == 0)
        return {};
      else
      {
        return []<typename ... Ts>(NESTED_ENUM_STRING_VIEW_TYPE enumName, detail::type_list<Ts...>)
        {
          constexpr auto recurse = []<auto Self, typename U, typename ... Us>(NESTED_ENUM_STRING_VIEW_TYPE enumName)
          {
            auto value = U::enum_id_recursive(enumName);
            if (value.has_value())
              return value;

            if constexpr (sizeof...(Us) > 0)
              return Self.template operator()<Self, Us...>(enumName);
            else
              return NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>{};
          };

          return recurse.template operator()<recurse, Ts...>(enumName);
        }(enumName, E::internalSubtypes_);
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its reflected string
    // make sure to provide the full enum name to avoid erroneous results
    template<fixed_string enumName>
    static constexpr auto enum_integer_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto enumInteger = T::enum_integer(NESTED_ENUM_STRING_VIEW_TYPE{ enumName });
        return enumInteger.has_value();
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return NESTED_ENUM_OPTIONAL_TYPE<typename E::underlying_type>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_integer(NESTED_ENUM_STRING_VIEW_TYPE{ enumName });
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_integer_by_id_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_integer_by_id(NESTED_ENUM_STRING_VIEW_TYPE{ id });
        return value.has_value();
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return NESTED_ENUM_OPTIONAL_TYPE<typename E::underlying_type>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_integer_by_id(NESTED_ENUM_STRING_VIEW_TYPE{ id });
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its reflected string
    template<fixed_string enumName>
    static constexpr auto enum_value_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_value(NESTED_ENUM_STRING_VIEW_TYPE{ enumName });
        return value.has_value();
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return NESTED_ENUM_OPTIONAL_TYPE<E>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_value(NESTED_ENUM_STRING_VIEW_TYPE(enumName));
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_value_by_id_recursive()
    {
      auto typeId = find_type_recursive_internal<[]<typename T>()
      {
        constexpr auto value = T::enum_value_by_id(NESTED_ENUM_STRING_VIEW_TYPE{ id });
        return value.has_value();
      }>(E::internalSubtypes_);

      if constexpr (typeId.size == 0)
        return NESTED_ENUM_OPTIONAL_TYPE<E>{};
      else
      {
        using subType = decltype(detail::get_first_type(typeId));
        return subType::enum_value_by_id(NESTED_ENUM_STRING_VIEW_TYPE{ id });
      }
    }
  };

  template <NestedEnum E>
  constexpr bool operator==(const E &left, const E &right) noexcept
  {
    return left.internal_value == right.internal_value;
  }

  template <NestedEnum E>
  constexpr bool operator==(const E &left, const typename E::Value &right) noexcept
  {
    return left.internal_value == right;
  }

  template <NestedEnum E>
  constexpr bool operator==(const typename E::Value &left, const E &right) noexcept
  {
    return left == right.internal_value;
  }
}

#undef TEST_INCLUSIVENESS
#define NESTED_ENUM_INTERNAL_EXPAND(...) NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(NESTED_ENUM_INTERNAL_EXPAND4(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND4(...) NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(NESTED_ENUM_INTERNAL_EXPAND3(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND3(...) NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(NESTED_ENUM_INTERNAL_EXPAND2(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND2(...) NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(NESTED_ENUM_INTERNAL_EXPAND1(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND1(...) __VA_ARGS__

// first one gets used to define horizonal things while this is used for the depth
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(NESTED_ENUM_INTERNAL_EXPAND_SECOND4(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND4(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(NESTED_ENUM_INTERNAL_EXPAND_SECOND3(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND3(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(NESTED_ENUM_INTERNAL_EXPAND_SECOND2(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND2(...) NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(NESTED_ENUM_INTERNAL_EXPAND_SECOND1(__VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_EXPAND_SECOND1(...) __VA_ARGS__

#define NESTED_ENUM_INTERNAL_PARENS ()
#define NESTED_ENUM_INTERNAL_EMPTY()

#define NESTED_ENUM_INTERNAL_EMPTY_STACK() NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY4(NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY4(...) NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(NESTED_ENUM_INTERNAL_EMPTY3(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY3(...) NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(NESTED_ENUM_INTERNAL_EMPTY2(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY2(...) NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(NESTED_ENUM_INTERNAL_EMPTY1(__VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY))))
#define NESTED_ENUM_INTERNAL_EMPTY1(...) __VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY

#define NESTED_ENUM_INTERNAL_PAREN_STACK() NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(NESTED_ENUM_INTERNAL_PAREN4(()))))
#define NESTED_ENUM_INTERNAL_PAREN4(...) NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(NESTED_ENUM_INTERNAL_PAREN3(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN3(...) NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(NESTED_ENUM_INTERNAL_PAREN2(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN2(...) NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(NESTED_ENUM_INTERNAL_PAREN1(() __VA_ARGS__))))
#define NESTED_ENUM_INTERNAL_PAREN1(...) () __VA_ARGS__

#define NESTED_ENUM_INTERNAL_DEFER(...)  __VA_ARGS__ NESTED_ENUM_INTERNAL_EMPTY_STACK() NESTED_ENUM_INTERNAL_PAREN_STACK()
#define NESTED_ENUM_INTERNAL_DEFER_ADD(id) NESTED_ENUM_INTERNAL_EMPTY NESTED_ENUM_INTERNAL_EMPTY id () ()


// this person is a genius https://stackoverflow.com/a/62984543
// the last macro definition is a little messed up but at least it keeps the naming scheme 
#define NESTED_ENUM_INTERNAL_DEPAREN(...) NESTED_ENUM_INTERNAL_ESC(NESTED_ENUM_INTERNAL_ISH __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_ISH(...) NESTED_ENUM_INTERNAL_ISH __VA_ARGS__
#define NESTED_ENUM_INTERNAL_ESC(...) NESTED_ENUM_INTERNAL_ESC_(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_ESC_(...) NESTED_ENUM_INTERNAL_VAN ## __VA_ARGS__
#define NESTED_ENUM_INTERNAL_VANNESTED_ENUM_INTERNAL_ISH


#define NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(...) #__VA_ARGS__
#define NESTED_ENUM_INTERNAL_STRINGIFY(...) NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL1(getter, ...) getter(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL(getter, options, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL1(getter, __VA_ARGS__ __VA_OPT__(,) NESTED_ENUM_INTERNAL_DEPAREN(options))
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY1(...) NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY(options, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY1(__VA_OPT__(,) NESTED_ENUM_INTERNAL_DEPAREN(options))


#define NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST(one, ...) one
#define NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST_(one, ...) __VA_ARGS__
#define NESTED_ENUM_INTERNAL_VALUE_IF_MISSING(one, ...) NESTED_ENUM_INTERNAL_VALUE_IF_NOT_TEST## __VA_OPT__(_) (one, __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(a1, ...) a1
#define NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY(a1, a2, ...) a2
#define NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY(a1, a2, a3, ...) a3
#define NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(a1, ...) __VA_ARGS__


#define NESTED_ENUM_INTERNAL_FOR_EACH(helper, macro, ...) __VA_OPT__(NESTED_ENUM_INTERNAL_EXPAND(helper(macro, __VA_ARGS__)))

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE(macro, separator, a1, ...)                                                                       \
    macro (NESTED_ENUM_INTERNAL_DEPAREN(a1)) __VA_OPT__(NESTED_ENUM_INTERNAL_DEPAREN(separator))                                                                 \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, separator, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE

// used only to interleave the entry definitions (typename, code) and specialisations
#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO(sequence, item, ...)                                                                      \
    (NESTED_ENUM_INTERNAL_GET_TYPE(NESTED_ENUM_INTERNAL_DEPAREN(item)), NESTED_ENUM_INTERNAL_EXTRACT_CODE(NESTED_ENUM_INTERNAL_DEPAREN(item)), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY sequence)            \
    __VA_OPT__(, NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN NESTED_ENUM_INTERNAL_PARENS ((NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY sequence), __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN() NESTED_ENUM_INTERNAL_INTERLEAVE_TWO


#define NESTED_ENUM_INTERNAL_GET_TYPE(...) NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_TYPE_STRING(...) NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__))

// in type we might find a comma so it must be __VA_ARGS__
#define NESTED_ENUM_INTERNAL_PASTE_VAL(value) value, , 
#define NESTED_ENUM_INTERNAL_PASTE_ID(id) , id, 
#define NESTED_ENUM_INTERNAL_PASTE_CODE(...) , , (NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_PASTE_VAL_ID(value, id) value, id,
#define NESTED_ENUM_INTERNAL_PASTE_VAL_CODE(value, ...) value, , (NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_PASTE_ID_CODE(id, ...) , id, (NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_PASTE_VAL_ID_CODE(value, id, ...) value, id, (NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))

// __VA_ARGS__ are the expanded typePack from an enum body definition
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE(functions, getter, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE1((NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE, NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY functions), functions, getter, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE1(functions, functions2, getter, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY(functions, __VA_ARGS__)(functions2, getter, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE(functions, getter, expectedParametersSuffix, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE1(functions, getter, NESTED_ENUM_INTERNAL_PASTE_##expectedParametersSuffix(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE1(functions, getter, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE2(functions, getter (__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE_CONTINUE2(functions, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL(NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY, functions __VA_OPT__(,) __VA_ARGS__)(__VA_ARGS__)

#define NESTED_ENUM_INTERNAL_GET_ID(...) NESTED_ENUM_INTERNAL_GET_ID1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_ID1(name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE((NESTED_ENUM_INTERNAL_GET_ID_FINAL2, NESTED_ENUM_INTERNAL_GET_ID_FINAL1), NESTED_ENUM_INTERNAL_GET_SECOND_OF_MANY __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_ID_FINAL1(...) NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>{}
#define NESTED_ENUM_INTERNAL_GET_ID_FINAL2(...) NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>{ __VA_ARGS__ }

#define NESTED_ENUM_INTERNAL_GET_VAL(...) NESTED_ENUM_INTERNAL_GET_VAL1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL1(name, ...) NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE((NESTED_ENUM_INTERNAL_GET_VAL_FINAL2, NESTED_ENUM_INTERNAL_GET_VAL_FINAL1), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_GET_VAL_FINAL1(...) ::nested_enum::detail::opt<underlying_type>{ false, underlying_type(0) }
#define NESTED_ENUM_INTERNAL_GET_VAL_FINAL2(...) ::nested_enum::detail::opt<underlying_type>{ true, underlying_type(__VA_ARGS__) }

#define NESTED_ENUM_INTERNAL_EXTRACT_CODE(...) NESTED_ENUM_INTERNAL_EXTRACT_CODE1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_EXTRACT_CODE1(name, ...)                                                               \
  NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE((NESTED_ENUM_INTERNAL_EXTRACT_CODE_FINAL2, NESTED_ENUM_INTERNAL_EXTRACT_CODE_FINAL1), NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_EXTRACT_CODE_FINAL1(...) 
#define NESTED_ENUM_INTERNAL_EXTRACT_CODE_FINAL2(...) __VA_ARGS__

#define NESTED_ENUM_INTERNAL_DEFINE_ENUM(...) NESTED_ENUM_INTERNAL_DEFINE_ENUM1(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM1(name, ...) name NESTED_ENUM_INTERNAL_GET_VAL_ID_CODE((NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL2, NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL1), NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL1(...) 
#define NESTED_ENUM_INTERNAL_DEFINE_ENUM_FINAL2(...) = __VA_ARGS__

#define NESTED_ENUM_INTERNAL_STRUCT_IDENTITY(typeName) struct typeName

// this is where the magic happens, so i'll write down how it works before i forget
// the way for_each works is through macro deferred expressions and continuous rescanning by EXPAND
// this however not only rescans the current iteration but also all previous ones and everything inside them
// in order to avoid rescanning and substituting the deferred NESTED_ENUM_INTERNAL_SETUP calls we need to defer the initial macro call
// as many times as there are expands - 1, hence the deferStages bit
// but because after every loop iteration one less expand is left we need to also decrease the number of deferrals
// during the process multiple deferrals are exhausted so we actually need to add one
// needs the main for_each macro to pass it the deferStages
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS(macro, deferStages, extraArgs, a1, ...)                                         \
    macro deferStages (extraArgs, a1, __VA_ARGS__)                                                                                            \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, NESTED_ENUM_INTERNAL_DEFER_ADD(deferStages), extraArgs, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS


// extraArgs are currently - (parentName), arg - (typeName, (embedded code), (specialisation specifier, ...))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP(extraArgs, arg, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP1(NESTED_ENUM_INTERNAL_DEPAREN(extraArgs), NESTED_ENUM_INTERNAL_DEPAREN(arg))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP1(parent, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, typeName, code, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, code, NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, code, treeCall, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, code, treeCall __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, code, treeCall, ...)  NESTED_ENUM_INTERNAL_TREE_##treeCall(parent, typeName, code, __VA_ARGS__)


#define NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                    \
  public:                                                                                                                                     \
    constexpr typeName() noexcept = delete;                                                                                                   \
    constexpr typeName(const typeName &other) noexcept = default;                                                                             \
    constexpr typeName(Value value) noexcept { internal_value = value; }                                                                      \
    constexpr typeName &operator=(const typeName &other) noexcept = default;                                                                  \
    constexpr typeName &operator=(Value value) noexcept { internal_value = value; return *this; }

#define NESTED_ENUM_INTERNAL_DEFINITION_NO_PARENT(isLeafValue, placeholder, typeName, underlying, code, continueEnum, ...)                    \
  struct typeName : public ::nested_enum::nested_enum<struct typeName>                                                                        \
  {                                                                                                                                           \
    template<typename U> friend struct ::nested_enum::nested_enum;                                                                            \
                                                                                                                                              \
    using underlying_type = underlying;                                                                                                       \
  private:                                                                                                                                    \
    static constexpr bool internalIsLeaf_ = isLeafValue;                                                                                      \
    static constexpr auto internalName_ = create_name(::nested_enum::fixed_string{ #typeName });   	                                          \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_BODY_##continueEnum(typeName, (NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__)),                                \
      NESTED_ENUM_INTERNAL_DEPAREN(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__)))                                                      \
  public:                                                                                                                                     \
    NESTED_ENUM_INTERNAL_DEPAREN(code)                                                                                                        \
  };

#define NESTED_ENUM_INTERNAL_DEFINITION_(isLeafValue, Parent, typeName, underlying, code, continueEnum, ...)                                  \
  struct typeName : public ::nested_enum::nested_enum<struct typeName>                                                                        \
  {                                                                                                                                           \
    template<typename U> friend struct ::nested_enum::nested_enum;                                                                            \
                                                                                                                                              \
    using parent = typename Parent::type;                                                                                                     \
    using underlying_type = underlying;                                                                                                       \
                                                                                                                                              \
    static constexpr parent value() { struct Parent result = Parent::typeName; return result; }                                               \
  private:                                                                                                                                    \
    static constexpr bool internalIsLeaf_ = isLeafValue;                                                                                      \
    static constexpr auto internalEnumIndex_ = ::nested_enum::detail::find_index(parent::internalEnumValues_, Parent::typeName).value();      \
    static constexpr auto internalName_ = create_name(::nested_enum::fixed_string{ #typeName });                                              \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_BODY_##continueEnum(typeName, (NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__)),                                \
      NESTED_ENUM_INTERNAL_DEPAREN(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__)))                                                      \
  public:                                                                                                                                     \
    NESTED_ENUM_INTERNAL_DEPAREN(code)                                                                                                        \
  };

#define NESTED_ENUM_INTERNAL_DEFINITION_FROM(isLeafValue, Parent, typeName, underlying, code, continueEnum, ...)                              \
  struct Parent::typeName : public ::nested_enum::nested_enum<struct Parent::typeName>                                                        \
  {                                                                                                                                           \
    template<typename U> friend struct ::nested_enum::nested_enum;                                                                            \
                                                                                                                                              \
    using parent = typename Parent::type;                                                                                                     \
    using underlying_type = underlying;                                                                                                       \
                                                                                                                                              \
    static constexpr parent value() { struct Parent result = Parent::typeName; return result; }                                               \
  private:                                                                                                                                    \
    static constexpr bool internalIsLeaf_ = isLeafValue;                                                                                      \
    static constexpr auto internalEnumIndex_ = ::nested_enum::detail::find_index(parent::internalEnumValues_, Parent::typeName).value();      \
    static constexpr auto internalName_ = create_name(::nested_enum::fixed_string{ #typeName });                                              \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_BODY_##continueEnum(typeName, (NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(__VA_ARGS__)),                                \
      NESTED_ENUM_INTERNAL_DEPAREN(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(__VA_ARGS__)))                                                      \
  public:                                                                                                                                     \
    NESTED_ENUM_INTERNAL_DEPAREN(code)                                                                                                        \
  };


#define NESTED_ENUM_INTERNAL_BODY_(typeName, ...)                                                                                             \
  public:                                                                                                                                     \
    enum Value : underlying_type { };                                                                                                         \
    Value internal_value;                                                                                                                     \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                        \
                                                                                                                                              \
    constexpr operator Value() const noexcept { return internal_value; }                                                                      \
  private:                                                                                                                                    \
    static constexpr NESTED_ENUM_ARRAY_TYPE<Value, 0> internalEnumValues_{};                                                                  \
                                                                                                                                              \
    static constexpr NESTED_ENUM_ARRAY_TYPE<NESTED_ENUM_OPTIONAL_TYPE<NESTED_ENUM_STRING_VIEW_TYPE>, 0> internalEnumIds_{};                   \
                                                                                                                                              \
    static constexpr ::nested_enum::detail::type_list<> internalSubtypes_{};

#define NESTED_ENUM_INTERNAL_BODY_CONTINUE(typeName, structsArguments, ...)                                                                   \
  public:                                                                                                                                     \
    enum Value : underlying_type { NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                                       \
      NESTED_ENUM_INTERNAL_DEFINE_ENUM, (,), __VA_ARGS__) };                                                                                  \
    Value internal_value;                                                                                                                     \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                                        \
                                                                                                                                              \
    constexpr operator Value() const noexcept { return internal_value; }                                                                      \
  private:                                                                                                                                    \
    static constexpr auto internalEnumValues_ = ::nested_enum::detail::get_array_of_values<Value, underlying_type,                            \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_GET_VAL, (,), __VA_ARGS__)>();                \
                                                                                                                                              \
    static constexpr auto internalEnumNames_ = ::nested_enum::detail::get_string_values<internalName_,                                        \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_GET_TYPE_STRING, (,), __VA_ARGS__)>();        \
                                                                                                                                              \
    static constexpr auto internalEnumIds_ = NESTED_ENUM_ARRAY_TYPE                                                                           \
      { NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE, NESTED_ENUM_INTERNAL_GET_ID, (,), __VA_ARGS__) };                \
  public:                                                                                                                                     \
    NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS, NESTED_ENUM_INTERNAL_STRUCT_PREP,                    \
      NESTED_ENUM_INTERNAL_DEFER_ADD(NESTED_ENUM_INTERNAL_DEFER()), (typeName),                                                               \
      NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_INTERLEAVE_TWO, structsArguments, __VA_ARGS__))                                      \
  private:                                                                                                                                    \
    static constexpr ::nested_enum::detail::type_list<NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                    \
      NESTED_ENUM_INTERNAL_STRUCT_IDENTITY, (,), NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                         \
      NESTED_ENUM_INTERNAL_GET_TYPE, (,), __VA_ARGS__))> internalSubtypes_{};


#define NESTED_ENUM_INTERNAL_SETUP(definitionType, definitionArguments) NESTED_ENUM_INTERNAL_DEFINITION_##definitionType definitionArguments
#define NESTED_ENUM_INTERNAL_SETUP_AGAIN() NESTED_ENUM_INTERNAL_SETUP

#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE(version, ...) NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE1(version, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE1(version, name, ...) NESTED_ENUM_INTERNAL_CHOOSE_CALL_BINARY((NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_HAS, NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_) __VA_OPT__(,) __VA_ARGS__)(version, name __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_(version, name, ...) name, NESTED_ENUM_DEFAULT_UNDERLYING_TYPE NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_INCLUDE_##version()
#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_HAS(version, name, underlying, ...) name, underlying NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_INCLUDE_##version(NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_INCLUDE_(...)
#define NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE_INCLUDE_DO(...) , (__VA_ARGS__)

// defines an outer node when no explicit definition is provided
#define NESTED_ENUM_INTERNAL_TREE_(parent, typeName, code, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (true, parent, typeName, NESTED_ENUM_DEFAULT_UNDERLYING_TYPE, code, ))
// defines an inner node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_ENUM(parent, typeName, code, enumDefinition, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (false, parent, NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE(, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), code, __VA_OPT__(CONTINUE,) __VA_ARGS__))
// defines an outer node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_LEAF(parent, typeName, code, enumDefinition, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (true, parent, NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE(, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), code, __VA_OPT__(CONTINUE,) __VA_ARGS__))
// only forward declares a node because the user wants to define it later 
#define NESTED_ENUM_INTERNAL_TREE_DEFER(parent, typeName, code, ...) struct typeName;

//==================================================================================

// defines a (root) enum that can be expanded into a tree
// the macro can be divided into 3 parts: definition, entries, specialisations
// 
//                     definition                                                                           entries                                                                 specialisations
//                         |                                                                                   |                                                                           |
//                         v                                                                                   v                                                                           v
// (name, [underlying type], [global string prefix], [linked type]), ((enum value name, [extra input specifier], [extra inputs]), enum values...), (specialisation specifier, child definition, child entries, child specialisations), ...
//                                      ^                                                                                                          \____________________________________  __________________________________________/  \ /
//                                      |                                                                                                                                               \/                                              V    
//                        (present only in topmost enum)                                                                                                                            (1st child)                              (consecutive children)
// 
// If a parenthesised section has only 1 argument, the parentheses can be omitted
// 
// Example:
// 
//    NESTED_ENUM((Vehicle, std::uint32_t, "Category"), (Land, Watercraft, Amphibious, Aircraft),
//      (ENUM, (Land, std::uint64_t), (Motorcycle, Car, Bus, Truck, Tram, Train), 
//        (DEFER),
//        (ENUM, Car, ((Minicompact, ID, "A-segment"), (Subcompact, ID, "B-segment"), (Compact, ID,
//          "C-segment"), (MidSize, ID, "D-segment"), (FullSize, ID, "E-segment"), (Luxury, ID, "F-segment")),
//          (ENUM, Minicompact, ((Fiat_500, TYPE, Fiat_type), (Hyundai_i10, TYPE, Hyundai_type), (Toyota_Aygo, TYPE, Toyota_type), ...)),
//          (ENUM, Subcompact, (Chevrolet_Aveo, Hyundai_Accent, Volkswagen_Polo, ...))
//        ),
//        (ENUM, Bus, (Shuttle, Trolley, School, Coach, Articulated)),
//        ...
//      )  
//    )
//    
//    NESTED_ENUM_FROM(Vehicle::Land, Motorcycle, (Scooter, Cruiser, Sport, OffRoad))
//
#define NESTED_ENUM(enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(NO_PARENT, (false, (DO_NOT_USE), NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE(DO, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_OPT__(CONTINUE,) __VA_ARGS__)))

// defines a deferred enum from a tree definition; needs to fill out the parent explicitly
// see NESTED_ENUM macro for more information
#define NESTED_ENUM_FROM(parent, enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(FROM, (false, parent, NESTED_ENUM_INTERNAL_CHECK_UNDERLYING_TYPE(DO, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_OPT__(CONTINUE,) __VA_ARGS__)))


#endif