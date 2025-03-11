/*
  ==============================================================================

    EffectModules.hpp
    Created: 27 Jul 2021 12:30:19am
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/constants.hpp"
#include "Framework/simd_buffer.hpp"
#include "BaseProcessor.hpp"
#include "Plugin/ProcessorTree.hpp"
#include "Framework/parameters.hpp"

namespace Generation
{
  // base class for the actual effects
  class BaseEffect : public BaseProcessor
  {
  public:
    BaseEffect(Plugin::ProcessorTree *processorTree, utils::string_view effectType) :
      BaseProcessor{ processorTree, effectType } { }

    BaseEffect(const BaseEffect &other) noexcept = default;
    BaseEffect &operator=(const BaseEffect &other) noexcept = default;

    BaseEffect(BaseEffect &&other) noexcept = default;
    BaseEffect &operator=(BaseEffect &&other) noexcept = default;

    void initialiseParameters() override;
    void deserialiseFromJson(void *jsonData) override;

    virtual void run([[maybe_unused]] Framework::ComplexDataSource &source,
      [[maybe_unused]] Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination,
      [[maybe_unused]] u32 binCount, [[maybe_unused]] float sampleRate) noexcept { }

  protected:
    BaseProcessor *createCopy() const noexcept override
    { COMPLEX_ASSERT_FALSE("This type cannot be copied directly, you need a derived type"); return nullptr; }

    enum class BoundRepresentation : u32 { Normalised, Frequency, BinIndex };

    // first - low shifted boundary, second - high shifted boundary
    auto getShiftedBounds(BoundRepresentation representation, 
      float sampleRate, u32 binCount) const noexcept -> std::pair<simd_float, simd_float>;

    static strict_inline simd_mask vector_call isOutsideBounds(simd_int positionIndices,
      simd_int lowBoundIndices, simd_int highBoundIndices, simd_mask isHighAboveLow)
    {
      simd_mask belowLowBounds = simd_int::lessThanSigned(positionIndices, lowBoundIndices);
      simd_mask aboveHighBounds = simd_int::greaterThanSigned(positionIndices, highBoundIndices);

      // 1st case examples: |   *  [    ]     | or
      //                    |      [    ]  *  |
      // 2nd case example:  |     ]   *  [    |
      return (belowLowBounds | aboveHighBounds) & ((belowLowBounds & aboveHighBounds) ^ isHighAboveLow);
    }

    static strict_inline simd_mask vector_call isOutsideBounds(
      simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
    {
      return isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices,
        simd_int::greaterThanOrEqualSigned(highBoundIndices, lowBoundIndices));
    }

    static strict_inline simd_mask vector_call isInsideBounds(simd_int positionIndices,
       simd_int lowBoundIndices, simd_int highBoundIndices, simd_mask isHighAboveLow)
    { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices, isHighAboveLow); }

    static strict_inline simd_mask vector_call isInsideBounds(
      simd_int positionIndices, simd_int lowBoundIndices, simd_int highBoundIndices)
    { return ~isOutsideBounds(positionIndices, lowBoundIndices, highBoundIndices); }

    // returns starting point, distance to end of processed/unprocessed range, and if the range is stereo
    static auto vector_call minimiseRange(simd_int lowIndices, simd_int highIndices,
      u32 binCount, bool isProcessedRange) -> std::tuple<u32, u32, bool>;

    static void circularLoop(const auto &lambda, u32 start, u32 length, u32 binCount)
    {
      for (u32 i = 0; i < length - 1; i++)
        lambda((start + i) & (binCount - 2));

      // since we're masking to power-of-2 we skip nyquist
      // process it if passed, else continue onwards
      if ((start + length) > (binCount - 1))
        lambda(binCount - 1);
      else
        lambda(start + length);
    }

    static void copyUnprocessedData(Framework::SimdBufferView<Framework::complex<float>, simd_float> source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, simd_int lowBoundIndices,
      simd_int highBoundIndices, u32 binCount) noexcept;

    utils::span<const utils::string_view> parameters_{};

    template<typename Type>
    friend void fillAndSetParameters(BaseEffect &effect);

    //// Parameters
    //
    // 1. internal fx type (mono) - [0, n]
    // 2. and 3. normalised frequency boundaries of processed region (stereo) - [0.0f, 1.0f]
    // 4. shifting of the freq boundaries (stereo) - [-1.0f; 1.0f]
  };

  class UtilityEffect final : public BaseEffect
  {
  public:
    UtilityEffect(Plugin::ProcessorTree *processorTree);

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) noexcept override
    {
      BaseEffect::run(source, destination, binCount, sampleRate);
    }

    UtilityEffect *createCopy() const noexcept override { return processorTree_->createProcessor<UtilityEffect>(*this); }
    void initialiseParameters() override;

  private:
    //// Parameters
    // 
    // 5. channel pan (stereo) - [-1.0f, 1.0f]
    // 6. flip phases (stereo) - [0, 1]
    // 7. reverse spectrum bins (stereo) - [0, 1]
    // 
    // freeze input
    // record and play back input

    // TODO:
    // idea: mix 2 input signals (left and right/right and left channels)
    // idea: flip the phases, panning
  };

  class FilterEffect final : public BaseEffect
  {
  public:
    FilterEffect(Plugin::ProcessorTree *processorTree);

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) noexcept override;

    FilterEffect *createCopy() const noexcept override { return processorTree_->createProcessor<FilterEffect>(*this); }
    void initialiseParameters() override;

  private:
    void runNormal(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

    void runPhase(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

    //// Modes
    // 
    // Normal - normal filtering
    // 5. gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
    //    lowers the loudness at/around the cutoff point for negative/positive values
    //    *parameter values are interpreted linearly, so the control needs to have an exponential slope
    // 
    // 6. cutoff (stereo) - range[0.0f, 1.0f]; controls where the filtering starts
    //    at 0.0f/1.0f it's at the low/high boundary
    //    *parameter values are interpreted linearly, so the control needs to have an exponential slope
    // 
    // 7. slope (stereo) - range[-1.0f, 1.0f]; controls the slope transition
    //    at 0.0f it stretches from the cutoff to the frequency boundaries
    //    at 1.0f only the center bin is left unaffected
    // 
    // 
    // Threshold - frequency gating
    // 5. threshold (stereo) - range[0%, 100%]; threshold is relative to the loudest bin in the spectrum
    // 
    // 6. gain (stereo) - range[-$ db, +$ db] (symmetric loudness);
    //    positive/negative values lower the loudness of the bins below/above the threshold
    // 
    // 7. tilt (stereo) - range[-100%, +100%]; tilt relative to the loudest bin in the spectrum
    //    at min/max values the left/right-most part of the masked spectrum will have no threshold
    // 
    // 8. delta - instead of the absolute loudness it uses the averaged loudness change from the 2 neighbouring bins
    // 
    // Tilt filter
    // Regular - triangles, squares, saws, pointy, sweep and custom, razor comb peak
    // Regular key tracked - regular filtering based on fundamental frequency (like dtblkfx autoharm)
    // 
    // TODO: write a constexpr generator for all of the weird mask types (triangle, saw, square, etc)
  };



  class DynamicsEffect final : public BaseEffect
  {
  public:
    DynamicsEffect(Plugin::ProcessorTree *processorTree);

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) noexcept override;

    DynamicsEffect *createCopy() const noexcept override { return processorTree_->createProcessor<DynamicsEffect>(*this); }
    void initialiseParameters() override;

  private:
    void runContrast(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) const noexcept;

    void runClip(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, u32 binCount, float sampleRate) const noexcept;

    static simd_float vector_call matchPower(simd_float target, simd_float current) noexcept;


    //// Dtblkfx Contrast
    /// Parameters
    //
    // 5. contrast (stereo) - range[-1.0f, 1.0f]; controls the relative loudness of the bins
    //
    /// Constants
    static constexpr float kContrastMaxPositiveValue = 4.0f;
    static constexpr float kContrastMaxNegativeValue = -0.5f;
    //
    // 
    //// Compressor
    //
    // Depth, Threshold, Ratio
    //

    // specops noise filter/focus, thinner
    // spectral compander, gate (threshold), clipping
  };



  class PhaseEffect final : public BaseEffect
  {
  public:
    PhaseEffect(Plugin::ProcessorTree *processorTree);

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) noexcept override;

    PhaseEffect *createCopy() const noexcept override { return processorTree_->createProcessor<PhaseEffect>(*this); }
    void initialiseParameters() override;

  private:
    void runShift(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) const noexcept;

    // phase zeroer, (constrained) phase randomiser (smear), channel phase shifter (pha-979), etc
    // phase filter - filtering based on phase
    // mfreeformphase impl
  };



  class PitchEffect final : public BaseEffect
  {
  public:
    PitchEffect(Plugin::ProcessorTree *processorTree);

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) noexcept override;

    PitchEffect *createCopy() const noexcept override { return processorTree_->createProcessor<PitchEffect>(*this); }
    void initialiseParameters() override;

  private:
    void runResample(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination, 
      u32 binCount, float sampleRate) const noexcept;
    void runConstShift(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination,
      u32 binCount, float sampleRate) const noexcept;

    // TODO: shift, harmonic shift, harmonic repitch
  };



  class StretchEffect final : public BaseEffect
  {
  public:
    StretchEffect(Plugin::ProcessorTree *processorTree);

    StretchEffect *createCopy() const noexcept override { return processorTree_->createProcessor<StretchEffect>(*this); }
    void initialiseParameters() override;

    // specops geometry
  };



  class WarpEffect final : public BaseEffect
  {
  public:
    WarpEffect(Plugin::ProcessorTree *processorTree);

    WarpEffect *createCopy() const noexcept override { return processorTree_->createProcessor<WarpEffect>(*this); }
    void initialiseParameters() override;

    // vocode, harmonic match, cross/warp mix
  };



  class DestroyEffect final : public BaseEffect
  {
  public:
    DestroyEffect(Plugin::ProcessorTree *processorTree);

    DestroyEffect *createCopy() const noexcept override { return processorTree_->createProcessor<DestroyEffect>(*this); }
    void initialiseParameters() override;

    void run(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination,
      u32 binCount, float sampleRate) noexcept override;

  private:
    void runReinterpret(Framework::ComplexDataSource &source,
      Framework::SimdBuffer<Framework::complex<float>, simd_float> &destination,
      u32 binCount, float sampleRate) const noexcept;

    // resize, specops effects category
    // randomising bin positions
    // bin sorting - by amplitude, phase
    // TODO: freezer and glitcher classes
  };


  // class for the whole FX unit
  class EffectModule final : public BaseProcessor
  {
  public:
    EffectModule(Plugin::ProcessorTree *processorTree) noexcept;
    EffectModule(const EffectModule &) = default;
    ~EffectModule() override;

    void deserialiseFromJson(void *jsonData) override;

    void processEffect(Framework::ComplexDataSource &source, u32 binCount, float sampleRate) noexcept;

    // Inherited via BaseProcessor
    // this method exists only to accomodate loading from save files
    void insertSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true) noexcept override;
    BaseProcessor &updateSubProcessor(usize index, BaseProcessor &newSubProcessor, bool callListeners = true) noexcept override;
    EffectModule *createCopy() const override { return processorTree_->createProcessor<EffectModule>(*this); }
    void initialiseParameters() override
    {
      createProcessorParameters(Framework::Processors::EffectModule::
        enum_ids_filter<Framework::kGetParameterPredicate, true>());
    }

    BaseEffect *getEffect() const noexcept { return utils::as<BaseEffect>(subProcessors_[0]); }
    BaseEffect *changeEffect(utils::string_view effectType);
  private:
    static constexpr auto effectCount = Framework::Processors::BaseEffect::enum_count_filter(Framework::kGetActiveEffectPredicate);
    Framework::SimdBuffer<Framework::complex<float>, simd_float> buffer_{};
    utils::array<BaseEffect *, effectCount> effects_{};
  };
}
