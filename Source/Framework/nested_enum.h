/*
  ==============================================================================

    nested_enum.h
    Created: 19 Sep 2023 4:08:26am
    Author:  theuser27

  ==============================================================================
*/

#ifndef NESTED_ENUM_HPP
#define NESTED_ENUM_HPP

#define NESTED_ENUM_VERSION_MAJOR 0
#define NESTED_ENUM_VERSION_MINOR 2
#define NESTED_ENUM_VERSION_PATCH 1

#include <cstdint>
#include <array>
#include <string_view>
#include <type_traits>
#include <optional>
#include <functional>
#include <tuple>
#include <limits>

namespace nested_enum
{
  template <size_t N>
  struct fixed_string
  {
    using char_type = std::string_view::value_type;

    constexpr fixed_string() = default;
    constexpr fixed_string(const char_type(&array)[N + 1]) noexcept
    {
      for (size_t i = 0; i < N; ++i)
        data[i] = array[i];

      data[N] = '\0';
    }

    constexpr explicit fixed_string(std::string_view string) noexcept :
      fixed_string{ string, std::make_index_sequence<N>{} }
    {
    }

    template<size_t M>
    constexpr auto concat(fixed_string<M> other) const noexcept
    {
      fixed_string<N + M> result;
      result.data[N + M] = '\0';

      for (size_t i = 0; i < N; ++i)
        result.data[i] = data[i];

      for (size_t i = 0; i < M; ++i)
        result.data[N + i] = other.data[i];

      return result;
    }

    [[nodiscard]] constexpr auto operator<=>(const fixed_string &) const = default;
    [[nodiscard]] constexpr operator std::string_view() const noexcept { return { data.data(), N }; }
    [[nodiscard]] constexpr auto size() const noexcept -> std::size_t { return N; }

    std::array<char_type, N + 1> data;

  private:
    template <size_t... Indices>
    constexpr fixed_string(std::string_view str, std::index_sequence<Indices...>) noexcept :
      data{ str[Indices]..., static_cast<char_type>('\0') }
    {
    }
  };

  template <size_t N>
  fixed_string(const char(&)[N])->fixed_string<N - 1>;

  namespace detail
  {
    template<typename Derived, typename Base>
    concept DerivedOrIs = std::is_nothrow_convertible_v<Base, Derived> || std::is_same_v<Base, Derived>;

    template<typename T>
    concept Enum = std::is_enum_v<T>;

    template<typename T>
    concept Integral = std::is_integral_v<T>;

    // checks if a type is complete
    template <class T, class = void>
    struct is_complete_type : std::false_type { };

    template <class T>
    struct is_complete_type<T, decltype(void(sizeof(T)))> : std::true_type { };

    template <class T>
    inline constexpr auto is_complete_type_v = is_complete_type<T>::value;

    // converts an array of integers into an integer_sequence
    template<auto A, class = std::make_index_sequence<std::tuple_size_v<std::decay_t<decltype(A)>>>>
    struct as_index_sequence;

    template<auto A, std::size_t ...I>
    struct as_index_sequence<A, std::index_sequence<I...>>
    {
      using type = std::integer_sequence<typename std::decay_t<decltype(A)>::value_type, A[I]...>;
    };

    template<typename T>
    using pure_type_t = std::remove_pointer_t<std::remove_cvref_t<T>>;

    template<typename TupleLike>
    constexpr size_t get_tuple_size()
    {
      return std::tuple_size_v<pure_type_t<TupleLike>>;
    }

    inline constexpr fixed_string scopeResolution = "::";

    constexpr auto count_if(const auto &container, auto predicate)
    {
      size_t count = 0;
      auto first = container.begin();
      auto last = container.end();
      for (; first != last; ++first)
        if (predicate(*first))
          ++count;
      return count;
    }

    template<typename T>
    constexpr auto find_index(const auto &container, const T &value)
    {
      auto first = container.begin();
      auto last = container.end();

      for (; first != last; ++first)
        if (*first == value)
          return std::optional{ (size_t)(first - container.begin()) };

      return std::optional<size_t>{};
    }

    template <bool attemptToReturnArray = true, class Callable, class Tuple>
    constexpr decltype(auto) apply_one(Callable &&callable, Tuple &&tuple)
    {
      return[]<size_t ... Indices>(Callable && callable, Tuple && tuple, std::index_sequence<Indices...>) -> decltype(auto)
      {
        if constexpr (std::is_same_v<std::invoke_result_t<Callable, std::tuple_element_t<0, std::remove_cvref_t<Tuple>>>, void>)
          (std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple))), ...);
        else if constexpr (attemptToReturnArray)
          return std::array{ std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple)))... };
        else
          return std::make_tuple(std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<Tuple>(tuple)))...);
      }(std::forward<Callable>(callable), std::forward<Tuple>(tuple),
        std::make_index_sequence<get_tuple_size<decltype(tuple)>()>{});
    }

    // callable must be a functor
    template <auto TupleLike, bool attemptToReturnArray = true, class Callable>
    constexpr decltype(auto) apply_one(Callable &&callable)
    {
      return[]<size_t ... Indices>(Callable && callable, std::index_sequence<Indices...>) -> decltype(auto)
      {
        if constexpr (attemptToReturnArray)
          return std::array{ callable.template operator() < std::get<Indices>(TupleLike) > ()... };
        else
          return std::make_tuple(callable.template operator() < std::get<Indices>(TupleLike) > ()...);
      }(std::forward<Callable>(callable), std::make_index_sequence<get_tuple_size<decltype(TupleLike)>()>{});
    }

    template <auto Predicate, auto &TupleLike>
    constexpr auto filter()
    {
      if constexpr (get_tuple_size<decltype(TupleLike)>() == 0)
        return std::tuple<>{};
      else
      {
        constexpr auto areIncluded = apply_one(Predicate, TupleLike);
        constexpr auto includedIndices = [&]()
        {
          constexpr size_t size = count_if(areIncluded,
            [](bool b) { return b == true; });

          std::array<size_t, size> values{};

          for (size_t i = 0, j = 0; i < areIncluded.size(); ++i)
            if (areIncluded[i])
              values[j++] = i;

          return values;
        }();

        return[]<size_t ... Indices>(std::index_sequence<Indices...>) -> decltype(auto)
        {
          return std::make_tuple(std::get<Indices>(TupleLike)...);
        }(typename as_index_sequence<includedIndices>::type{});
      }
    }



    constexpr int8_t get_digit(char character)
    {
      if (character >= '0' && character <= '9')
        return (int8_t)(character - '0');

      if (character >= 'A' && character <= 'Z')
        return (int8_t)(character - 'A' + 10);

      if (character >= 'a' && character <= 'z')
        return (int8_t)(character - 'a' + 10);

      return 0;
    }

    constexpr std::string_view trim_white_space(std::string_view view)
    {
      auto begin = view.find_first_not_of(' ');
      if (begin == std::string::npos)
        return {};

      auto end = view.find_last_not_of(' ');
      auto range = end - begin + 1;

      return view.substr(begin, range);
    }

    template<Integral Int = int64_t>
    constexpr Int get_int_from_string(std::string_view view)
    {
      auto trimmedView = trim_white_space(view);

      bool isNegative = false;
      if (trimmedView.starts_with('-'))
      {
        trimmedView.remove_prefix(1);
        isNegative = true;
      }

      Int number = 0, decimal = 1;
      for (size_t i = trimmedView.size(); i-- > 0; )
      {
        if (trimmedView[i] == '\'')
          continue;
        number += static_cast<Int>(get_digit(trimmedView[i])) * decimal;
        decimal *= 10;
      }

      if (isNegative)
      {
        if constexpr (std::is_unsigned_v<Int>)
          return std::numeric_limits<Int>::max() - number;
        else
          return -number;
      }

      return number;
    }

    template<typename E, fixed_string staticString, bool hasIds = false>
    consteval auto get_array_of_values()
    {
      using underlying_type = std::underlying_type_t<E>;

      constexpr size_t size = []() -> size_t
      {
        auto view = trim_white_space({ staticString });
        if (view.empty())
          return 0;
        size_t size = count_if(view, [](char c) { return c == ','; }) + 1;
        return (!hasIds) ? size : size / 2;
      }();

      auto view = trim_white_space({ staticString });
      std::array<E, size> enumValues{};

      if constexpr (size != 0)
      {
        underlying_type currentValue = 0;
        size_t index = 0;
        [[maybe_unused]] size_t outerIndex = 0;

        while (!view.empty())
        {
          auto separatorIndex = view.find(',');
          auto substr = view.substr(0, separatorIndex);
          if constexpr (!hasIds)
          {
            if (auto equalsIndex = substr.find('='); equalsIndex != substr.npos)
              currentValue = get_int_from_string<underlying_type>(substr.substr(equalsIndex));

            enumValues[index++] = (E)currentValue++;
          }
          else
          {
            if (outerIndex % 2 == 0)
            {
              if (auto equalsIndex = substr.find('='); equalsIndex != substr.npos)
                currentValue = get_int_from_string<underlying_type>(substr.substr(equalsIndex));

              enumValues[index++] = (E)currentValue++;
            }
          }

          outerIndex++;
          if (separatorIndex == view.npos)
            break;
          view.remove_prefix(separatorIndex + 1);
        }
      }

      return enumValues;
    }

    template<fixed_string values, fixed_string type, bool hasIds = false, size_t index = 0>
    consteval auto get_string_values()
    {
      constexpr auto trimmedView = []()
      {
        // trimming is implemented with pure indices because 
        // msvc freaks out and gives nonsensical results otherwise
        size_t begin = index;
        size_t end = index;
        for (; end < values.size(); ++end)
        {
          if (values.data[end] == ',')
            break;
        }
        for (; begin < values.size(); ++begin)
        {
          if (values.data[begin] != ' ')
            break;
        }
        --end;
        for (; end >= index; --end)
        {
          if (values.data[end] != ' ')
            break;
        }
        return std::pair{ begin, end - begin + 1 };
      }();

      constexpr auto current = [&]()
      {
        if constexpr (type.size() == 0)
        {
          fixed_string<trimmedView.second + 1> result{};

          for (size_t i = 0; i < trimmedView.second; ++i)
            result.data[i] = values.data[trimmedView.first + i];

          result.data[trimmedView.second] = '\0';
          return result;
        }
        else
        {
          fixed_string<type.size() + scopeResolution.size() + trimmedView.second + 1> result{};

          for (size_t i = 0; i < type.size(); ++i)
            result.data[i] = type.data[i];

          for (size_t i = 0; i < scopeResolution.size(); ++i)
            result.data[type.size() + i] = scopeResolution.data[i];

          for (size_t i = 0; i < trimmedView.second; ++i)
            result.data[type.size() + scopeResolution.size() + i] = values.data[trimmedView.first + i];

          result.data[type.size() + scopeResolution.size() + trimmedView.second] = '\0';
          return result;
        }
      }();

      constexpr size_t nextIndex = []()
      {
        size_t commaCount = (!hasIds) ? 1 : 0;
        for (size_t i = index; i < values.size(); ++i)
        {
          if (values.data[i] == ',')
          {
            ++commaCount;
            if (commaCount == 2)
              return ++i;
          }
        }
        return values.size();
      }();

      if constexpr (nextIndex == values.size())
        return current;
      else
        return current.concat(get_string_values<values, type, hasIds, nextIndex>());
    }

    // bless their soul https://stackoverflow.com/a/54932626
    namespace tupleMagic
    {
      template <typename T> struct is_tuple : std::false_type { };
      template <typename... Ts> struct is_tuple<std::tuple<Ts...>> : std::true_type { };

      template<typename T>
      constexpr decltype(auto) as_tuple(T t)
      {
        return std::make_tuple(t);
      }

      template<typename ...Ts>
      constexpr decltype(auto) as_tuple(std::tuple<Ts...> t)
      {
        return t;
      }

      constexpr decltype(auto) flatten(auto t)
      {
        return t;
      }

      template<typename T>
      constexpr decltype(auto) flatten(std::tuple<T> t)
      {
        return flatten(std::get<0>(t));
      }

      template<typename ...Ts, std::enable_if_t<!(is_tuple<Ts>::value || ...), bool> = false>
      constexpr decltype(auto) flatten(std::tuple<Ts...> t)
      {
        return t;
      }

      template<typename ...Ts, std::enable_if_t<(is_tuple<Ts>::value || ...), bool> = false>
      constexpr decltype(auto) flatten(std::tuple<Ts...> t)
      {
        return std::apply([](auto...ts) { return flatten(std::tuple_cat(as_tuple(flatten(ts))...)); }, t);
      }
    }

    template<class Tuple>
    constexpr auto tuple_of_arrays_to_array(Tuple &&tuple)
    {
      constexpr auto function = []<auto Self, typename T, size_t N, typename ... Args>(std::array<T, N> first, Args ... args)
      {
        if constexpr (sizeof...(Args) == 0)
          return first;
        else
        {
          auto next = Self.template operator() < Self > (args...);
          std::array<T, N + next.size()> newArray;

          for (size_t i = 0; i < first.size(); i++)
            newArray[i] = first[i];

          for (size_t i = 0; i < next.size(); i++)
            newArray[first.size() + i] = next[i];

          return newArray;
        }
      };

      return[&]<size_t ... Indices>(Tuple && tupleToExpand, std::index_sequence<Indices...>)
      {
        return function.template operator() < function > (std::get<Indices>(tupleToExpand)...);
      }(std::forward<Tuple>(tuple), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    }

    template<typename T, typename E>
    concept this_enum = std::is_same_v<T, typename E::Value>;
  }

  // InnerNodes - enum values that are themselves enums
  // OuterNodes - enum values that are NOT enums
  // AllNodes - all enum values 
  enum InnerOuterAll { InnerNodes, OuterNodes, AllNodes };

  template<class E, class P = E, typename Integral = int32_t, fixed_string GlobalId = "">
  struct nested_enum
  {
    using underlying_type = Integral;
    using type = E;
    using parent = P;

    template<class, class, typename, fixed_string>
    friend struct nested_enum;

    template<typename T> requires std::is_integral_v<T> || std::is_floating_point_v<T>
    static constexpr auto make_enum(T t)
    {
      auto i = enum_value(static_cast<underlying_type>(t));
      if (i.has_value())
        return std::optional{ E{ i.value() } };
      else
        return std::optional<E>{};
    }

    // the following member functions assume a valid enum value had been provided to create the object

    // returns the string of the currently held value
    constexpr auto enum_string(bool clean = false) const noexcept
    {
      return enum_string(static_cast<const E &>(*this).value, clean).value();
    }
    // returns the id of the currently held value
    constexpr auto enum_id() const noexcept
    {
      return enum_id(static_cast<const E &>(*this).value).value();
    }
    // returns the string and id of the currently held value
    constexpr auto enum_string_and_id(bool clean = false) const noexcept
    {
      return enum_string_and_id(static_cast<const E &>(*this).value, clean).value();
    }
    // returns the currently held value
    constexpr auto enum_value() const noexcept
    {
      return static_cast<const E &>(*this).value;
    }
    // returns the integer representation of the currently held value
    constexpr auto enum_integer() const noexcept
    {
      return (underlying_type)static_cast<const E &>(*this).value;
    }

    // returns the global id that was provided
    static constexpr auto enum_global_id() -> std::string_view
    {
      if constexpr (std::is_same_v<E, P>)
        return std::string_view{ GlobalId };
      else
        return parent::enum_global_id();
    }

  protected:
    template<size_t N>
    static constexpr auto create_type_name(fixed_string<N> name)
    {
      if constexpr (std::is_same_v<E, P>)
      {
        if constexpr (GlobalId.size() == 0)
          return name;
        else
        {
          auto fullName = GlobalId.concat(detail::scopeResolution.concat(name));
          return fullName;
        }
      }
      else
      {
        auto fullName = P::name.concat(detail::scopeResolution.concat(name));
        return fullName;
      }
    }
  public:
    // returns the reflected type name of the enum 
    static constexpr auto enum_type_name(bool clean = false) noexcept -> std::string_view
    {
      std::string_view name = E::name;
      if (clean)
      {
        std::string_view scope = "::";
        name.remove_prefix(name.rfind(scope) + scope.length());
      }
      return name;
    }
    // returns the values inside this enum that satisfy the Selection
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_values() noexcept
    {
      if constexpr (E::enumValues.size() == 0 || Selection == AllNodes)
        return E::enumValues;
      else
      {
        constexpr auto valuesNeeded = detail::apply_one([]<typename T>(T &&) -> bool
        {
          using subType = typename detail::pure_type_t<T>::type;

          if constexpr (!detail::is_complete_type_v<subType>)
            return Selection == OuterNodes;
          else
          {
            if constexpr (subType::isOuterNode)
              return Selection == OuterNodes;
            else
              return Selection == InnerNodes;
          }
        },
          E::subtypes);

        constexpr auto result = [&]()
        {
          constexpr size_t size = detail::count_if(valuesNeeded,
            [](bool b) { return b == true; });

          std::array<typename E::Value, size> values;
          for (size_t i = 0, j = 0; i < valuesNeeded.size(); i++)
            if (valuesNeeded[i])
              values[j++] = E::enumValues[i];
          return values;
        }();

        return result;
      }
    }
    // returns the values count inside this enum that satisfy the selection
    static constexpr size_t enum_count(InnerOuterAll selection = AllNodes) noexcept
    {
      if constexpr (detail::get_tuple_size<decltype(E::subtypes)>() == 0)
        return E::enumValues.size();
      else
      {
        if (selection == AllNodes)
          return E::enumValues.size();

        auto counts = detail::apply_one([&]<typename T>(T &&) -> size_t
        {
          using subType = typename detail::pure_type_t<T>::type;

          if ((selection == AllNodes) ||
            (detail::is_complete_type_v<subType> && !subType::isOuterNode && selection == InnerNodes) ||
            (detail::is_complete_type_v<subType> && subType::isOuterNode && selection == OuterNodes) ||
            (!detail::is_complete_type_v<subType> && selection == OuterNodes))
            return 1;
          else
            return 0;
        },
          E::subtypes);

        size_t total = 0;
        for (auto count : counts)
          total += count;

        return total;
      }
    }
    // returns the underlying integers of enum values that satisfy the selection
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_integers() noexcept
    {
      constexpr auto enumValues = enum_values<Selection>();
      constexpr auto integerArray = [&]()
      {
        std::array<underlying_type, enumValues.size()> result;
        for (size_t i = 0; i < result.size(); i++)
          result[i] = (underlying_type)enumValues[i];

        return result;
      }();
      return integerArray;
    }
    // returns the ids of enum values that satisfy the selection
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_ids() noexcept
    {
      if constexpr (E::ids.size() == 0 || Selection == AllNodes)
        return E::ids;
      else
      {
        constexpr auto idsNeeded = detail::apply_one([]<typename T>(T &&) -> bool
        {
          using subType = typename detail::pure_type_t<T>::type;

          if constexpr (!detail::is_complete_type_v<subType>)
            return Selection == OuterNodes;
          else
          {
            if constexpr (subType::isOuterNode)
              return Selection == OuterNodes;
            else
              return Selection == InnerNodes;
          }
        },
          E::subtypes);

        constexpr auto result = [&]()
        {
          constexpr size_t size = detail::count_if(idsNeeded,
            [](bool b) { return b == true; });

          std::array<std::string_view, size> values;
          for (size_t i = 0, j = 0; i < idsNeeded.size(); i++)
            if (idsNeeded[i])
              values[j++] = E::ids[i];
          return values;
        }();

        return result;
      }
    }
    // returns the reflected strings of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_strings(bool clean = false) noexcept
    {
      if constexpr (enum_count(Selection) == 0)
      {
        // if this isn't assigned to a constexpr variable msvc won't recognise the function as constexpr
        constexpr auto enumStrings = std::array<std::string_view, 0>{};
        return enumStrings;
      }
      else
      {
        constexpr auto generateStrings = []<bool cleanString>()
        {
          constexpr auto valuesNeeded = detail::apply_one([]<typename T>(T &&) -> bool
          {
            if constexpr (Selection == AllNodes)
              return true;
            else
            {
              using subType = typename detail::pure_type_t<T>::type;

              if constexpr (!detail::is_complete_type_v<subType>)
                return Selection == OuterNodes;
              else
              {
                if constexpr (subType::isOuterNode)
                  return Selection == OuterNodes;
                else
                  return Selection == InnerNodes;
              }
            }
          },
            E::subtypes);

          constexpr size_t size = detail::count_if(valuesNeeded,
            [](bool b) { return b == true; });

          std::array<std::string_view, size> result;
          for (size_t i = 0, j = 0; i < valuesNeeded.size(); i++)
          {
            if (valuesNeeded[i])
            {
              std::string_view view = E::enumStrings;

              size_t end = 0;
              size_t count = 0;
              while (count < i)
                if (view[end++] == '\0')
                  ++count;

              view = std::string_view(&E::enumStrings.data[end]);

              if constexpr (cleanString)
              {
                std::string_view scope = "::";
                view.remove_prefix(view.rfind(scope) + scope.length());
              }
              result[j++] = view;
            }
          }
          return result;
        };

        constexpr auto enumStrings = generateStrings.template operator() < false > ();
        constexpr auto enumStringsClean = generateStrings.template operator() < true > ();
        if (!clean)
          return enumStrings;
        else
          return enumStringsClean;
      }
    }
    // returns the reflected strings and ids of enum values that satisfy the selection
    // "clean" refers to whether only the enum value name or its entire path is returned
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_strings_and_ids(bool clean = false) noexcept
    {
      constexpr auto enumIds = enum_ids<Selection>();
      if constexpr (enumIds.size() == 0)
        return std::array<std::pair<std::string_view, std::string_view>, 0>{};
      else
      {
        constexpr auto stringsAndIds = [&]()
        {
          std::array<std::pair<std::string_view, std::string_view>, enumIds.size()> result{};
          for (size_t i = 0; i < enumIds.size(); ++i)
            result[i] = { enum_strings<Selection>(false)[i], enumIds[i] };
          return result;
        }();

        constexpr auto stringsAndIdsClean = [&]()
        {
          std::array<std::pair<std::string_view, std::string_view>, enumIds.size()> result{};
          for (size_t i = 0; i < enumIds.size(); ++i)
            result[i] = { enum_strings<Selection>(true)[i], enumIds[i] };
          return result;
        }();

        if (!clean)
          return stringsAndIds;
        else
          return stringsAndIdsClean;
      }
    }
    // returns a tuple of the enum subtypes that satisfy the selection
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_subtypes() noexcept
    {
      if constexpr (detail::get_tuple_size<decltype(E::subtypes)>() == 0)
        return std::tuple<>{};
      else
      {
        return detail::filter < []<typename T>(T &&) -> bool
        {
          if constexpr (Selection == AllNodes)
            return true;
          else
          {
            using subType = typename detail::pure_type_t<T>::type;

            if constexpr (!detail::is_complete_type_v<subType>)
              return Selection == OuterNodes;
            else
            {
              if constexpr (subType::isOuterNode)
                return Selection == OuterNodes;
              else
                return Selection == InnerNodes;
            }
          }
        },
          E::subtypes > ();
      }
    }

  private:
    static constexpr size_t enum_count_recursive_internal(InnerOuterAll selection = AllNodes) noexcept
    {
      size_t count = (selection == InnerNodes) || (selection == AllNodes);
      if constexpr (detail::get_tuple_size<decltype(E::subtypes)>() == 0)
        return count;
      else
      {
        auto subtypesSizes = detail::apply_one([&]<typename T>(T &&) -> size_t
        {
          using subType = typename detail::pure_type_t<T>::type;
          if constexpr (detail::is_complete_type_v<subType> && !subType::isOuterNode)
            return subType::enum_count_recursive_internal(selection);
          else
            return (selection == OuterNodes) || (selection == AllNodes);
        },
          E::subtypes);

        for (auto subtypeSize : subtypesSizes)
          count += subtypeSize;

        return count;
      }
    }
  public:
    // returns are recursive count of all enum values in the subtree that satisfy the selection
    static constexpr size_t enum_count_recursive(InnerOuterAll selection = AllNodes) noexcept
    {
      size_t count = enum_count_recursive_internal(selection);
      if ((selection == InnerNodes) || (selection == AllNodes))
        return --count;
      return count;
    }
  private:
    template<auto Predicate, InnerOuterAll Selection = AllNodes>
    static constexpr auto return_recursive_internal() noexcept
    {
      if constexpr (detail::get_tuple_size<decltype(E::subtypes)>() == 0)
        return std::tuple<>{};
      else
      {
        // checking which of the contained "enums" we need (outer or inner nodes)
        constexpr auto subtypesNeeded = detail::apply_one([]<typename T>(T &&) -> bool
        {
          if constexpr (Selection == AllNodes)
            return true;
          else
          {
            using subType = typename detail::pure_type_t<T>::type;

            if constexpr (!detail::is_complete_type_v<subType>)
              return Selection == OuterNodes;
            else
            {
              if constexpr (subType::isOuterNode)
                return Selection == OuterNodes;
              else
                return Selection == InnerNodes;
            }
          }
        },
          E::subtypes);

        // getting an array of values we need
        constexpr auto values = [&]()
        {
          constexpr size_t size = detail::count_if(subtypesNeeded,
            [](bool b) { return b == true; });

          return Predicate.template operator() < E, size > (subtypesNeeded);
        }();

        // new tuple of only the inner enum nodes
        constexpr auto subtypesToQuery = detail::filter < []<typename T>(T &&) -> bool
        {
          using subType = typename detail::pure_type_t<T>::type;

          // does the enum exist
          if constexpr (!detail::is_complete_type_v<subType>)
            return false;
          else
          {
            // does the enum have any values
            if constexpr (detail::get_tuple_size<decltype(subType::subtypes)>() == 0)
              return false;
            // different branches based on what we want
            else if constexpr ((Selection == AllNodes) || (Selection == OuterNodes))
              return true;
            else
            {
              constexpr auto subSubtypesNeeded = detail::filter < []<typename U>(U &&) -> bool
              {
                using subSubType = typename detail::pure_type_t<U>::type;
                return detail::is_complete_type_v<subSubType> && !subSubType::isOuterNode;
              },
                subType::subtypes > ();

              return detail::get_tuple_size<decltype(subSubtypesNeeded)>() != 0;
            }
          }
        },
          E::subtypes > ();

        // returning only the values if we don't have any subtypes to query
        if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
          return std::make_tuple(values);
        else
        {
          constexpr auto subtypesTuples = detail::apply_one<false>([]<typename T>(T &&)
          {
            using subType = typename detail::pure_type_t<T>::type;
            return subType::template return_recursive_internal<Predicate, Selection>();
          },
            subtypesToQuery);

          if constexpr (values.size() == 0)
            return subtypesTuples;
          else
            return std::tuple_cat(std::make_tuple(values), subtypesTuples);
        }
      }
    }
  public:
    // returns a tuple of arrays of all enum values in the subtree that satisfy the selection
    template<InnerOuterAll Selection = AllNodes>
    static constexpr auto enum_values_recursive() noexcept
    {
      constexpr auto predicate = []<typename T, size_t size>(auto array)
      {
        std::array<typename T::Value, size> values{};

        for (size_t i = 0, j = 0; i < array.size(); ++i)
          if (array[i])
            values[j++] = T::template enum_values<AllNodes>()[i];

        return values;
      };

      return detail::tupleMagic::flatten(return_recursive_internal<predicate, Selection>());
    }
    // returns a tuple of arrays of string_view of the reflected names of all enum values in the subtree that satisfy the selection
    // if flattenTuple == true, the tuple of arrays will be flattened to a single array with all strings inside
    template<InnerOuterAll Selection = AllNodes, bool flattenTuple = true, bool clean = false>
    static constexpr auto enum_strings_recursive() noexcept
    {
      constexpr auto predicate = []<typename T, size_t size>(auto array)
      {
        std::array<std::string_view, size> values{};

        for (size_t i = 0, j = 0; i < array.size(); ++i)
          if (array[i])
            values[j++] = T::template enum_strings<AllNodes>(clean)[i];

        return values;
      };

      if constexpr (flattenTuple)
        return detail::tuple_of_arrays_to_array(detail::tupleMagic::flatten(return_recursive_internal<predicate, Selection>()));
      else
        return detail::tupleMagic::flatten(return_recursive_internal<predicate, Selection>());
    }

    // returns the reflected string of an enum value of this type
    template<typename T> requires detail::this_enum<T, E>
    static constexpr auto enum_string(T value, bool clean = false) -> std::optional<std::string_view>
    {
      constexpr auto values = enum_values<AllNodes>();
      auto index = detail::find_index(values, value);
      if (!index.has_value())
        return {};

      return enum_strings<AllNodes>(clean)[index.value()];
    }
    // returns the reflected string of an enum value of this type, specified by its id
    static constexpr auto enum_string_by_id(std::string_view id, bool clean = false) -> std::optional<std::string_view>
    {
      constexpr auto enumIds = enum_ids<AllNodes>();
      auto index = detail::find_index(enumIds, id);
      if (!index.has_value())
        return {};

      return enum_strings(clean)[index.value()];
    }
    // returns the id of an enum value of this type
    template<typename T> requires detail::this_enum<T, E>
    static constexpr auto enum_id(T value) -> std::optional<std::string_view>
    {
      constexpr auto values = enum_values<AllNodes>();
      auto index = detail::find_index(values, value);
      if (!index.has_value())
        return {};

      return enum_ids<AllNodes>()[index.value()];
    }
    // returns the id of an enum value of this type, specified by its reflected string
    static constexpr auto enum_id(std::string_view enumString) -> std::optional<std::string_view>
    {
      constexpr auto strings = enum_strings<AllNodes>();
      auto index = detail::find_index(strings, enumString);
      if (!index.has_value())
        return {};

      return enum_ids<AllNodes>()[index.value()];
    }
    // returns the reflected string and id of an enum value of this type
    template<typename T> requires detail::this_enum<T, E>
    static constexpr auto enum_string_and_id(T value, bool clean = false) -> std::optional<std::pair<std::string_view, std::string_view>>
    {
      constexpr auto values = enum_values<AllNodes>();
      auto index = detail::find_index(values, value);
      if (!index.has_value())
        return {};

      auto stringsAndIds = enum_strings_and_ids<AllNodes>(clean);
      if (index.value() >= stringsAndIds.size())
        return {};

      return stringsAndIds[index.value()];
    }
    // returns the underlying integer of an enum value of this type
    // this one seems kind of pointless but i added it for completeness' sake
    template<typename T> requires detail::this_enum<T, E>
    static constexpr auto enum_integer(T value) -> std::optional<underlying_type>
    {
      static_assert(std::is_same_v<decltype(value), typename E::Value>, "Provided value doesn't match enum type");
      return (underlying_type)value;
    }
    // returns the underlying integer of an enum value of this type, specified by its reflected string
    static constexpr auto enum_integer(std::string_view enumString) -> std::optional<underlying_type>
    {
      constexpr auto enumStrings = enum_strings<AllNodes>(false);
      auto index = detail::find_index(enumStrings, enumString);
      if (!index.has_value())
        return {};

      return (underlying_type)enum_values<AllNodes>()[index.value()];
    }
    // returns the underlying integer of an enum value of this type, specified by its id
    static constexpr auto enum_integer_by_id(std::string_view id) -> std::optional<underlying_type>
    {
      constexpr auto enumIds = enum_ids();
      auto index = detail::find_index(enumIds, id);
      if (!index.has_value())
        return {};

      return (underlying_type)enum_values<AllNodes>()[index.value()];
    }
    // returns the enum value of this type, specified by an integer
    template<typename T> requires std::is_same_v<T, underlying_type>
    static constexpr auto enum_value(T integer)
    {
      constexpr auto enumValues = enum_values<AllNodes>();
      for (auto value : enumValues)
        if ((underlying_type)value == integer)
          return std::optional<typename E::Value>{ value };

      return std::optional<typename E::Value>{};
    }
    // returns the enum value of this type, specified by its reflected string
    static constexpr auto enum_value(std::string_view enumString)
    {
      constexpr auto enumStrings = enum_strings<AllNodes>(false);
      auto index = detail::find_index(enumStrings, enumString);
      if (!index.has_value())
        return std::optional<typename E::Value>{};

      return std::optional{ enum_values<AllNodes>()[index.value()] };
    }
    // returns the enum value of this type, specified by its id
    static constexpr auto enum_value_by_id(std::string_view id)
    {
      constexpr auto enumIds = enum_ids();
      auto index = detail::find_index(enumIds, id);
      if (!index.has_value())
        return std::optional<typename E::Value>{};

      return std::optional{ enum_values<AllNodes>()[index.value()] };
    }
  private:
    // predicate is a lambda that takes the current nested_enum struct as type template parameter and
    // returns a bool whether this is the searched for type (i.e. the enum value searched for is contained inside)
    template<auto Predicate>
    static constexpr auto find_type_recursive_internal()
    {
      if constexpr (Predicate.template operator() < E > ())
        return std::optional{ std::type_identity<E>{} };
      else
      {
        if constexpr (detail::get_tuple_size<decltype(E::subtypes)>() == 0)
          return std::optional<std::type_identity<E>>{};
        else
        {
          constexpr auto subtypesToQuery = detail::filter < []<typename T>(T &&) -> bool
          {
            return detail::is_complete_type_v<typename detail::pure_type_t<T>::type>;
          },
            E::subtypes > ();

          if constexpr (detail::get_tuple_size<decltype(subtypesToQuery)>() == 0)
            return std::optional<std::type_identity<E>>{};
          else
          {
            constexpr auto subtypesResults = detail::apply_one<false>([]<typename T>(T &&)
            {
              using subType = typename detail::pure_type_t<T>::type;
              return subType::template find_type_recursive_internal<Predicate>();
            },
              subtypesToQuery);

            constexpr auto areIncluded = detail::apply_one([](auto &&optional) -> bool
              {
                return optional.has_value();
              }, subtypesResults);

            constexpr auto index = [](const auto container, const auto value)
            {
              auto first = container.begin();
              auto last = container.end();

              for (; first != last; ++first)
                if (*first == value)
                  return std::optional{ first - container.begin() };

              return std::optional<std::ptrdiff_t>{};
            }(areIncluded, true);

            if constexpr (!index.has_value())
              return std::optional<std::type_identity<E>>{};
            else
              return std::get<index.value()>(subtypesResults);
          }
        }
      }
    }
  public:
    // returns the reflected string of an enum value that is located somewhere in the subtree
    static constexpr auto enum_string_recursive(detail::Enum auto value, bool clean = false) -> std::optional<std::string_view>
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        return std::is_same_v<decltype(value), typename T::Value>;
      } > ();

      if constexpr (!typeId.has_value())
        return {};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_string(value, clean);
      }
    }
    // returns the id of an enum value that is located somewhere in the subtree
    static constexpr auto enum_id_recursive(detail::Enum auto value) -> std::optional<std::string_view>
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        return std::is_same_v<decltype(value), typename T::Value>;
      } > ();

      if constexpr (!typeId.has_value())
        return {};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_id(value);
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its reflected string
    template<fixed_string enumString>
    static constexpr auto enum_integer_recursive()
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        constexpr auto enumInteger = T::enum_integer(std::string_view(enumString));
        return enumInteger.has_value();
      } > ();

      if constexpr (!typeId.has_value())
        return std::optional<underlying_type>{};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_integer(std::string_view(enumString));
      }
    }
    // returns the underlying integer of an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_integer_recursive_by_id()
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        constexpr auto value = T::enum_integer_by_id(std::string_view(id));
        return value.has_value();
      } > ();

      if constexpr (!typeId.has_value())
        return std::optional<underlying_type>{};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_integer_by_id(std::string_view(id));
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its reflected string
    template<fixed_string enumString>
    static constexpr auto enum_value_recursive()
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        constexpr auto value = T::enum_value(std::string_view(enumString));
        return value.has_value();
      } > ();

      if constexpr (!typeId.has_value())
        return std::optional<typename E::Value>{};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_value(std::string_view(enumString));
      }
    }
    // returns an enum value that is located somewhere in the subtree, specified by its id
    template<fixed_string id>
    static constexpr auto enum_value_recursive_by_id()
    {
      constexpr auto typeId = find_type_recursive_internal < []<typename T>()
      {
        constexpr auto value = T::enum_value_by_id(std::string_view(id));
        return value.has_value();
      } > ();

      if constexpr (!typeId.has_value())
        return std::optional<typename E::Value>{};
      else
      {
        using subType = typename detail::pure_type_t<decltype(typeId.value())>::type;
        return subType::enum_value_by_id(std::string_view(id));
      }
    }
  };

  template <typename T>
  constexpr bool operator==(const nested_enum<T> &left, const nested_enum<T> &right) noexcept
  {
    return static_cast<const T &>(left).value == static_cast<const T &>(right).value;
  }

  template <typename T>
  constexpr bool operator==(const nested_enum<T> &left, const typename T::Value &right) noexcept
  {
    return static_cast<const T &>(left).value == right;
  }

  template <typename T>
  constexpr bool operator==(const typename T::Value &left, const nested_enum<T> &right) noexcept
  {
    return left == static_cast<const T &>(right).value;
  }

  template<class E>
  concept NestedEnum = detail::DerivedOrIs<E, nested_enum<E>>;
}

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


#define NESTED_ENUM_INTERNAL_ID_WITH_COMMA(x, ...) x __VA_OPT__(,)
#define NESTED_ENUM_INTERNAL_TWO_WITH_COMMA(x, ...) (x __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_IDENTITY(typeName, ...) std::type_identity<struct typeName> __VA_OPT__(,)
#define NESTED_ENUM_INTERNAL_PARENS ()

#define NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(...) #__VA_ARGS__
#define NESTED_ENUM_INTERNAL_STRINGIFY(...) NESTED_ENUM_INTERNAL_STRINGIFY_PRIMITIVE(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL_CONCAT(x, suffix, arguments) x##suffix arguments
#define NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL(function, suffix, arguments, ...) NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL_CONCAT(function, __VA_OPT__(suffix), arguments)

// this person is a genius https://stackoverflow.com/a/62984543
// the last macro definition is a little messed up but at least it keeps the naming scheme 
#define NESTED_ENUM_INTERNAL_DEPAREN(X) NESTED_ENUM_INTERNAL_ESC(NESTED_ENUM_INTERNAL_ISH X)
#define NESTED_ENUM_INTERNAL_ISH(...) NESTED_ENUM_INTERNAL_ISH __VA_ARGS__
#define NESTED_ENUM_INTERNAL_ESC(...) NESTED_ENUM_INTERNAL_ESC_(__VA_ARGS__)
#define NESTED_ENUM_INTERNAL_ESC_(...) NESTED_ENUM_INTERNAL_VAN ## __VA_ARGS__
#define NESTED_ENUM_INTERNAL_VANNESTED_ENUM_INTERNAL_ISH

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY(a1, ...) a1
#define NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY(a1, a2, a3, ...) a3
#define NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY(a1, ...) __VA_ARGS__
#define NESTED_ENUM_INTERNAL_X_OF_MANY(helper, ...) helper(__VA_ARGS__)

#define NESTED_ENUM_INTERNAL_FOR_EACH(helper, macro, ...)                                                                           \
    __VA_OPT__(NESTED_ENUM_INTERNAL_EXPAND(helper(macro, __VA_ARGS__)))

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE(macro, a1, ...)                                                                       \
    macro(a1, __VA_ARGS__)                                                                                                          \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE

#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO(macro, a1, a2, ...)                                                                   \
    macro(a1, __VA_ARGS__)                                                                                                          \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO

#define NESTED_ENUM_INTERNAL_GET_SECOND_OF_TWO(macro, a1, a2, ...)                                                                  \
    macro(a2, __VA_ARGS__)                                                                                                          \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_SECOND_OF_TWO_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_SECOND_OF_TWO_AGAIN() NESTED_ENUM_INTERNAL_GET_SECOND_OF_TWO

#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO(macro, sequence1, item2, ...)                                                           \
    macro(item2, NESTED_ENUM_INTERNAL_X_OF_MANY(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY, NESTED_ENUM_INTERNAL_DEPAREN(sequence1)))   \
    __VA_OPT__(, NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN NESTED_ENUM_INTERNAL_PARENS (macro, (NESTED_ENUM_INTERNAL_X_OF_MANY(NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY, NESTED_ENUM_INTERNAL_DEPAREN(sequence1))), __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_INTERLEAVE_TWO_AGAIN() NESTED_ENUM_INTERNAL_INTERLEAVE_TWO


#define NESTED_ENUM_INTERNAL_INTERLEAVE_SEQUENCES(one, two) NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_INTERLEAVE_TWO, NESTED_ENUM_INTERNAL_TWO_WITH_COMMA, two, NESTED_ENUM_INTERNAL_DEPAREN(one))


#define NESTED_ENUM_INTERNAL_STRUCT_FOR_EACH(helper, macro, ...)                                                                    \
    __VA_OPT__(NESTED_ENUM_INTERNAL_EXPAND(helper(NESTED_ENUM_INTERNAL_DEFER(), macro, __VA_ARGS__)))

// this is where the magic happens, so i'll write down how it works before i forget
// the way for_each works is through macro deferred expressions and continuous rescanning by EXPAND
// this however not only rescans the current iteration but also all previous ones and everything inside them
// in order to avoid rescanning and substituting the deferred SETUP calls we need to defer the initial macro call
// as many times as there are expands - 1, hence the deferStages bit
// but because after every loop iteration one less expand is left we need to also decrease the number of deferrals
// during the process multiple deferrals are exhausted so we actually need to add one
// needs the main for_each macro to pass it the deferStages
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS(deferStages, macro, extraArgs, a1, ...)                               \
    macro deferStages (extraArgs, a1, __VA_ARGS__)                                                                                  \
    __VA_OPT__(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN NESTED_ENUM_INTERNAL_PARENS (NESTED_ENUM_INTERNAL_DEFER_ADD(deferStages), macro, extraArgs, __VA_ARGS__))
#define NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS_AGAIN() NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS


#define NESTED_ENUM_INTERNAL_STRUCT_PREP(extraArgs, arg, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP1(NESTED_ENUM_INTERNAL_DEPAREN(extraArgs), NESTED_ENUM_INTERNAL_DEPAREN(arg))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP1(parent, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP2(parent, typeName, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, NESTED_ENUM_INTERNAL_DEPAREN(__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_STRUCT_PREP3(parent, typeName, treeCall, ...) NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, treeCall __VA_OPT__(,) __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_STRUCT_PREP_CALL(parent, typeName, treeCall, ...)  NESTED_ENUM_INTERNAL_TREE_##treeCall(parent, typeName, __VA_ARGS__)


#define NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                      \
    constexpr typeName(const typeName &other) noexcept { this->value = other.value; }                                           \
    constexpr typeName(auto value) noexcept                                                                                     \
    {                                                                                                                           \
        static_assert(std::is_same_v<Value, decltype(value)>, "Value is not of the same enum type");                            \
        this->value = value;                                                                                                    \
    }                                                                                                                           \
    constexpr typeName &operator=(typeName other) noexcept                                                                      \
    {                                                                                                                           \
        this->value = other.value;                                                                                              \
        return *this;                                                                                                           \
    }                                                                                                                           \
    constexpr typeName &operator=(auto value) noexcept                                                                          \
    {                                                                                                                           \
        static_assert(std::is_same_v<Value, decltype(value)>, "Value is not of the same enum type");                            \
        this->value = value;                                                                                                    \
        return *this;                                                                                                           \
    }

#define NESTED_ENUM_INTERNAL_DEFINITION_NO_PARENT(isLeaf, placeholder, typeName, ...)                                           \
  struct typeName : public ::nested_enum::nested_enum<struct typeName, struct typeName __VA_OPT__(,) __VA_ARGS__>               \
  {                                                                                                                             \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                          \
                                                                                                                                \
    static constexpr bool isOuterNode = isLeaf;                                                                                 \
    static constexpr auto name = create_type_name(::nested_enum::fixed_string{ #typeName });                                    \
    static constexpr auto self() { return std::optional<struct typeName *>{}; }

#define NESTED_ENUM_INTERNAL_DEFINITION_(isLeaf, parent, typeName, ...)                                                         \
  struct typeName : public ::nested_enum::nested_enum<struct typeName, struct parent __VA_OPT__(,) __VA_ARGS__>           		  \
  {                                                                                                                             \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                          \
                                                                                                                                \
    static constexpr bool isOuterNode = isLeaf;                                                                                 \
    static constexpr ::nested_enum::fixed_string name = create_type_name(::nested_enum::fixed_string{ #typeName });             \
    static constexpr auto self() { constexpr auto result = (struct parent)(parent::typeName); return result; }

#define NESTED_ENUM_INTERNAL_DEFINITION_FROM(isLeaf, parent, typeName, ...)                                                     \
  struct parent::typeName : public ::nested_enum::nested_enum<struct parent::typeName, struct parent __VA_OPT__(,) __VA_ARGS__> \
  {                                                                                                                             \
    NESTED_ENUM_INTERNAL_DEFINE_CONSTRUCTORS(typeName)                                                                          \
                                                                                                                                \
    static constexpr bool isOuterNode = isLeaf;                                                                                 \
    static constexpr ::nested_enum::fixed_string name = create_type_name(::nested_enum::fixed_string{ #typeName });             \
    static constexpr auto self() { constexpr auto result = (struct parent)(parent::typeName); return result; }


#define NESTED_ENUM_INTERNAL_BODY_(...)                                                                                         \
                                                                                                                                \
    enum Value : underlying_type { };                                                                                           \
    Value value;                                                                                                                \
                                                                                                                                \
    constexpr operator Value() const noexcept { return value; }                                                                 \
                                                                                                                                \
    static constexpr std::array<Value, 0> enumValues{};                                                                         \
                                                                                                                                \
    static constexpr std::array<std::string_view, 0> ids{};																	                                    \
                                                                                                                                \
    static constexpr std::tuple<> subtypes{};                                                                                  	\
  };

#define NESTED_ENUM_INTERNAL_BODY_CONTINUE_(typeName, bodyArguments, structsArguments)                                          \
                                                                                                                                \
    enum Value : underlying_type { NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments) };                                               \
    Value value;                                                                                                                \
                                                                                                                                \
    constexpr operator Value() const noexcept { return value; }                                                                 \
                                                                                                                                \
    static constexpr std::array enumValues = ::nested_enum::detail::get_array_of_values<Value,                                  \
      NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)), false>();                                    \
                                                                                                                                \
    static constexpr auto enumStrings = ::nested_enum::detail::get_string_values<																                \
      NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)), name, false>();											        \
                                                                                                                                \
    static constexpr std::array<std::string_view, 0> ids{};																	                                    \
                                                                                                                                \
    NESTED_ENUM_INTERNAL_STRUCT_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS,                                 \
      NESTED_ENUM_INTERNAL_STRUCT_PREP, (typeName), NESTED_ENUM_INTERNAL_INTERLEAVE_SEQUENCES(bodyArguments, structsArguments)) \
                                                                                                                                \
    static constexpr std::tuple<NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE,                            \
      NESTED_ENUM_INTERNAL_STRUCT_IDENTITY, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))> subtypes{};                           \
  };

#define NESTED_ENUM_INTERNAL_BODY_CONTINUE_IDS(typeName, bodyArguments, structsArguments)                                                     \
                                                                                                                                              \
    enum Value : underlying_type { NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO,                                       \
      NESTED_ENUM_INTERNAL_ID_WITH_COMMA, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)) };                                                     \
    Value value;                                                                                                                              \
                                                                                                                                              \
    constexpr operator Value() const noexcept { return value; }                                                                               \
                                                                                                                                              \
    static constexpr std::array enumValues = ::nested_enum::detail::get_array_of_values<Value,                                                \
      NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)), true>();                                                   \
                                                                                                                                              \
    static constexpr auto enumStrings = ::nested_enum::detail::get_string_values<																                              \
      NESTED_ENUM_INTERNAL_STRINGIFY(NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)), name, true>();												                      \
                                                                                                                                              \
    static constexpr auto ids = std::to_array<std::string_view> ({ NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_SECOND_OF_TWO,      \
        NESTED_ENUM_INTERNAL_ID_WITH_COMMA, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)) });                                                  \
                                                                                                                                              \
    NESTED_ENUM_INTERNAL_STRUCT_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_ONE_WITH_EXTRA_ARGS, NESTED_ENUM_INTERNAL_STRUCT_PREP, (typeName), \
      NESTED_ENUM_INTERNAL_INTERLEAVE_SEQUENCES((NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO,                         \
      NESTED_ENUM_INTERNAL_ID_WITH_COMMA, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))), structsArguments))                                   \
                                                                                                                                              \
    static constexpr std::tuple<NESTED_ENUM_INTERNAL_FOR_EACH(NESTED_ENUM_INTERNAL_GET_FIRST_OF_TWO,                                          \
      NESTED_ENUM_INTERNAL_STRUCT_IDENTITY, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))> subtypes{};                                        	\
  };


#define NESTED_ENUM_INTERNAL_CHOOSE_BODY(typeName, bodyType, bodyArguments, ...) NESTED_ENUM_INTERNAL_BODY_CONTINUE_##bodyType (typeName, bodyArguments, (__VA_ARGS__))
#define NESTED_ENUM_INTERNAL_BODY_CONTINUE1(typeName, bodyType, bodyArguments, ...) NESTED_ENUM_INTERNAL_CHOOSE_BODY(typeName, bodyType, bodyArguments, __VA_ARGS__)
#define NESTED_ENUM_INTERNAL_BODY_CONTINUE(typeName, bodyArguments, ...)                                                                                 \
    NESTED_ENUM_INTERNAL_BODY_CONTINUE1(typeName, NESTED_ENUM_INTERNAL_X_OF_MANY(NESTED_ENUM_INTERNAL_GET_FIRST_OF_MANY, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments)),                                                  \
    (NESTED_ENUM_INTERNAL_X_OF_MANY(NESTED_ENUM_INTERNAL_SKIP_FIRST_OF_MANY, NESTED_ENUM_INTERNAL_DEPAREN(bodyArguments))), __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_SETUP(definitionType, definitionArguments, ...)                                                                             \
    NESTED_ENUM_INTERNAL_DEFINITION_##definitionType definitionArguments                                                                                 \
    NESTED_ENUM_INTERNAL_CONDITIONALLY_CALL(NESTED_ENUM_INTERNAL_BODY_, CONTINUE, (NESTED_ENUM_INTERNAL_GET_THIRD_OF_MANY definitionArguments, __VA_ARGS__), __VA_ARGS__)

#define NESTED_ENUM_INTERNAL_SETUP_AGAIN() NESTED_ENUM_INTERNAL_SETUP

// defines an outer node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_(parent, typeName, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (true, parent, typeName))
// defines an inner node inside a tree
#define NESTED_ENUM_INTERNAL_TREE_ENUM(parent, typeName, enumDefinition, ...) NESTED_ENUM_INTERNAL_SETUP_AGAIN NESTED_ENUM_INTERNAL_PARENS (, (false, parent, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__)
// only forward declares a node because the user wants to define it later 
#define NESTED_ENUM_INTERNAL_TREE_DEFER(parent, typeName, ...) struct typeName;





// defines a (root) enum that can be expanded into a tree
#define NESTED_ENUM(enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(NO_PARENT, (false, (), NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__))

// defines a deferred enum from a tree definition; needs to fill out the parent explicitly
#define NESTED_ENUM_FROM(parent, enumDefinition, ...) NESTED_ENUM_INTERNAL_EXPAND_SECOND(NESTED_ENUM_INTERNAL_SETUP(FROM, (false, parent, NESTED_ENUM_INTERNAL_DEPAREN(enumDefinition)), __VA_ARGS__))


#endif
