/*
	==============================================================================

		parameter_bridge.h
		Created: 28 Jul 2022 4:21:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "parameter_value.h"

namespace Plugin
{
	class ComplexPlugin;
}

namespace Framework
{
	class ParameterBridge : public AudioProcessorParameter
	{
		static constexpr float kDefaultParameterValue = 0.5f;
	public:
		ParameterBridge() = delete;
		ParameterBridge(const ParameterBridge &) = delete;
		ParameterBridge(ParameterBridge &&) = delete;
		ParameterBridge &operator=(const ParameterBridge &) = delete;
		ParameterBridge &operator=(ParameterBridge &&) = delete;

		ParameterBridge(Plugin::ComplexPlugin *plugin,
			u32 parameterIndex = (u32)-1, ParameterLink *link = nullptr) noexcept;

		auto *getParameterLink() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }
		void resetParameterLink(ParameterLink *link, bool getValueFromParameter = true) noexcept;

		void setValueFromUI(float newValue) noexcept
		{
			value_.store(newValue, std::memory_order_release);
			sendValueChangedMessageToListeners(newValue);
		}

		void updateUIParameter() noexcept;

		void setCustomName(const String &name)
		{
			utils::ScopedSpinLock guard{ name_.first };

			auto index = name_.second.indexOfChar(0, ' ');
			index = (index < 0) ? name_.second.length() : index;
			name_.second = String(name_.second.begin(), index);

			name_.second += name;
		}

		bool isMappedToParameter() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }

		// Inherited via AudioProcessorParameter
		float getValue() const override { return value_.load(std::memory_order_acquire); }
		void setValue(float newValue) override;
		float getDefaultValue() const override;

		String getName(int maximumStringLength) const override;
		String getLabel() const override;
		String getText(float value, int maximumStringLength) const override;
		float getValueForText(const String &text) const override;

		bool isAutomatable() const override { return true; }
		int getNumSteps() const override;
		bool isDiscrete() const override;
		bool isBoolean() const override;

	private:
		std::pair<std::atomic<bool>, String> name_{};
		std::atomic<float> value_ = kDefaultParameterValue;
		std::atomic<bool> wasValueChanged_ = false;
		std::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;

		Plugin::ComplexPlugin *plugin_ = nullptr;

		friend class Plugin::ComplexPlugin;
	};
}