/*
	==============================================================================

		parameter_bridge.h
		Created: 28 Jul 2022 4:21:59am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "AppConfig.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "platform_definitions.h"

namespace Plugin
{
	class ComplexPlugin;
}

namespace Framework
{
	struct ParameterLink;

	class ParameterBridge final : public juce::AudioProcessorParameter
	{
		static constexpr float kDefaultParameterValue = 0.5f;
	public:
		ParameterBridge() = delete;
		ParameterBridge(const ParameterBridge &) = delete;
		ParameterBridge(ParameterBridge &&) = delete;
		ParameterBridge &operator=(const ParameterBridge &) = delete;
		ParameterBridge &operator=(ParameterBridge &&) = delete;

		ParameterBridge(Plugin::ComplexPlugin *plugin,
			u64 parameterIndex = UINT64_MAX, ParameterLink *link = nullptr) noexcept;

		auto *getParameterLink() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }
		void resetParameterLink(ParameterLink *link, bool getValueFromParameter = true) noexcept;

		void setValueFromUI(float newValue) noexcept
		{
			value_.store(newValue, std::memory_order_release);
			sendValueChangedMessageToListeners(newValue);
		}

		void updateUIParameter() noexcept;

		void setCustomName(const juce::String &name);

		u64 getIndex() const noexcept { return parameterIndex_; }
		bool isMappedToParameter() const noexcept { return parameterLinkPointer_.load(std::memory_order_acquire); }

		// Inherited via AudioProcessorParameter
		float getValue() const override { return value_.load(std::memory_order_acquire); }
		void setValue(float newValue) override;
		float getDefaultValue() const override;

		juce::String getName(int maximumStringLength) const override;
		juce::String getLabel() const override;
		juce::String getText(float value, int maximumStringLength) const override;
		float getValueForText(const juce::String &text) const override;

		bool isAutomatable() const override { return true; }
		int getNumSteps() const override;
		bool isDiscrete() const override;
		bool isBoolean() const override;

	private:
		u64 parameterIndex_ = UINT64_MAX;
		std::pair<std::atomic<bool>, juce::String> name_{};
		std::atomic<float> value_ = kDefaultParameterValue;
		std::atomic<bool> wasValueChanged_ = false;
		std::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;

		Plugin::ComplexPlugin *plugin_ = nullptr;

		friend class Plugin::ComplexPlugin;
	};
}