/*
  ==============================================================================

    vector_map.hpp
    Created: 22 Jul 2022 5:34:59pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <vector>
#include <algorithm>

namespace Framework
{
  template<typename Key, typename Value>
  struct VectorMap
  {
    std::vector<std::pair<Key, Value>> data{};

    constexpr VectorMap() = default;
    VectorMap(size_t size) noexcept { data.reserve(size); }
    VectorMap(const VectorMap &other) noexcept : data(other.data) {}
    VectorMap(VectorMap &&other) noexcept : data(std::move(other.data)) {}
    VectorMap &operator=(const VectorMap &other) noexcept
    {
      if (this != &other)
        data = other.data;
      return *this;
    }
    VectorMap &operator=(VectorMap &&other) noexcept
    {
      if (this != &other)
        data = std::move(other.data);
      return *this;
    }

    [[nodiscard]] constexpr auto find(const Key &key) noexcept
    {
      auto iter = std::ranges::find_if(data.begin(), data.end(),
        [&key](const auto &v) { return v.first == key; });
      return iter;
    }

    [[nodiscard]] constexpr auto find(const Key &key) const noexcept
    {
      auto iter = std::ranges::find_if(data.begin(), data.end(),
        [&key](const auto &v) { return v.first == key; });
      return iter;
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto find_if(Fn functor) noexcept
    {
      auto iter = std::ranges::find_if(data, functor);
      return iter;
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto find_if(Fn functor) const noexcept
    {
      auto iter = std::ranges::find_if(data, functor);
      return iter;
    }

    constexpr void add(Key key, Value value) noexcept
    { data.emplace_back(std::move(key), std::move(value)); }

    constexpr void updateValue(const Key &key, Value newValue) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;
      iter->second = std::move(newValue);
    }

    constexpr void updateValue(typename decltype(data)::iterator iterator, Value newValue) noexcept
    { (*iterator).second = std::move(newValue); }

    constexpr void updateKey(const Key &key, Key newKey) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;
      iter->first = std::move(newKey);
    }

    constexpr void updateKey(typename decltype(data)::iterator iterator, Key newKey) noexcept
    { (*iterator).first = std::move(newKey); }

    constexpr void update(const Key &key, std::pair<Key, Value> newEntry) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;

      iter->first = std::move(newEntry.first);
      iter->second = std::move(newEntry.second);
    }

    constexpr void update(typename decltype(data)::iterator iterator, std::pair<Key, Value> newEntry) noexcept
    { (*iterator) = std::move(newEntry); }

    constexpr void erase(const Key &key) noexcept
    { std::erase_if(data, [&](const auto &v) { return v.first == key; }); }

    constexpr void erase(typename decltype(data)::iterator iterator) noexcept
    { data.erase(iterator); }

    const Value &operator[](size_t index) const noexcept { return data[index].second; }
    Value &operator[](size_t index) noexcept { return data[index].second; }
  };
}