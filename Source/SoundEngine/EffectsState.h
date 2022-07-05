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
			sourceBuffer.reserve(kNumChannels, kMaxFFTBufferLength / 2);
			workBuffer.reserve(kNumChannels, kMaxFFTBufferLength / 2);

		}

		// is the work buffer in cartesian or polar representation
		bool isCartesian = true;
		// flag for keeping track where the latest data is
		bool dataIsInWork = false;

		// intermediate buffer used for dry/wet mixing per effect
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer;
		// main buffer for processing
		Framework::SimdBuffer<std::complex<float>, simd_float> workBuffer;
	};

	struct EffectsChain
	{
		EffectsChain()
		{
			// TODO: get rid of temporary
			effectOrder.reserve(kNumFx);
			effectOrder.emplace_back(ModuleTypes::Utility);
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
			: effectOrder(std::move(other.effectOrder)), chainData(std::move(other.chainData)) { }

		EffectsChain &operator=(EffectsChain &&other) noexcept
		{
			if (this != &other)
			{
				effectOrder = std::move(other.effectOrder);
				chainData = std::move(other.chainData);
			}

			return *this;
		}

		void initialise()
		{
			isFinished.store(true, std::memory_order_release);
			isStopped.store(false, std::memory_order_release);
			currentEffectIndex.store(0, std::memory_order_release);
			inputIndex = 0;
			outputIndex = 0;
			isEnabled = true;
		}
		void getParameters();
		void setParameters();

		std::weak_ptr<EffectsChainData> getChainData() const noexcept
		{ return chainData; }


		std::vector<EffectModule> effectOrder;
		// TODO: make it so a separate thread allocates memory for the buffers
		std::shared_ptr<EffectsChainData> chainData;

		// modulatable parameters
		u32 inputIndex = 0, outputIndex = 0;
		bool isEnabled = true;

		simd_float inputVolume, outputVolume;
		std::atomic<u32> currentEffectIndex = 0;
		// a way for threads to check whether other threads have 
		// either stopped temporarily or finished all processing
		// isFinished also doubles as a flag for the thread itself whether to run or not
		std::atomic<bool> isStopped = false;
		std::atomic<bool> isFinished = true;

		bool isUsed = true;
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
			// size is half the max because a single SIMD package stores both real and imaginary parts
			sourceBuffer_.reserve(kNumTotalChannels, kMaxFFTBufferLength / 2);
			
			// assigning the only chain the main input/output
			addChain(0, 0);

			chainThreads_.reserve(kMaxNumChains);
			chainThreads_.emplace_back(processIndividualChains, std::cref(*this), 0);
		}
		~EffectsState()
		{
			for (size_t i = 0; i < chainThreads_.size(); i++)
				chainThreads_[i].request_stop();
		}

		EffectsState(const EffectsState &other, bool copySourceData = false) : chains_(other.chains_), 
			sourceBuffer_(other.sourceBuffer_, copySourceData), FFTSize_(other.FFTSize_),
			sampleRate_(other.sampleRate_) { }

		EffectsState &operator=(const EffectsState &other) = delete;
		EffectsState(EffectsState &&other) = delete;
		EffectsState &operator=(EffectsState &&other) = delete;

		void writeInputData(const AudioBuffer<float> &inputBuffer);
		void distributeData();
		void processChains();
		void sumChains();
		void writeOutputData(AudioBuffer<float> &outputBuffer);

		// the following functions need to be called outside of processing time

		// adding chains is as simple as finding the first place not used in the vector
		// (could be a previous, currently not used, chain) and placing/reusing an object
		// returns index of the chain inside the vector
		u32 addChain(u32 inputIndex, u32 outputIndex)
		{
			for (size_t i = 0; i < chains_.size(); i++)
			{
				if (!chains_[i].isUsed)
				{
					chains_[i].isUsed = true;
					chains_[i].initialise();
					chains_[i].inputIndex = inputIndex;
					chains_[i].outputIndex = outputIndex;
					return i;
				}
			}

			chains_.emplace_back();
			chains_.back().inputIndex = inputIndex;
			chains_.back().outputIndex = outputIndex;
			return chains_.size() - 1;
		}

		// deleting a chain is effectively a no-op
		void deleteChain(u32 chainIndex)
		{
			chainThreads_[chainIndex].request_stop();
			chains_[chainIndex].isUsed = false;
		}

		u32 copyChain(u32 chainIndex)
		{
			for (size_t i = 0; i < chains_.size(); i++)
			{
				if (!chains_[i].isUsed)
				{
					chains_[i].isUsed = true;
					chains_[i].initialise();
					chains_[i].inputIndex = chains_[chainIndex].inputIndex;
					chains_[i].outputIndex = chains_[chainIndex].outputIndex;
					return i;
				}
			}

			chains_.emplace_back();
			chains_.back().inputIndex = chains_[chainIndex].inputIndex;
			chains_.back().outputIndex = chains_[chainIndex].outputIndex;
			return chains_.size() - 1;
		}

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
				// if the input is not other chains' output and the chain is enabled
				if (((chains_[i].inputIndex & kChainInputMask) == 0) && chains_[i].isEnabled)
					usedInputs_[chains_[i].inputIndex] = true;
			}

			std::array<bool, kNumTotalChannels> usedInputChannels{};
			for (size_t i = 0; i < usedInputChannels.size(); i++)
				usedInputChannels[i] = usedInputs_[i / kComplexSimdRatio];

			return usedInputChannels;
		}

		perf_inline auto getUsedOutputChannels() noexcept
		{
			for (size_t i = 0; i < chains_.size(); i++)
			{
				// if the output is not defaulted and the chain is enabled
				if ((chains_[i].outputIndex != kDefaultOutput) && chains_[i].isEnabled)
					usedOutputs_[chains_[i].outputIndex] = true;
			}

			std::array<bool, kNumTotalChannels> usedOutputChannels{};
			for (size_t i = 0; i < usedOutputChannels.size(); i++)
				usedOutputChannels[i] = usedOutputs_[i / kComplexSimdRatio];

			return usedOutputChannels;
		}

		strict_inline u32 getFFTSize() const noexcept { return FFTSize_; }
		strict_inline double getSampleRate() const noexcept { return sampleRate_; }
		strict_inline bool getChainEnable(u32 numChain) const noexcept { return chains_[numChain].isEnabled; }
		strict_inline u32 getChainInputIndex(u32 numChain) const noexcept { return chains_[numChain].inputIndex; }
		strict_inline u32 getChainOutputIndex(u32 numChain) const noexcept { return chains_[numChain].outputIndex; }

		strict_inline void setFFTSize(u32 newFFTSize) noexcept { FFTSize_ = newFFTSize; }
		strict_inline void setSampleRate(double newSampleRate) noexcept { sampleRate_ = newSampleRate; }
		strict_inline void setChainEnable(bool newIsEnabled, u32 numChain) noexcept { chains_[numChain].isEnabled = newIsEnabled; }
		strict_inline void setChainInputIndex(u32 newInputIndex, u32 numChain) noexcept { chains_[numChain].inputIndex = newInputIndex; }
		strict_inline void setChainOutputIndex(u32 newOutputIndex, u32 numChain) noexcept { chains_[numChain].outputIndex = newOutputIndex; }

	private:
		static void processIndividualChains(std::stop_token stoken, const EffectsState &state, u32 chainIndex);

		// declared mutable for the chains to process
		mutable std::vector<EffectsChain> chains_;
		// main buffer to store every FFT-ed input
		Framework::SimdBuffer<std::complex<float>, simd_float> sourceBuffer_;
		// current FFT process size
		u32 FFTSize_ = 1 << kDefaultFFTOrder;
		double sampleRate_ = kDefaultSampleRate;

		// if an input isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedInputs_{};
		// if an input isn't used there's no need to process it at all
		std::array<bool, kNumInputsOutputs> usedOutputs_{};

		std::vector<std::jthread> chainThreads_;

		static constexpr u32 kChainInputMask = kSignMask;
		static constexpr u32 kDefaultOutput = -1;
	};
}