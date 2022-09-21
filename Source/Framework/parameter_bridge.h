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
#include "./SoundEngine/PluginModule.h"

namespace Framework
{
	class ParameterBridge : public AudioProcessorParameter, public ParameterValueControl
	{
	public:
		ParameterBridge() = delete;
		ParameterBridge(const ParameterBridge &) = delete;
		ParameterBridge(ParameterBridge &&) = delete;
		ParameterBridge &operator=(const ParameterBridge &) = delete;
		ParameterBridge &operator=(ParameterBridge &&) = delete;

		ParameterBridge(u32 parameterIndex = u32(-1), ParameterLink *link = nullptr) noexcept
		{
			// should be enough i think
			name_.preallocateBytes(64);
			// case when the parameter mapping remains permanent for the entirety of the plugin lifetime
			if (parameterIndex == -1 && link)
			{
				parameterLinkPointer_.store(link, std::memory_order_release);
				name_ += link->parameter->getParameterDetails()->displayName.data();
				link->hostControl = this;
				value_ = link->parameter->getNormalisedValue();
			}
			else if (link)
			{
				name_ += 'P';
				name_ += (int)parameterIndex;
				name_ += " > ";
				parameterLinkPointer_.store(link, std::memory_order_release);
				name_ += link->parameter->getParameterDetails()->displayName.data();
				link->hostControl = this;
				value_ = link->parameter->getNormalisedValue();
			}
			else
			{
				name_ += 'P';
				name_ += (int)parameterIndex;
			}			
		}

		// DON'T CALL ON AUDIO THREAD
		// because of the stupid way juce::String works
		// every time the name needs to be changed i need to allocate a separate string
		void linkToParameter(ParameterLink *link) noexcept
		{
			if (link == parameterLinkPointer_.load(std::memory_order_acquire))
				return;

			parameterLinkPointer_.store(link, std::memory_order_release);

			size_t index = name_.indexOfChar(0, ' ');
			if (!link)
			{
				name_ = String(name_.begin(), index);
				return;
			}

			link->hostControl = this;
			value_ = link->parameter->getNormalisedValue();
			
			String newString{};
			newString.preallocateBytes(64);
			newString.append(name_, index);
			newString += " > ";
			newString += link->parameter->getParameterDetails()->displayName.data();
			name_ = std::move(newString);
		}
		
		// Inherited via ParameterValueControl
		// value changed from UI
		void setValueInternal(float newValue) noexcept override
		{
			if (!parameterLinkPointer_.load(std::memory_order_acquire))
				return;

			value_.store(newValue, std::memory_order_release);
			sendValueChangedMessageToListeners(newValue);
		}

		// Inherited via AudioProcessorParameter
		float getValue() const override
		{ return value_.load(std::memory_order_acquire); }

		void setValue(float newValue) override
		{
			// we store the new value and if the bridge is linked, we notify the UI
			value_.store(newValue, std::memory_order_release);
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				if (pointer->UIControl)
					pointer->UIControl->setValueInternal(newValue);
		}

		float getDefaultValue() const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return pointer->parameter->getParameterDetails()->defaultNormalisedValue;
			return kDefaultParameterValue;
		}

		String getName(int maximumStringLength) const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return name_.substring(0, maximumStringLength);
			
			auto index = name_.indexOfChar(0, ' ');
			return name_.substring(0, (index > maximumStringLength) ? maximumStringLength : index);
		}

		String getLabel() const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return pointer->parameter->getParameterDetails()->displayUnits.data();
			return "";
		}

		String getText(float value, int maximumStringLength) const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			{
				String result{};
				auto details = pointer->parameter->getParameterDetails();
				float internalValue = utils::scaleValue(value_.load(std::memory_order_acquire), details, true)[0];
				if (details->stringLookup)
					result = details->stringLookup[std::clamp<size_t>(internalValue, 
						details->minValue, details->maxValue) - (size_t)details->minValue].data();
				else
					result = String(internalValue);

				return result;
			}
			else return String(value_.load(std::memory_order_acquire));
		}

		float getValueForText(const String &text) const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return utils::unscaleValue(text.getFloatValue(), pointer->parameter->getParameterDetails());
			else return text.getFloatValue();
		}

		bool isAutomatable() const override { return true; }

		int getNumSteps() const override
		{
			if (!isDiscrete())
				return AudioProcessorParameter::getNumSteps();

			auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
			return (int)pointer ->parameter->getParameterDetails()->maxValue - (int)pointer->parameter->getParameterDetails()->minValue + 1;
		}

		bool isDiscrete() const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return pointer->parameter->getParameterDetails()->scale == ParameterScale::Indexed;
			return false;
		}

		bool isBoolean() const override
		{
			if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
				return pointer->parameter->getParameterDetails()->scale == ParameterScale::Toggle;
			return false;
		}

	private:
		String name_{};
		std::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;
	};
}