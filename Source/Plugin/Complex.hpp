/*
  ==============================================================================

    Complex.hpp
    Created: 23 May 2021 12:20:15am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/utils.hpp"
#include "ProcessorTree.hpp"

namespace Generation
{
  class SoundEngine;
  class EffectsState;
}

namespace Interface
{
  class Renderer;
}

namespace Plugin
{
  class ComplexPlugin : public ProcessorTree
  {
  public:
    ComplexPlugin(u32 inSidechains, u32 outSidechains);

    void initialise(float sampleRate, u32 samplesPerBlock);
    void process(float *const *buffer, u32 numSamples, 
      float sampleRate, u32 numInputs, u32 numOutputs) noexcept;

    u32 getProcessingDelay() const noexcept;
    
    void updateParameters(UpdateFlag flag, float sampleRate) noexcept;
    void initialiseModuleTree() noexcept;

    virtual void parameterChangeMidi(u64 parentModuleId, std::string_view parameterName, float value);

    Generation::SoundEngine &getSoundEngine() noexcept { return *soundEngine_; }
    Generation::EffectsState &getEffectsState() noexcept;
    float getOverlap() noexcept;
    u32 getFFTSize() noexcept;

    Interface::Renderer &getRenderer();

    bool deserialiseFromJson(void *newSave, void *fallbackSave) override;
    void loadDefaultPreset();
    size_t getLaneCount() const final;

  protected:
    // pointer to the main processing engine
    Generation::SoundEngine *soundEngine_ = nullptr;
    utils::up<Interface::Renderer> rendererInstance_;
    std::atomic<bool> isLoaded_ = false;
  };
}
