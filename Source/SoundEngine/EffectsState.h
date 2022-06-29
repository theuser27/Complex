/*
	==============================================================================

		EffectsState.h
		Created: 2 Oct 2021 8:53:05pm
		Author:  Lenovo

	==============================================================================
*/

#pragma once

#include "EffectModules.h"
#include "./Framework/simd_buffer.h"

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
		}

		// is the work buffer in cartesian or polar representation
		bool isCartesian = true;

		// volume of the output wet signal
		simd_float outputVolume;

		// main buffer for processing
		Framework::SimdBuffer<std::complex<float>, simd_float> workBuffer;

		// intermediate buffer used for dry/wet mixing per effect
		Framework::SimdBuffer<std::complex<float>, simd_float> intermediateBuffer;
	};

	struct EffectsChain
	{
		EffectsChain()
		{
			// TODO: get rid of temporary
			effectOrder.reserve(kNumFx);
			effectOrder.emplace_back();
			chainData = std::make_shared<EffectsChainData>();
		}
		~EffectsChain() = default;

		EffectsChain(const EffectsChain &other) : effectOrder(other.effectOrder), 
			chainData(std::make_shared<EffectsChainData>()) { }

		EffectsChain &operator=(const EffectsChain &other) 
		{ 
			if (this != &other)
			{
				effectOrder = other.effectOrder;
				chainData = std::make_shared<EffectsChainData>();
			}
				
			return *this;
		}

		EffectsChain(EffectsChain &&other) noexcept 
			: effectOrder(other.effectOrder), chainData(other.chainData) { }

		EffectsChain &operator=(EffectsChain &&other) noexcept
		{
			if (this != &other)
			{
				effectOrder = other.effectOrder;
				chainData = other.chainData;
			}

			return *this;
		}

		void getParameters();
		void setParameters();
		void processEffects();

		std::weak_ptr<EffectsChainData> getChainData() noexcept
		{ return chainData; }


		std::vector<EffectModule> effectOrder;
		// TODO: make it so a separate thread allocates memory for the buffers
		std::shared_ptr<EffectsChainData> chainData;

		// TODO: add a class/variable for linking individual chains (i.e in order to use vocoder, warp, etc)
	};

	class EffectsState
	{
	public:
		// data link between modules in different chains
		struct EffectsModuleLink
		{
			bool checkForFeedback();

			std::pair<u32, u32> sourceIndex, destinationIndex;
		};


		EffectsState()
		{
			chains_.reserve(kMaxNumChains);
			chains_.emplace_back();
			// size is half the max because a single SIMD package stores both real and imaginary parts
			sourceBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength / 2);
		}
		~EffectsState() = default;

		EffectsState(const EffectsState &other) : chains_(other.chains_), 
			FFTSize_(other.FFTSize_), sampleRate_(other.sampleRate_) 
		{ 
			sourceBuffer_.reserve(other.sourceBuffer_.getNumChannels(), other.sourceBuffer_.getSize());
		}
		EffectsState &operator=(const EffectsState &other)
		{
			if (this != &other)
			{
				chains_ = other.chains_;
				sourceBuffer_.reserve(other.sourceBuffer_.getNumChannels(), other.sourceBuffer_.getSize());
				FFTSize_ = other.FFTSize_;
				sampleRate_ = other.sampleRate_;
			}
			return *this;
		}
		
		EffectsState(EffectsState &&other) = delete;
		EffectsState &operator=(EffectsState &&other) = delete;

		void writeInputData(AudioBuffer<float> &inputBuffer);
		void distributeData(std::array<u32, kMaxNumChains> &chainInputs);
		void processChains();
		void sumChains(std::array<u32, kMaxNumChains> &chainOutputs);
		void writeOutputData(AudioBuffer<float> &outputBuffer);

		void addEffect(u32 chainIndex, u32 effectIndex, ModuleTypes type = ModuleTypes::Utility);
		void deleteEffect(u32 chainIndex, u32 effectIndex);
		void moveEffect(u32 currentChainIndex, u32 currentEffectIndex, u32 newChainIndex, u32 newEffectIndex);
		void copyEffect(u32 chainIndex, u32 effectIndex, u32 copyChainIndex, u32 copyEffectIndex);

		// TODO: every parameter that's modulated should be mapped to particular
		//				every parameter that's mapped out should retain its 
		void mapOutParameter();


		strict_inline u32 getNumChains() const noexcept { return chains_.size(); }

		strict_inline void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }
		strict_inline void setSampleRate(double newSampleRate) noexcept { sampleRate_ = newSampleRate; }

	private:
		// TODO: parallelalise effects chains with threads
		std::vector<EffectsChain> chains_;
		// main buffer to store every FFT-ed input
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer_;
		// current FFT process size
		u32 FFTSize_ = 1 << kDefaultFFTOrder;
		double sampleRate_ = kDefaultSampleRate;
	};
}