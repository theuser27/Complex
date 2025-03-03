/*
  ==============================================================================

    vector_map.hpp
    Created: 22 Jul 2022 5:34:59pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <vector>

#include "platform_definitions.hpp"
#include "stl_utils.hpp"

namespace utils
{
  template<typename Key, typename Value>
  struct VectorMap
  {
  private:
    static auto find_if(auto &container, const auto &predicate)
    {
      auto begin = container.begin();
      auto end = container.end();
      for (; begin != end; ++begin)
        if (predicate(*begin))
          break;
      
      return begin;
    }
  public:
    std::vector<utils::pair<Key, Value>> data{};

    constexpr VectorMap() = default;
    VectorMap(usize size) noexcept { data.reserve(size); }
    VectorMap(const VectorMap &other) noexcept : data(other.data) {}
    VectorMap(VectorMap &&other) noexcept : data(COMPLEX_MOVE(other.data)) {}
    VectorMap &operator=(const VectorMap &other) noexcept
    {
      if (this != &other)
        data = other.data;
      return *this;
    }
    VectorMap &operator=(VectorMap &&other) noexcept
    {
      if (this != &other)
        data = COMPLEX_MOVE(other.data);
      return *this;
    }

    [[nodiscard]] constexpr auto find(const Key &key) noexcept
    { return find_if(data, [&key](const auto &v) { return v.first == key; }); }

    [[nodiscard]] constexpr auto find(const Key &key) const noexcept
    { return find_if(data, [&key](const auto &v) { return v.first == key; }); }

    [[nodiscard]] constexpr auto find_if(const auto &functor) noexcept { return find_if(data, functor); }
    [[nodiscard]] constexpr auto find_if(const auto &functor) const noexcept { return find_if(data, functor); }

    constexpr void add(Key key, Value value) noexcept
    { data.emplace_back(COMPLEX_MOVE(key), COMPLEX_MOVE(value)); }

    constexpr void updateValue(const Key &key, Value newValue) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;
      iter->second = COMPLEX_MOVE(newValue);
    }

    constexpr void updateValue(typename decltype(data)::iterator iterator, Value newValue) noexcept
    { (*iterator).second = COMPLEX_MOVE(newValue); }

    constexpr void updateKey(const Key &key, Key newKey) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;
      iter->first = COMPLEX_MOVE(newKey);
    }

    constexpr void updateKey(typename decltype(data)::iterator iterator, Key newKey) noexcept
    { (*iterator).first = COMPLEX_MOVE(newKey); }

    constexpr void update(const Key &key, utils::pair<Key, Value> newEntry) noexcept
    {
      auto iter = find(key);
      if (iter == data.end())
        return;

      iter->first = COMPLEX_MOVE(newEntry.first);
      iter->second = COMPLEX_MOVE(newEntry.second);
    }

    constexpr void update(typename decltype(data)::iterator iterator, utils::pair<Key, Value> newEntry) noexcept
    { (*iterator) = COMPLEX_MOVE(newEntry); }

    constexpr void erase(typename decltype(data)::iterator iterator) noexcept
    { data.erase(iterator); }

    constexpr void erase(const Key &key) noexcept
    {
      auto iter = find_if(data, [&key](const auto &v) { return v.first == key; });
      data.erase(iter);
    }

    const Value &operator[](usize index) const noexcept { return data[index].second; }
    Value &operator[](usize index) noexcept { return data[index].second; }
  };
}