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
		static void processEffects(EffectsChain &object, u32 FFTSize, float sampleRate)
		{

		}

		std::weak_ptr<EffectsChainData> getChainData() noexcept
		{ return chainData; }


		std::vector<EffectModule> effectOrder;
		// TODO: make it so a separate thread allocates memory for the buffers
		std::shared_ptr<EffectsChainData> chainData;

		// volume of the incoming dry signal
		simd_float inputVolume;
		// volume of the output wet signal
		simd_float outputVolume;

		// chain on/off switch
		bool isEnabled = false;

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
			
			// assigning first input to first chain
			chainInputs_[0] = 0;
			// assigning the first chain to the first output
			chainOutputs_[0] = 0;
			chains_[0].isEnabled = true;

			for (size_t i = 1; i < chainOutputs_.size(); i++)
				chainOutputs_[i] = kDefaultOutput;
		}
		~EffectsState() = default;

		EffectsState(const EffectsState &other, bool copySourceData = false) : chains_(other.chains_), 
			sourceBuffer_(other.sourceBuffer_, copySourceData), FFTSize_(other.FFTSize_),
			sampleRate_(other.sampleRate_), chainInputs_(other.chainInputs_), 
			chainOutputs_(other.chainOutputs_) { }

		EffectsState &operator=(const EffectsState &other) = delete;
		EffectsState(EffectsState &&other) = delete;
		EffectsState &operator=(EffectsState &&other) = delete;

		void writeInputData(const AudioBuffer<float> &inputBuffer);
		void distributeData();
		void processChains();
		static void processIndividualChains(const EffectsState &state, u32 chainIndex);
		void sumChains();
		void writeOutputData(AudioBuffer<float> &outputBuffer);

		void addChain();
		void deleteChain(u32 chainIndex);
		void moveChain(u32 chainIndex, u32 newChainIndex);
		void copyChain(u32 chainIndex, u32 copyChainIndex);

		void addEffect(u32 chainIndex, u32 effectIndex, ModuleTypes type = ModuleTypes::Utility);
		void deleteEffect(u32 chainIndex, u32 effectIndex);
		void moveEffect(u32 currentChainIndex, u32 currentEffectIndex, u32 newChainIndex, u32 newEffectIndex);
		void copyEffect(u32 chainIndex, u32 effectIndex, u32 copyChainIndex, u32 copyEffectIndex);

		// TODO: every parameter that's modulated should be mapped to particular
		//				every parameter that's mapped out should retain its 
		void mapOutParameter();


		strict_inline u32 getNumChains() const noexcept { return chains_.size(); }
		perf_inline auto getUsedInputChannels() noexcept
		{
			for (size_t i = 0; i < chains_.size(); i++)
			{
				// if the input is not other chains' output and the chain itself is enabled/outputs
				if (((chainInputs_[i] & kChainInputMask) == 0) && chains_[i].isEnabled)
					usedInputs_[chainInputs_[i]] = true;
			}

			std::array<bool, kNumTotalChannels> usedInputChannels{};
			for (size_t i = 0; i < usedInputChannels.size(); i++)
				usedInputChannels[i] = usedInputs_[i / kNumChannels];

			return usedInputChannels;
		}

		perf_inline auto getUsedOutputChannels() noexcept
		{
			for (size_t i = 0; i < chains_.size(); i++)
				if (chains_[i].isEnabled && (chainOutputs_[i] != kDefaultOutput))
					usedOutputs_[chainOutputs_[i]] = true;
				

			std::array<bool, kNumTotalChannels> usedOutputChannels{};
			for (size_t i = 0; i < usedOutputChannels.size(); i++)
				usedOutputChannels[i] = usedInputs_[i / kNumChannels];

			return usedOutputChannels;
		}

		strict_inline void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }
		strict_inline void setSampleRate(double newSampleRate) noexcept { sampleRate_ = newSampleRate; }

		strict_inline void setChainEnable(bool newIsEnabled, u32 numChain) { chains_[numChain].isEnabled = newIsEnabled; }

	private:
		// TODO: parallelalise effects chains with threads
		std::vector<EffectsChain> chains_;
		// main buffer to store every FFT-ed input
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer_;
		// current FFT process size
		u32 FFTSize_ = 1 << kDefaultFFTOrder;
		double sampleRate_ = kDefaultSampleRate;

		// input routing for the chains
		std::array<u32, kMaxNumChains> chainInputs_{};
		// if an input isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedInputs_{};
		// output routing for the chains
		std::array<u32, kMaxNumChains> chainOutputs_{};
		// if an input isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedOutputs_{};

		// a way for threads to check whether other threads have finished
		mutable std::array<std::atomic<bool>, kMaxNumChains> AreChainsFinished_{};

		static constexpr u32 kChainInputMask = kSignMask;
		static constexpr u32 kDefaultOutput = -1;
	};
}