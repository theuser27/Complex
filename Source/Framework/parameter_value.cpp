/*
  ==============================================================================

    parameter_value.cpp
    Created: 27 Dec 2022 2:57:25am
    Author:  theuser27

  ==============================================================================
*/

#include "parameter_value.hpp"

#include "simd_utils.hpp"
#include "parameter_bridge.hpp"
#include "Interface/Components/BaseControl.hpp"

namespace Framework
{
  void ParameterValue::updateValue(float sampleRate)
  {
    utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

    bool isDirty = isDirty_;
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
      newNormalisedValue = (float)control->getValueRaw();
    }

    if (normalisedValue_ != newNormalisedValue)
    {
      normalisedValue_ = newNormalisedValue;
      isDirty = true;
    }

    simd_float newModulations = modulations_;
    for (auto &modulator : parameterLink_.modulators)
    {
      // only getting the change from the previous used value
      if (auto modulatorPointer = modulator.lock())
        newModulations += modulatorPointer->getDeltaValue();
    }

    if (isDirty || !utils::completelyEqual(modulations_, newModulations))
    {
      modulations_ = newModulations;
      isDirty = true;
    }

    if (isDirty)
    {
      normalisedInternalValue_ = simd_float::clamp(newModulations + newNormalisedValue, 0.0f, 1.0f);
      internalValue_ = scaleValue(normalisedInternalValue_, details_, sampleRate);
    }

    isDirty_ = false;
  }
}
