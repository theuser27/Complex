/*
	==============================================================================

		parameter_value.cpp
		Created: 27 Dec 2022 2:57:25am
		Author:  theuser27

	==============================================================================
*/

#include "parameter_value.h"
#include "parameter_bridge.h"

namespace Framework
{
	void ParameterValue::updateValues(float sampleRate) noexcept
	{
		utils::ScopedSpinLock lock(waitLock_);

		bool isChanged = false;

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
			internalValue_ = scaleValue(normalisedInternalValue_, details_, sampleRate);
		}
	}
}

namespace Interface
{
	void ParameterUI::setValueToHost() noexcept
	{
		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->setValueFromUI((float)getValueSafe());
	}
}
