/*
	==============================================================================

		parameter_bridge.cpp
		Created: 29 Dec 2022 4:13:18am
		Author:  theuser27

	==============================================================================
*/

#include "parameter_bridge.h"
#include "Interface/Components/BaseControl.h"
#include "Plugin/PluginProcessor.h"

namespace Framework
{
	ParameterBridge::ParameterBridge(Plugin::ComplexPlugin *plugin,
		u32 parameterIndex, ParameterLink *link) noexcept
	{
		plugin_ = plugin;

		// should be enough i think
		name_.second.preallocateBytes(64);
		// case when the parameter mapping remains permanent for the entirety of the plugin lifetime
		if (parameterIndex == (u32)(-1) && link)
		{
			parameterLinkPointer_.store(link, std::memory_order_release);
			name_.second += Interface::toJuceString(link->parameter->getParameterDetails().displayName);
			link->hostControl = this;
			value_ = link->parameter->getNormalisedValue();
		}
		else if (link)
		{
			name_.second += 'P';
			name_.second += (int)parameterIndex;
			name_.second += " > ";
			parameterLinkPointer_.store(link, std::memory_order_release);
			name_.second += Interface::toJuceString(link->parameter->getParameterDetails().displayName);
			link->hostControl = this;
			value_ = link->parameter->getNormalisedValue();
		}
		else
		{
			name_.second += 'P';
			name_.second += (int)parameterIndex;
		}
	}

	void ParameterBridge::resetParameterLink(ParameterLink *link, bool getValueFromParameter) noexcept
	{
		if (link == parameterLinkPointer_.load(std::memory_order_acquire))
			return;

		parameterLinkPointer_.store(link, std::memory_order_release);
		utils::ScopedSpinLock guard{ name_.first };

		auto index = name_.second.indexOfChar(0, ' ');
		index = (index < 0) ? name_.second.length() : index;
		if (!link)
			name_.second = String(name_.second.begin(), index);
		else
		{
			link->parameter->changeControl(this);
			if (getValueFromParameter)
			{
				auto newValue = link->parameter->getNormalisedValue();
				value_.store(newValue, std::memory_order_release);
				sendValueChangedMessageToListeners(newValue);
			}
			else
				link->UIControl->setValueFromHost();

			String newString{};
			newString.preallocateBytes(64);
			newString.append(name_.second, index);
			newString += " > ";
			newString += Interface::toJuceString(link->parameter->getParameterDetails().displayName);
			name_.second = std::move(newString);
		}

		guard.~ScopedSpinLock();

		utils::as<ComplexAudioProcessor>(plugin_)
			->updateHostDisplay(AudioProcessor::ChangeDetails{}.withParameterInfoChanged(true));
	}

	void ParameterBridge::updateUIParameter() noexcept
	{
		// for wasValueChanged_ we only require atomicity, therefore memory_order_relaxed suffices
		// for parameterLinkPointer_ it's fine to use relaxed because this method is only called from the message thread
		//	which is the only one that touches the UI and we only want to get the linked UI parameter
		if (wasValueChanged_.load(std::memory_order_relaxed))
		{
			parameterLinkPointer_.load(std::memory_order_relaxed)->UIControl->valueChanged();
			wasValueChanged_.store(false, std::memory_order_relaxed);
		}
	}

	void ParameterBridge::setValue(float newValue)
	{
		// we store the new value and if the bridge is linked, we notify the UI
		value_.store(newValue, std::memory_order_release);
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
			pointer && pointer->UIControl)
		{
			bool wasValueChanged = pointer->UIControl->setValueFromHost();
			wasValueChanged_.store(wasValueChanged, std::memory_order_relaxed);
		}
	}

	float ParameterBridge::getDefaultValue() const
	{
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			return std::clamp(pointer->parameter->getParameterDetails().defaultNormalisedValue, 0.0f, 1.0f);
		return kDefaultParameterValue;
	}

	String ParameterBridge::getName(int maximumStringLength) const
	{
		// this is hacky i know but there's no way to declare the atomic mutable inside the pair
		utils::ScopedSpinLock guard{ const_cast<std::atomic<bool>&>(name_.first) };

		if (parameterLinkPointer_.load(std::memory_order_acquire))
			return name_.second.substring(0, maximumStringLength);

		auto index = name_.second.indexOfChar(0, ' ');
		return name_.second.substring(0, (index > maximumStringLength) ? maximumStringLength : index);
	}

	String ParameterBridge::getLabel() const
	{
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			return pointer->parameter->getParameterDetails().displayUnits.data();
		return "";
	}

	String ParameterBridge::getText(float value, int maximumStringLength) const
	{
		auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
		if (!pointer)
			return { value, maximumStringLength };

		auto details = pointer->parameter->getParameterDetails();
		auto sampleRate = plugin_->getSampleRate();
		double internalValue = scaleValue(value, details, sampleRate, true);
		if (!details.stringLookup.empty())
		{
			auto index = (size_t)std::clamp((float)internalValue, details.minValue, details.maxValue) - (size_t)details.minValue;
			// clamping in case the value goes above the string count inside the array
			index = std::clamp(index, (size_t)0, details.stringLookup.size());
			return { details.stringLookup[index].data(), (size_t)maximumStringLength };
		}

		return { internalValue, maximumStringLength };
	}

	float ParameterBridge::getValueForText(const String &text) const
	{
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			return (float)unscaleValue(text.getFloatValue(), pointer->parameter->getParameterDetails(), true);

		return text.getFloatValue();
	}

	int ParameterBridge::getNumSteps() const
	{
		if (!isDiscrete())
			return AudioProcessorParameter::getNumSteps();

		auto pointer = parameterLinkPointer_.load(std::memory_order_acquire);
		return (int)pointer->parameter->getParameterDetails().maxValue - 
			(int)pointer->parameter->getParameterDetails().minValue + 1;
	}

	bool ParameterBridge::isDiscrete() const
	{
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			return pointer->parameter->getParameterDetails().scale == ParameterScale::Indexed;
		return false;
	}

	bool ParameterBridge::isBoolean() const
	{
		if (auto pointer = parameterLinkPointer_.load(std::memory_order_acquire))
			return pointer->parameter->getParameterDetails().scale == ParameterScale::Toggle;
		return false;
	}
}
