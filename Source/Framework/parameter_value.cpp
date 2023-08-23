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

		isDirty_ = false;
	}
}
