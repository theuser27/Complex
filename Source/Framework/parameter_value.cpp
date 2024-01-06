/*
	==============================================================================

		parameter_value.cpp
		Created: 27 Dec 2022 2:57:25am
		Author:  theuser27

	==============================================================================
*/

#include "parameter_value.h"
#include "parameter_bridge.h"
#include "Interface/Components/BaseControl.h"

namespace Framework
{
	void ParameterValue::initialise(std::optional<float> value)
	{
		utils::ScopedSpinLock lock(waitLock_);

		normalisedValue_ = value.value_or(details_.defaultNormalisedValue);
		modulations_ = 0.0f;
		normalisedInternalValue_ = value.value_or(details_.defaultNormalisedValue);
		internalValue_ = (!value.has_value()) ? details_.defaultValue :
			(float)scaleValue(value.value(), details_);
		isDirty_ = false;
	}

	auto ParameterValue::changeControl(std::variant<Interface::BaseControl *, ParameterBridge *> control) noexcept 
		-> std::variant<Interface::BaseControl *, ParameterBridge *>
	{
		utils::ScopedSpinLock lock(waitLock_);

		if (std::holds_alternative<Interface::BaseControl *>(control))
		{
			auto variant = decltype(control){ std::in_place_index<0>, parameterLink_.UIControl };
			parameterLink_.UIControl = std::get<0>(control);
			return variant;
		}
		else
		{
			auto variant = decltype(control){ std::in_place_index<1>, parameterLink_.hostControl };
			parameterLink_.hostControl = std::get<1>(control);
			return variant;
		}
	}

	void ParameterValue::addModulator(std::weak_ptr<ParameterModulator> modulator, i64 index)
	{
		COMPLEX_ASSERT(!modulator.expired() && "You're trying to add an empty modulator to parameter");

		utils::ScopedSpinLock lock(waitLock_);

		if (index < 0) parameterLink_.modulators.emplace_back(std::move(modulator));
		else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, std::move(modulator));

		isDirty_ = true;
	}

	auto ParameterValue::updateModulator(std::weak_ptr<ParameterModulator> modulator, size_t index) noexcept 
		-> std::weak_ptr<ParameterModulator>
	{
		COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

		utils::ScopedSpinLock lock(waitLock_);

		auto replacedModulator = parameterLink_.modulators[index];
		parameterLink_.modulators[index] = std::move(modulator);

		isDirty_ = true;

		return replacedModulator;
	}

	auto ParameterValue::deleteModulator(size_t index) noexcept 
		-> std::weak_ptr<ParameterModulator>
	{
		COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

		utils::ScopedSpinLock lock(waitLock_);

		auto deletedModulator = parameterLink_.modulators[index];
		parameterLink_.modulators.erase(parameterLink_.modulators.begin() + (std::ptrdiff_t)index);

		isDirty_ = true;

		return deletedModulator;
	}

	void ParameterValue::updateValues(float sampleRate, std::optional<float> value) noexcept
	{
		utils::ScopedSpinLock lock(waitLock_);

		if (value.has_value())
		{
			isDirty_ = isDirty_ || normalisedValue_ != value.value();
			normalisedValue_ = value.value();

			if (parameterLink_.hostControl)
				parameterLink_.hostControl->setValue(normalisedValue_);
			if (parameterLink_.UIControl)
				parameterLink_.UIControl->setValueSafe(normalisedValue_);
		}
		else
		{
			float newNormalisedValue = normalisedValue_;

			// if there's a set hostControl set, then we're automating this parameter
			if (parameterLink_.hostControl)
			{
				auto *control = parameterLink_.hostControl;
				newNormalisedValue = control->getValue();
			}
			else if (parameterLink_.UIControl)
			{
				auto *control = parameterLink_.UIControl;
				newNormalisedValue = (float)control->getValueSafe();
			}

			if (normalisedValue_ != newNormalisedValue)
			{
				normalisedValue_ = newNormalisedValue;
				isDirty_ = true;
			}
		}

		simd_float newModulations = modulations_;
		for (auto &modulator : parameterLink_.modulators)
		{
			// only getting the change from the previous used value
			if (auto modulatorPointer = modulator.lock())
				newModulations += modulatorPointer->getDeltaValue();
		}

		if (isDirty_ || !utils::completelyEqual(modulations_, newModulations))
		{
			modulations_ = newModulations;
			isDirty_ = true;
		}

		if (isDirty_)
		{
			normalisedInternalValue_ = simd_float::clamp(modulations_ + normalisedValue_, 0.0f, 1.0f);
			internalValue_ = scaleValue(normalisedInternalValue_, details_, sampleRate);
		}
		// values can change at any point in time due to changing sample rate
		else if (details_.scale == ParameterScale::Frequency || details_.scale == ParameterScale::SymmetricFrequency)
			internalValue_ = scaleValue(normalisedInternalValue_, details_, sampleRate);

		isDirty_ = false;
	}
}
