/*
	==============================================================================

		vector_map.h
		Created: 22 Jul 2022 5:34:59pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <array>
#include <vector>
#include "common.h"

namespace Framework
{
	template<typename Key, typename Value, size_t size>
	struct ConstexprMap
	{
		std::array<std::pair<Key, Value>, size> data{};

		constexpr ConstexprMap() = default;

		[[nodiscard]] constexpr auto find(const Key &key)
		{
			auto iter = std::find_if(data.begin(), data.end(),
				[&key](const auto &v) { return v.first == key; });
			COMPLEX_ASSERT(iter != data.end());
			return iter;
		}

		[[nodiscard]] constexpr auto find(const Key &key) const
		{
			auto iter = std::find_if(data.begin(), data.end(),
				[&key](const auto &v) { return v.first == key; });
			COMPLEX_ASSERT(iter != data.end());
			return iter;
		}
	};

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
			auto iter = std::find_if(data.begin(), data.end(),
				[&key](const auto &v) { return v.first == key; });
			return iter;
		}

		[[nodiscard]] constexpr auto find(const Key &key) const noexcept
		{
			auto iter = std::find_if(data.begin(), data.end(),
				[&key](const auto &v) { return v.first == key; });
			return iter;
		}

		constexpr void add(Key key, Value value) noexcept
		{ data.emplace_back(key, value); }

		constexpr void update(const Key &key, const Value &newValue) noexcept
		{
			auto iter = find(key);
			if (iter == data.end())
				return;
			iter->second = newValue;
		}

		constexpr void update(const Key &key, const std::pair<Key, Value> &newEntry) noexcept
		{
			auto iter = find(key);
			if (iter != data.end())
				return;

			iter->first = newEntry.first;
			iter->second = newEntry.second;
		}

		constexpr void erase(const Key &key) noexcept
		{ std::erase_if(data, [&key](const auto &v) { return v.first == key; }); }

		const Value &operator[](size_t index) const noexcept
		{ return data[index].second; }
	};
}