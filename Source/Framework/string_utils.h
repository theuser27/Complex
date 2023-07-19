/*
  ==============================================================================

    string_utils.h
    Created: 8 Jul 2023 3:24:13am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <span>
#include <Third Party/constexpr-to-string/to_string.hpp>
#include <Third Party/fixed_string/fixed_string.hpp>

namespace utils
{
  template<size_t N, size_t totalStringSize>
  struct ConstexprStringArray
  {
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
  };

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


}
