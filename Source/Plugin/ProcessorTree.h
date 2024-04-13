/*
	==============================================================================

		ProcessorTree.h
		Created: 31 May 2023 4:07:02am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <any>
#include <memory>
#include "Framework/constants.h"
#include "Framework/sync_primitives.h"
#include "Framework/vector_map.h"

namespace juce
{
	class UndoManager;
}

namespace Generation
{
	class BaseProcessor;
}

namespace Framework
{
	class ParameterValue;
	class ParameterModulator;
	class ParameterBridge;
	class WaitingUpdate;
}

namespace Plugin
{
	// global state for keeping track of all parameters lists through global module number
	class ProcessorTree
	{
	protected:
		static constexpr size_t expandAmount = 2;
		static constexpr float expandThreshold = 0.75f;

		ProcessorTree();
		virtual ~ProcessorTree();

	public:
		// 0 is reserved to mean "uninitialised" and
		// 1 is reserved for the processorTree itself
		static constexpr u64 processorTreeId = 1;

		ProcessorTree(const ProcessorTree &) = delete;
		ProcessorTree(ProcessorTree &&) = delete;
		ProcessorTree &operator=(const ProcessorTree &other) = delete;
		ProcessorTree &operator=(ProcessorTree &&other) = delete;

		// return type is nlohmann::json but the header is too big to just put here
		[[nodiscard]] std::any serialiseToJson() const;
		void deserialiseFromJson(std::any jsonData);
		
		// do not call this function manually, it's always called when you instantiate a BaseProcessor derived type
		// may block and allocate if expandThreshold has been reached
		u64 getId(Generation::BaseProcessor *newProcessor) noexcept;
		std::unique_ptr<Generation::BaseProcessor> deleteProcessor(u64 processorId) noexcept;

		// remember to use a scoped lock before calling these 2 methods
		Generation::BaseProcessor *getProcessor(u64 processorId) const noexcept;
		// finds a parameter from a specified module with a specified name
		Framework::ParameterValue *getProcessorParameter(u64 parentProcessorId, std::string_view parameterName) const noexcept;
		Generation::BaseProcessor *copyProcessor(Generation::BaseProcessor *processor);

		UpdateFlag getUpdateFlag() const noexcept { return updateFlag_.load(std::memory_order_acquire); }
		// only the audio thread changes the updateFlag
		// so we need acq_rel in order for it to see any changes made by the GUI thread
		// because it's only done twice per run i opted for max security with seq_cst just in case
		void setUpdateFlag(UpdateFlag newFlag) noexcept { updateFlag_.store(newFlag, std::memory_order_seq_cst); }

		auto getSampleRate() const noexcept { return sampleRate_.load(std::memory_order_acquire); }
		auto getSamplesPerBlock() const noexcept { return samplesPerBlock_.load(std::memory_order_acquire); }

		strict_inline auto &getParameterBridges() noexcept { return parameterBridges_; }
		strict_inline auto &getParameterModulators() noexcept { return parameterModulators_; }

		void pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction = true);
		void undo();
		void redo();

		// quick and dirty spinlock to ensure lambdas are executed outside of an audio callback
		auto executeOutsideProcessing(auto &function)
		{
			// check if we're in the middle of an audio callback
			while (updateFlag_.load(std::memory_order_relaxed) != UpdateFlag::AfterProcess) { utils::millisleep(); }

			utils::ScopedLock g{ processingLock_, utils::WaitMechanism::Spin };
			return function();
		}

		bool isBeingDestroyed() const noexcept { return isBeingDestroyed_.load(std::memory_order_acquire); }

	protected:
		// all plugin undo steps are stored here
		std::unique_ptr<juce::UndoManager> undoManager_;
		// the processor tree is stored in a flattened map
		Framework::VectorMap<u64, std::unique_ptr<Generation::BaseProcessor>> allProcessors_;
		// outward facing parameters, which can be mapped to in-plugin parameters
		std::vector<Framework::ParameterBridge *> parameterBridges_{};
		// modulators inside the plugin
		std::vector<Framework::ParameterModulator *> parameterModulators_{};
		// used to give out non-repeating ids for all PluginModules
		std::atomic<u64> processorIdCounter_ = processorTreeId + 1;
		// used for checking whether it's ok to update parameters/plugin structure
		std::atomic<UpdateFlag> updateFlag_ = UpdateFlag::AfterProcess;
		// if any updates are supposed to happen to the processing tree/undoManager
		// the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
		mutable std::atomic<bool> processingLock_ = false;
		std::atomic<bool> isBeingDestroyed_ = false;

		// the reason for these being atomics is because idk which thread they might be updated on
		std::atomic<u32> samplesPerBlock_{};
		std::atomic<float> sampleRate_ = kDefaultSampleRate;
	};
}
