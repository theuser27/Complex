/*
	==============================================================================

		ProcessorTree.h
		Created: 31 May 2023 4:07:02am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Generation/BaseProcessor.h"

namespace Framework
{
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

		ProcessorTree() = default;
		virtual ~ProcessorTree();

	public:
		ProcessorTree(const ProcessorTree &) = delete;
		ProcessorTree(ProcessorTree &&) = delete;
		ProcessorTree &operator=(const ProcessorTree &other) = delete;
		ProcessorTree &operator=(ProcessorTree &&other) = delete;

		// do not call this function manually, it's always called when you instantiate a BaseProcessor derived type
		// may block and allocate if expandThreshold has been reached
		u64 getId(Generation::BaseProcessor *newModulePointer, bool isRootModule = false) noexcept;
		std::unique_ptr<Generation::BaseProcessor> deleteProcessor(u64 moduleId) noexcept;

		// remember to use a scoped lock before calling these 2 methods
		Generation::BaseProcessor *getProcessor(u64 moduleId) const noexcept;
		// finds a parameter from a specified module with a specified name
		Framework::ParameterValue *getProcessorParameter(u64 parentModuleId, std::string_view parameter) const noexcept;
		Framework::ParameterValue *getProcessorParameter(u64 parentModuleId, nested_enum::NestedEnum auto parameterEnum) const noexcept
		{
			auto *processorPointer = getProcessor(parentModuleId);
			if (!processorPointer)
				return nullptr;

			return processorPointer->getParameter(parameterEnum);
		}

		Framework::UpdateFlag getUpdateFlag() const noexcept { return updateFlag_.load(std::memory_order_acquire); }
		// only the audio thread changes the updateFlag
		// so we need acq_rel in order for it to see any changes made by the GUI thread
		// because it's only done twice per run i opted for max security with seq_cst just in case
		void setUpdateFlag(Framework::UpdateFlag newFlag) noexcept { updateFlag_.store(newFlag, std::memory_order_seq_cst); }

		auto getSampleRate() const noexcept { return sampleRate_.load(std::memory_order_acquire); }
		auto getSamplesPerBlock() const noexcept { return samplesPerBlock_.load(std::memory_order_acquire); }

		auto copyProcessor(Generation::BaseProcessor *processor)
		{
			auto copyProcessor = [processor]() { return processor->createCopy({}); };
			return executeOutsideProcessing(copyProcessor);
		}

		void pushUndo(Framework::WaitingUpdate *action, bool isNewTransaction = true);
		void undo() { undoManager_.undo(); }
		void redo() { undoManager_.redo(); }

		// quick and dirty spinlock to ensure lambdas are executed outside of an audio callback
		auto executeOutsideProcessing(auto &function)
		{
			// check if we're in the middle of an audio callback
			while (getUpdateFlag() != Framework::UpdateFlag::AfterProcess) { utils::wait(); }

			utils::ScopedSpinLock g(waitLock_);
			return function();
		}

		bool isBeingDestroyed() const noexcept { return isBeingDestroyed_; }

	protected:
		// all plugin undo steps are stored here
		UndoManager undoManager_{ 0, 100 };
		// the processor tree is stored in a flattened map
		Framework::VectorMap<u64, std::unique_ptr<Generation::BaseProcessor>> allProcessors_{ 64 };
		// used to give out non-repeating ids for all PluginModules
		std::atomic<u64> processorIdCounter_ = 0;
		// used for checking whether it's ok to update parameters/plugin structure
		std::atomic<Framework::UpdateFlag> updateFlag_ = Framework::UpdateFlag::AfterProcess;
		// if any updates are supposed to happen to the processing tree/undoManager
		// the thread needs to acquire this lock after checking that the updateFlag is set to AfterProcess
		mutable std::atomic<bool> waitLock_ = false;
		bool isBeingDestroyed_ = false;

		// the reason for these being atomics is because idk which thread they might be updated on
		std::atomic<u32> samplesPerBlock_{};
		std::atomic<float> sampleRate_ = kDefaultSampleRate;
	};
}
