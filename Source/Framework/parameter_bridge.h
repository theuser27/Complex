/*
	==============================================================================

		parameter_bridge.h
		Created: 28 Jul 2022 4:21:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
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
			u32 parameterIndex = u32(-1), ParameterLink *link = nullptr) noexcept;

		// DON'T CALL ON AUDIO THREAD
		// because of the stupid way juce::String works
		// every time the name is changed a separate string needs to be allocated
		void resetParameterLink(ParameterLink *link) noexcept;

		void setValueFromUI(float newValue) noexcept
		{
			value_.store(newValue, std::memory_order_release);
			sendValueChangedMessageToListeners(newValue);
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
		std::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;

		Plugin::ComplexPlugin *plugin_ = nullptr;

		friend class Plugin::ComplexPlugin;
	};
}