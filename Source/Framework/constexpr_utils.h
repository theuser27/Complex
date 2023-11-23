/*
  ==============================================================================

    constexpr_utils.h
    Created: 8 Jul 2023 3:24:13am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <span>
#include <functional>
#include <algorithm>
#include <numeric>
#include <Third Party/constexpr-to-string/to_string.hpp>
#include <Third Party/fixed_string/fixed_string.hpp>

namespace CommonConcepts
{
  template<typename Derived, typename Base>
  concept DerivedOrIs = std::derived_from<Derived, Base> || std::same_as<Derived, Base>;

  template<typename T>
  concept Enum = std::is_enum_v<T>;
}

namespace utils
{
  // whether a type is complete or not
  // taken from https://stackoverflow.com/a/37193089
  template <class T, class = void>
  struct is_complete_type : std::false_type { };

  template <class T>
  struct is_complete_type<T, decltype(void(sizeof(T)))> : std::true_type { };

  template <class T>
  inline constexpr auto is_complete_type_v = is_complete_type<T>::value;

  // calls a function on every tuple element individually
  template <class Callable, class TupleLike>
  constexpr decltype(auto) applyOne(Callable &&callable, TupleLike &&tuple)
  {
    return []<size_t ... Indices>(Callable &&callable, TupleLike &&tuple, std::index_sequence<Indices...>) -> decltype(auto)
    {
      if constexpr (std::is_same_v<std::invoke_result_t<Callable, std::tuple_element_t<0, std::remove_cvref_t<TupleLike>>>, void>)
        (std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<TupleLike>(tuple))), ...);
      else
        return std::array{ std::invoke(std::forward<Callable>(callable), std::get<Indices>(std::forward<TupleLike>(tuple)))... };
    }(std::forward<Callable>(callable), std::forward<TupleLike>(tuple),
      std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<TupleLike>>>{});
  }

  // calls a function on every tuple element individually
  // callable must be a functor
  template <auto TupleLike, class Callable>
  constexpr decltype(auto) applyOne(Callable &&callable)
  {
    return[]<size_t ... Indices>(Callable && callable, std::index_sequence<Indices...>) -> decltype(auto)
    {
      return std::array{ callable.template operator()<std::get<Indices>(TupleLike)>()... };
    }(std::forward<Callable>(callable), std::make_index_sequence<std::tuple_size_v<decltype(TupleLike)>>{});
  }
  
  // the following 2 functions are based on those here with the same names
  // https://github.com/GuillaumeDua/GCL_CPP/blob/6521a51f9bb6492fe90c95ec94aad3bf7c2d1e85/includes/gcl/cx/typeinfo.hpp#L19
  template <typename T>
  constexpr auto type_name()
  {
    std::string_view name;
  #if defined(__GNUC__) or defined (__clang__)
    name = __PRETTY_FUNCTION__;
    std::string_view typeBit = "T = ";
    name.remove_prefix(name.find(typeBit) + typeBit.length());
    name.remove_suffix(name.length() - name.rfind(']'));
  #elif defined(_MSC_VER)
    name = __FUNCSIG__;
    name.remove_prefix(name.find('<') + 1);
    std::string_view enumBit = "enum ";
    if (auto enumPos = name.find(enumBit); enumPos != std::string_view::npos)
      name.remove_prefix(enumPos + enumBit.length());
    name.remove_suffix(name.length() - name.rfind(">(void)"));
  #else
    #error Unsupported Compiler
  #endif

    return name;
  }

  template<auto Value>
  constexpr auto value_name()
  {
    std::string_view name;
  #if defined(__GNUC__) or defined (__clang__)
    name = __PRETTY_FUNCTION__;
    std::string_view valueBit = "Value = ";
    name.remove_prefix(name.find(valueBit) + valueBit.length());
    name.remove_suffix(name.length() - name.rfind(']'));
  #elif defined(_MSC_VER)
    name = __FUNCSIG__;
    name.remove_prefix(name.find('<') + 1);
    std::string_view enumBit = "enum ";
    if (auto enum_token_pos = name.find(enumBit); enum_token_pos != std::string_view::npos)
      name.remove_prefix(enum_token_pos + enumBit.length());
    name.remove_suffix(name.length() - name.rfind(">(void)"));
  #else
    #error Unsupported Compiler
  #endif
    return name;
  }

  template<class Tuple>
  constexpr auto tupleToArray(Tuple &&tuple)
  {
    constexpr auto getArray = [](auto&& ... x) { return std::array{ std::forward<decltype(x)>(x) ... }; };
    return std::apply(getArray, std::forward<Tuple>(tuple));
  }

  template<class Tuple>
  constexpr auto tupleOfArraysToArray(Tuple &&tuple)
  {
    constexpr auto function = []<auto Self, typename T, size_t N, typename ... Args>(std::array<T, N> first, Args ... args)
    {
      if constexpr (sizeof...(Args) == 0)
        return first;
      else
      {
        auto next = Self.template operator()<Self>(args...);
        std::array<T, N + next.size()> newArray;

        for (size_t i = 0; i < first.size(); i++)
          newArray[i] = first[i];

        for (size_t i = 0; i < next.size(); i++)
          newArray[first.size() + i] = next[i];

        return newArray;
      }
    };

    return[&]<size_t ... Indices>(Tuple &&tuple, std::index_sequence<Indices...>)
    {
      return function.template operator()<function>(std::get<Indices>(tuple)...);
    }(std::forward<Tuple>(tuple), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
  }

  template <size_t N>
  struct static_str
  {

    using char_type = std::string_view::value_type;

    constexpr static_str(const char_type(&array)[N + 1]) noexcept
    {
      std::copy(std::begin(array), std::end(array), data_.begin());
    }

    constexpr const char_type *data() const noexcept { return data_.data(); }
    constexpr size_t size() const noexcept { return N; }

    constexpr operator std::string_view() const noexcept { return { data(), size() }; }

    std::array<char_type, N + 1> data_;
  };

  template <size_t N>
  static_str(const char(&)[N])->static_str<N - 1>;

  template<size_t N, size_t totalStringSize>
  struct ConstexprStringArray
  {
    // an array of indices where string_views begin and
    // a giant array of all the null-terminated string data
    struct DataHolder
    {
      constexpr DataHolder() = default;
      constexpr DataHolder(const DataHolder &other) = default;
      constexpr DataHolder(DataHolder &&other) noexcept
      {
        strings = std::move(other.strings);
        stringIndices = std::move(other.stringIndices);
      }
      constexpr DataHolder &operator=(DataHolder &&other) noexcept
      {
        strings = std::move(other.strings);
        stringIndices = std::move(other.stringIndices);
        return *this;
      }
      static constexpr auto totalSize() noexcept { return totalStringSize; }
      static constexpr auto size() noexcept { return N; }
      constexpr const char *data() const noexcept { return strings; }
      constexpr const char &operator[](std::size_t index) { return strings[index]; }

      std::array<size_t, N> stringIndices{};
      std::array<char, totalStringSize> strings{};
    };

    constexpr ConstexprStringArray() = default;
    constexpr ConstexprStringArray(const ConstexprStringArray &other) = default;
    constexpr ConstexprStringArray(ConstexprStringArray &&other) noexcept
    {
      dataHolder = std::move(other.dataHolder);
      for (size_t i = 0; i < N; i++)
        views[i] = std::string_view(dataHolder.strings.data() + dataHolder.stringIndices[i]);
    }
    constexpr ConstexprStringArray(DataHolder &&other) noexcept
    {
      dataHolder = std::move(other);
    }
    static constexpr auto totalSize() noexcept { return totalStringSize; }
    static constexpr auto size() noexcept { return N; }
    constexpr const char *data() const noexcept { return dataHolder.data(); }
    constexpr std::span<const std::string_view> getSpan() const noexcept { return views; }
    constexpr auto operator[](std::size_t index) const noexcept
    {
      if (!std::is_constant_evaluated())
        return views[index];

      auto i = dataHolder.stringIndices[index];
      return std::string_view(dataHolder.strings.data() + i);
    }

    std::array<std::string_view, N> views{};
    DataHolder dataHolder{};
  };

  template<size_t desiredSize, size_t trimSource = 0, size_t trimDestination = 0, size_t currentSize, typename T>
  consteval std::array<T, desiredSize> toDifferentSizeArray(std::array<T, currentSize> source, T init = {})
  {
    std::array<T, desiredSize> destination{};
    auto moveSize = std::min(desiredSize - trimDestination, currentSize - trimSource);
    for (size_t i = 0; i < moveSize; i++)
      destination[i] = std::move(source[i]);
    if (desiredSize - trimDestination > moveSize)
    {
      for (size_t i = moveSize; i < desiredSize - trimDestination; ++i)
        destination[i] = init;
    }
    return destination;
  }

  /**
   * @tparam start number where the sequence should begin
   * @tparam size sequence length
   * @tparam offset difference between elements (arithmetic sequence)
   * @tparam multiplier multiplier factor between elements (geometric sequence)
   */
  template<int64_t start, size_t size, int64_t offset = 1, int64_t multiplier = 1, size_t count = 1>
  consteval auto numberSequenceStrings()
  {
    static_assert(size > 0, "You need a non-negative number of elements to generate");
    const char *number = to_string<start>;

    if constexpr (count == size)
      return std::array<std::string_view, 1>{ number };
    else
    {
      std::array<std::string_view, size - count + 1> numberStrings{};
      numberStrings[0] = number;

      auto numbersToConcat = numberSequenceStrings<(start + offset) *multiplier, size, offset, multiplier, count + 1>();
      for (size_t i = 0; i < numbersToConcat.size(); i++)
        numberStrings[i + 1] = numbersToConcat[i];

      return numberStrings;
    }
  }

  template<size_t end, size_t start = 0, typename T>
  constexpr auto getArrayDataSize(T array)
  {
    if constexpr (start == end - 1)
      return array[start].size();
    else
      return array[start].size() + getArrayDataSize<end, start + 1, T>(array);
  }

  template<typename... Args>
  constexpr auto getArraysDataSize(Args ... arrays) { return (getArrayDataSize<arrays.size()>(arrays) + ...); }

  template<typename... Args>
  constexpr auto getArraysSize(Args ... arrays) { return (arrays.size() + ...); }

  template<typename... Args>
  constexpr void concatenateStringArrays(char *destination, size_t &destinationIndex,
    std::string_view delimiter, size_t iteration, const auto strings, Args... args)
  {
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(strings)>, std::string_view>)
    {
      std::char_traits<char>::copy(destination + destinationIndex,
        strings.data(), strings.size());
      destinationIndex += strings.size();
    }
    else
    {
      std::char_traits<char>::copy(destination + destinationIndex,
        strings[iteration].data(), strings[iteration].size());
      destinationIndex += strings[iteration].size();
    }


    if constexpr (sizeof...(Args) == 0)
    {
      constexpr char nullTerminator[] = "\0";
      std::char_traits<char>::copy(destination + destinationIndex, nullTerminator, 1);
      destinationIndex++;
      return;
    }
    else
    {
      std::char_traits<char>::copy(destination + destinationIndex, delimiter.data(), delimiter.size());
      destinationIndex += delimiter.size();
      concatenateStringArrays(destination, destinationIndex, delimiter, iteration, args...);
    }
  }

  template<typename... Args>
  constexpr void recursivelyCombineStringViewArrays(auto &dataHolder, size_t characterIndex,
    size_t dataHolderIndex, const auto strings, Args ... args)
  {
    // inserting based on if strings is a single string or an array of strings
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(strings)>, std::string_view>)
    {
      dataHolder.stringIndices[dataHolderIndex] = characterIndex;
      concatenateStringArrays(dataHolder.strings.data(), characterIndex, " ", 0, strings);
      dataHolderIndex++;
    }
    else
    {
      for (size_t i = 0; i < strings.size(); i++)
      {
        dataHolder.stringIndices[dataHolderIndex] = characterIndex;
        concatenateStringArrays(dataHolder.strings.data(), characterIndex, " ", i, strings);
        dataHolderIndex++;
      }
    }

    if constexpr (sizeof...(Args) > 0)
      recursivelyCombineStringViewArrays(dataHolder, characterIndex, dataHolderIndex, args...);
  }

  template<size_t totalSize, size_t totalIndices, typename... Args>
  constexpr auto combineStringViewArrays(Args ... args)
  {
    // + totalIndices because of the null terminator
    constexpr auto fullTotalSize = totalSize + totalIndices;

    using StringArray = ConstexprStringArray<totalIndices, fullTotalSize>;
    using DataHolder = typename ConstexprStringArray<totalIndices, fullTotalSize>::DataHolder;
    DataHolder dataHolder{};

    recursivelyCombineStringViewArrays(dataHolder, 0, 0, args...);
    StringArray stringArray{ std::move(dataHolder) };
    return stringArray;
  }

  template<size_t insertIndex, size_t destinationIndices, size_t totalSize,
    size_t totalIndices, typename T, typename U>
  constexpr auto insertStringViewsArray(T destination, U source)
  {
    static_assert(insertIndex < totalIndices, "the insert index can't be larger than the number of indices");
    // + totalIndices because of the null terminator
    constexpr auto fullTotalSize = totalSize + totalIndices;

    using StringArray = ConstexprStringArray<totalIndices, fullTotalSize>;
    using DataHolder = typename ConstexprStringArray<totalIndices, fullTotalSize>::DataHolder;
    DataHolder dataHolder{};

    size_t dataHolderIndex = 0;
    size_t characterIndex = 0;

    // destination data before insertion
    for (size_t i = 0; i < insertIndex; i++)
    {
      dataHolder.stringIndices[dataHolderIndex] = characterIndex;
      concatenateStringArrays(dataHolder.strings.data(), characterIndex, "", i, destination);
      dataHolderIndex++;
    }

    // inserted section, inserting it based on its type
    if constexpr (std::is_same_v<std::remove_cvref_t<decltype(source)>, std::string_view>)
    {
      dataHolder.stringIndices[dataHolderIndex] = characterIndex;
      concatenateStringArrays(dataHolder.strings.data(), characterIndex, "", 0, source);
      dataHolderIndex++;
    }
    else
    {
      for (size_t i = 0; i < source.size(); i++)
      {
        dataHolder.stringIndices[dataHolderIndex] = characterIndex;
        concatenateStringArrays(dataHolder.strings.data(), characterIndex, "", i, source);
        dataHolderIndex++;
      }
    }

    // destination data after insertion
    for (size_t i = insertIndex; i < destinationIndices - insertIndex + 1; i++)
    {
      dataHolder.stringIndices[dataHolderIndex] = characterIndex;
      concatenateStringArrays(dataHolder.strings.data(), characterIndex, "", i, destination);
      dataHolderIndex++;
    }

    StringArray stringArray{ std::move(dataHolder) };
    return stringArray;
  }

  template<fixstr::fixed_string delimiter, size_t totalSize, size_t totalIndices, typename... Args>
  constexpr auto appendStringViewsArrays(Args ... args)
  {
    // + totalIndices for the null terminators
    constexpr auto fullTotalSize = totalSize + totalIndices +
      delimiter.size() * (sizeof...(Args) - 1) * totalIndices;

    using StringArray = ConstexprStringArray<totalIndices, fullTotalSize>;
    using DataHolder = typename ConstexprStringArray<totalIndices, fullTotalSize>::DataHolder;
    DataHolder dataHolder{};

    size_t characterIndex = 0;
    for (size_t i = 0; i < totalIndices; i++)
    {
      dataHolder.stringIndices[i] = characterIndex;
      concatenateStringArrays(dataHolder.strings.data(), characterIndex, delimiter.c_str(), i, args...);
    }

    StringArray stringArray{ std::move(dataHolder) };
    return stringArray;
  }

  template<size_t Size, typename Elem = char>
  class string_multi_view
  {
  public:
    constexpr string_multi_view() = default;
    constexpr string_multi_view(std::array<std::basic_string_view<Elem>, Size> array)
    {
      views_ = std::move(array);
      currentSize_ = Size;
    }

    constexpr void addView(std::basic_string_view<Elem> view)
    {
      if (!std::is_constant_evaluated())
        COMPLEX_ASSERT(currentSize_ < Size);
      views_[currentSize_++] = view;
    }

    constexpr auto getViews() const noexcept { return views_; }

  private:
    std::array<std::basic_string_view<Elem>, Size> views_{};
    size_t currentSize_ = 0;
  };

  template<size_t Size, typename Elem = char>
  constexpr string_multi_view<Size, Elem> to_multi_view(const std::basic_string_view<Elem>(&array)[Size])
  {
    return []<std::size_t... I>(const std::basic_string_view<Elem>(&array)[Size], std::index_sequence<I...>)
      ->std::array<std::basic_string_view<Elem>, Size>
    {
      return { { array[I]... } };
    }(array, std::make_index_sequence<Size>{});
  }

  template <typename Elem, size_t SizeOne, size_t SizeTwo>
  constexpr bool operator==(const string_multi_view<SizeOne, Elem> &left,
    const string_multi_view<SizeTwo, Elem> &right) noexcept
  {
    auto leftArray = left.getViews();
    auto rightArray = right.getViews();

    {
      size_t leftTotalSize = 0, rightTotalSize = 0;
      for (auto &view : leftArray)
        leftTotalSize += view.size();

      for (auto &view : rightArray)
        rightTotalSize += view.size();

      if (leftTotalSize != rightTotalSize)
        return false;
    }

    using traits = typename decltype(leftArray)::value_type::traits_type;
    size_t leftViewIndex = 0, rightViewIndex = 0;
    size_t leftIndex = 0, rightIndex = 0;

    while (true)
    {
      size_t sizeToCompare = std::min(leftArray[leftViewIndex].size() - leftIndex,
        rightArray[rightViewIndex].size() - rightIndex);

      int comparisonValue = traits::compare(leftArray[leftViewIndex].data() + leftIndex,
        rightArray[rightViewIndex].data() + rightIndex, sizeToCompare);

      if (comparisonValue != 0)
        return false;

      leftIndex += sizeToCompare;
      rightIndex += sizeToCompare;

      int64_t sizeDifference = (leftArray[leftViewIndex].size() - leftIndex) -
        (rightArray[rightViewIndex].size() - rightIndex);

      if (sizeDifference >= 0)
      {
        if (rightViewIndex == rightArray.size() - 1)
          break;
        ++rightViewIndex;
        rightIndex = 0;
      }

      if (sizeDifference <= 0)
      {
        if (leftViewIndex == leftArray.size() - 1)
          break;
        ++leftViewIndex;
        leftIndex = 0;
      }
    }

    return true;
  }

  constexpr int getDigit(char character)
  {
    if (character >= '0' && character <= '9')
      return character - '0';

    if (character >= 'A' && character <= 'Z')
      return character - 'A' + 10;

    if (character >= 'a' && character <= 'z')
      return character - 'a' + 10;

    return 0;
  }

  constexpr std::string_view trimWhiteSpace(std::string_view view)
  {
    auto begin = view.find_first_not_of(' ');
    if (begin == std::string::npos)
      return {};

    auto end = view.find_last_not_of(' ');
    auto range = end - begin + 1;

    return view.substr(begin, range);
  }

  template<std::integral Int = int64_t>
  constexpr Int getIntFromString(std::string_view view)
  {
    auto trimmedView = trimWhiteSpace(view);

    bool isNegative = false;
    if (trimmedView.starts_with('-'))
    {
      trimmedView.remove_prefix(1);
      isNegative = true;
    }

    Int number = 0, decimal = 1;
    for (size_t i = trimmedView.size(); i-- > 0; )
    {
      if (trimmedView[i] == ' ' || trimmedView[i] == '\'')
        continue;

      number += (Int)getDigit(trimmedView[i]) * decimal;
      decimal *= 10;
    }

    return (isNegative) ? -number : number;
  }
}
