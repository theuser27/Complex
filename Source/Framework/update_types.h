/*
	==============================================================================

		update_types.h
		Created: 16 Nov 2022 2:11:06am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Generation/BaseProcessor.h"
#include "Plugin/ProcessorTree.h"

namespace Framework
{
	// most of the updates need to happen outside of processing, but parts of them could be unbounded in time
	// (i.e. allocations) however those can happen regardless if we're currently processing or not
	// so we can specify *when* we need to wait by injecting a synchronisation function into the update
	class WaitingUpdate : public juce::UndoableAction
	{
	public:
		void setWaitFunction(std::function<utils::ScopedSpinLock()> waitFunction) { waitFunction_ = std::move(waitFunction); }
	protected:
		utils::ScopedSpinLock wait() const { return waitFunction_(); }
		std::function<utils::ScopedSpinLock()> waitFunction_ = []()
		{ 
			COMPLEX_ASSERT_FALSE("Wait function for update hasn't been set");
			std::atomic<bool> temp{};
			return utils::ScopedSpinLock{ temp };
		};
		bool isDone_ = false;
	};
	
	class AddProcessorUpdate : public WaitingUpdate
	{
	public:
		AddProcessorUpdate(Plugin::ProcessorTree *processorTree, u64 destinationProcessorId, 
			size_t destinationSubProcessorIndex, std::string_view newSubProcessorType) noexcept : processorTree_(processorTree),
			newSubProcessorType_(newSubProcessorType), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~AddProcessorUpdate() override
		{
			if (!processorTree_->isBeingDestroyed() && addedProcessor_ && !isDone_)
				processorTree_->deleteProcessor(addedProcessor_->getProcessorId());
		}

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree *processorTree_;

		std::string_view newSubProcessorType_;
		Generation::BaseProcessor *addedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class CopyProcessorUpdate : public WaitingUpdate
	{
	public:
		CopyProcessorUpdate(Plugin::ProcessorTree *processorTree, Generation::BaseProcessor &processorCopy, 
			u64 destinationProcessorId, size_t destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
			processorCopy(processorCopy), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~CopyProcessorUpdate() override
		{
			if (!processorTree_->isBeingDestroyed() && !isDone_)
				processorTree_->deleteProcessor(processorCopy.getProcessorId());
		}

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree *processorTree_;

		Generation::BaseProcessor &processorCopy;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class MoveProcessorUpdate : public WaitingUpdate
	{
	public:
		MoveProcessorUpdate(Plugin::ProcessorTree *processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex, u64 sourceProcessorId, size_t sourceSubProcessorIndex) noexcept :
			processorTree_(processorTree), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex), sourceProcessorId_(sourceProcessorId),
			sourceSubProcessorIndex_(sourceSubProcessorIndex) { }

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree *processorTree_;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
		u64 sourceProcessorId_;
		size_t sourceSubProcessorIndex_;
	};

	class UpdateProcessorUpdate : public WaitingUpdate
	{
	public:
		UpdateProcessorUpdate(Plugin::ProcessorTree *processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex, std::string_view newSubProcessorType) noexcept : processorTree_(processorTree),
			newSubProcessorType_(newSubProcessorType), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~UpdateProcessorUpdate() override
		{
			if (!processorTree_->isBeingDestroyed() && savedProcessor_)
				processorTree_->deleteProcessor(savedProcessor_->getProcessorId());
		}

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree *processorTree_;

		std::string_view newSubProcessorType_;
		Generation::BaseProcessor *savedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class DeleteProcessorUpdate : public WaitingUpdate
	{
	public:
		DeleteProcessorUpdate(Plugin::ProcessorTree *processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
			destinationProcessorId_(destinationProcessorId), destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~DeleteProcessorUpdate() override
		{
			if (!processorTree_->isBeingDestroyed() && deletedProcessor_ && isDone_)
				processorTree_->deleteProcessor(deletedProcessor_->getProcessorId());
		}

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree *processorTree_;

		Generation::BaseProcessor *deletedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class ParameterUpdate : public WaitingUpdate
	{
	public:
		ParameterUpdate(Interface::BaseControl *parameterUI, double oldValue, double newValue) :
			baseParameter_(parameterUI), oldValue_(oldValue), newValue_(newValue) { }

		bool perform() override;
		bool undo() override;

		int getSizeInUnits() override { return sizeof(*this); }

	private:
		Interface::BaseControl *baseParameter_;
		double oldValue_;
		double newValue_;
		bool firstTime_ = true;
	};

	// (eventually) for any kind of changes in the modulator amounts
	// TODO: modulator updates for the undo manager when you get to modulators
	/*struct ModulatorUpdate : public juce::UndoableAction
	{

	};*/
}
