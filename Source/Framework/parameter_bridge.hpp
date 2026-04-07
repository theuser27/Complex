
// Created: 2022-07-28 04:21:59

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "memory.hpp"

namespace Plugin
{
	struct State;
}

namespace Framework
{
	struct ParameterLink;

	class ParameterBridge
	{
	public:		
		static constexpr float kDefaultParameterValue = 0.5f;

		ParameterBridge() = delete;
		ParameterBridge(const ParameterBridge &) = delete;
		ParameterBridge(ParameterBridge &&) = delete;
		ParameterBridge &operator=(const ParameterBridge &) = delete;
		ParameterBridge &operator=(ParameterBridge &&) = delete;

		ParameterBridge(Plugin::State *state,
			u64 parameterIndex = u64(-1), ParameterLink *link = nullptr);
		~ParameterBridge();

		auto *getParameterLink() const { return parameterLinkPointer_.load(satomi::memory_order_acquire); }
		void resetParameterLink(ParameterLink *link, bool getValueFromParameter = true);
		static void notifyParameterChange();

		void beginChangeGesture();
		void setValueFromUI(float newValue);
		void endChangeGesture();

		void updateUIParameter();

		void setCustomName(utils::string name);

		bool isMappedToParameter() const { return parameterLinkPointer_.load(satomi::memory_order_acquire); }

		float getValue() const { return value_.load(satomi::memory_order_relaxed); }
		void setValue(float newValue)
		{
			auto oldValue = value_.exchange(newValue, satomi::memory_order_relaxed);
			if (oldValue != newValue)
				wasValueSet_.store(true, satomi::memory_order_relaxed);
		}
		float getDefaultValue() const;

		void getName(utils::string &outString) const;
		void getName(char *buffer, usize maximumStringLength) const;
		void getText(float value, char *buffer, usize maximumStringLength) const;
		float getValueForText(utils::string_view text) const;

		bool isDiscrete() const;
		bool isBoolean() const;

		const u64 parameterIndex;
		Plugin::State *state = nullptr;

	private:
		utils::pair<satomi::atomic<bool>, utils::string> name_{};
		satomi::atomic<float> value_ = kDefaultParameterValue;
		satomi::atomic<bool> wasValueSet_ = false;
		satomi::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;
	};
}