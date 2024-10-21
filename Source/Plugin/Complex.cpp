/*
  ==============================================================================

    Complex.cpp
    Created: 23 May 2021 12:20:15am
    Author:  theuser27

  ==============================================================================
*/

#include "Complex.hpp"

#include "Framework/parameter_value.hpp"
#include "Generation/EffectsState.hpp"
#include "Generation/SoundEngine.hpp"
#include "Renderer.hpp"
#include "Interface/LookAndFeel/Miscellaneous.hpp"

namespace Plugin
{
  ComplexPlugin::ComplexPlugin(u32 inSidechains, u32 outSidechains) :
    ProcessorTree{ inSidechains, outSidechains } { }

  void ComplexPlugin::initialise(float sampleRate, u32 samplesPerBlock)
  {
    if (sampleRate != getSampleRate())
      sampleRate_.store(sampleRate, std::memory_order_release);

    if (samplesPerBlock != getSamplesPerBlock())
    {
      samplesPerBlock_.store(samplesPerBlock, std::memory_order_release);
      soundEngine_->resetBuffers();
    }
  }

  u32 ComplexPlugin::getProcessingDelay() const noexcept { return soundEngine_->getProcessingDelay(); }

  void ComplexPlugin::updateParameters(UpdateFlag flag, float sampleRate) noexcept
  { soundEngine_->updateParameters(flag, sampleRate, true); }

  void ComplexPlugin::initialiseModuleTree() noexcept
  {
    // TODO: make the module structure here instead of doing it in the constructors
  }

  void ComplexPlugin::parameterChangeMidi([[maybe_unused]] u64 parentModuleId,
    [[maybe_unused]] std::string_view parameterName, [[maybe_unused]] float value)
  {
    // TODO
  }

  Generation::EffectsState &ComplexPlugin::getEffectsState() noexcept
  {
    return soundEngine_->getEffectsState();
  }

  float ComplexPlugin::getOverlap() noexcept { return soundEngine_->getOverlap(); }

  u32 ComplexPlugin::getFFTSize() noexcept
  {
    return 1 << soundEngine_->getParameter(Framework::Processors::
      SoundEngine::BlockSize::id().value())->getInternalValue<u32>();
  }

  Interface::Renderer &ComplexPlugin::getRenderer()
  {
    if (!rendererInstance_)
    {
      rendererInstance_ = utils::up<Interface::Renderer>::create(*this);
      return *rendererInstance_;
    }

    // setting these once more because i don't know if the message thread 
    // might have been shut down and started up again
    Interface::uiRelated.renderer = rendererInstance_.get();
    Interface::uiRelated.skin = rendererInstance_->getSkin();
    
    return *rendererInstance_;
  }

  size_t ComplexPlugin::getLaneCount() const
  {
    return soundEngine_->getEffectsState().getLaneCount();
  }

  void ComplexPlugin::process(float *const *buffer, u32 numSamples, 
    float sampleRate, u32 numInputs, u32 numOutputs) noexcept
  {
    utils::ScopedLock g{ processingLock_, utils::WaitMechanism::Spin };
    soundEngine_->process(buffer, numSamples, sampleRate, numInputs, numOutputs);
  }

}

