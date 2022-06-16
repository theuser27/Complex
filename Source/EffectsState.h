/*
	==============================================================================

		EffectsState.h
		Created: 2 Oct 2021 8:53:05pm
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "EffectModules.h"
#include "simd_buffer.h"

namespace Generation
{
	struct EffectsChainData
	{
		EffectsChainData()
		{
			// currently buffers will only process a single complex input
			// size is half the max because a single SIMD package stores both real and imaginary parts
			workBuffer.reserve(kNumChannels, kMaxFFTBufferLength / 2);
			intermediateBuffer.reserve(kNumChannels, kMaxFFTBufferLength / 2);
			isCartesian = true;
		}

		// main buffer for processing
		Framework::SimdBuffer<std::complex<float>, simd_float> workBuffer;

		// intermediate buffer used for dry/wet mixing per effect
		Framework::SimdBuffer<std::complex<float>, simd_float> intermediateBuffer;

		// volume of the output wet signal
		simd_float outputVolume;

		// is the work buffer in cartesian or polar representation
		bool isCartesian;
	};

	class EffectsChain
	{
	public:
		EffectsChain()
		{
			// TODO: get rid of temporary
			fxOrder.reserve(kNumFx);
			fxOrder.emplace_back();
			chainData = std::make_shared<EffectsChainData>();
		}
		~EffectsChain() = default;

		EffectsChain(const EffectsChain &other) : fxOrder(other.fxOrder) { }

		void mapOutModuleParameters();
		// TODO: every parameter that's modulated should be mapped to particular
		//				every parameter that's mapped out should retain its 
		void setParameterValues();
		void processEffects();

		std::weak_ptr<EffectsChainData> getChainData() noexcept
		{ return chainData; }

	private:
		std::vector<EffectModule> fxOrder;
		// TODO: make it so a separate thread allocates memory for the buffers
		std::shared_ptr<EffectsChainData> chainData;

		// TODO: add a class/variable for linking individual chains (i.e in order to use vocoder, warp, etc)
	};

	class EffectsState
	{
	public:
		EffectsState()
		{
			chains_.reserve(kMaxNumChains);
			chains_.emplace_back();
			// size is half the max because a single SIMD package stores both real and imaginary parts
			sourceBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength / 2);
		}
		~EffectsState() = default;

		EffectsState(const EffectsState &other) : chains_(other.chains_), sourceBuffer_(other.sourceBuffer_) { }

		void writeInputData(AudioBuffer<float> &inputBuffer);
		void distributeData(std::array<u32, kMaxNumChains> &chainInputs);
		void processChains();
		void sumChains(std::array<u32, kMaxNumChains> &chainOutputs);
		void writeOutputData(AudioBuffer<float> &outputBuffer);

		force_inline u32 getNumChains() const noexcept { return chains_.size(); }

		force_inline void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }
		force_inline void setSampleRate(double newSampleRate) { sampleRate_ = newSampleRate; }

	private:
		// TODO: parallelalise effects chains with threads
		std::vector<EffectsChain> chains_;
		// main buffer to store every FFT-ed input
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer_;
		// current FFT process size
		u32 FFTSize_ = 0;
		double sampleRate_ = kDefaultSampleRate;
	};
}