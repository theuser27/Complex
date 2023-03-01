/*
  ==============================================================================

    parameter_value.cpp
    Created: 27 Dec 2022 2:57:25am
    Author:  theuser27

  ==============================================================================
*/

#include "parameter_value.h"

namespace Framework
{
	void ParameterValue::updateValues() noexcept
	{
		utils::ScopedSpinLock lock(waitLock_);

		// if there's a set hostControl set, then we're automating this parameter
		Interface::ParameterUI *control = (parameterLink_.hostControl) ? parameterLink_.hostControl : parameterLink_.UIControl;
		bool isChanged = false;

		if (control)
		{
			float newNormalisedValue = control->getValueInternal();
			if (normalisedValue_ != newNormalisedValue)
			{
				normalisedValue_ = newNormalisedValue;
				isChanged = true;
			}
		}

		simd_float newModulations = modulations_;
		for (auto &modulator : parameterLink_.modulators)
		{
			// only getting the change from the previous used value
			if (auto modulatorPointer = modulator.lock())
				newModulations += modulatorPointer->getDeltaValue();
		}

		if (isChanged || !utils::completelyEqual(modulations_, newModulations))
		{
			modulations_ = newModulations;
			isChanged = true;
		}

		if (isChanged)
		{
			normalisedInternalValue_ = simd_float::clamp(modulations_ + normalisedValue_, 0.0f, 1.0f);
			internalValue_ = utils::scaleValue(normalisedInternalValue_, details_);
		}
	}
}
