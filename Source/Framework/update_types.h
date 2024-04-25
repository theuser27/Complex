/*
	==============================================================================

		update_types.h
		Created: 16 Nov 2022 2:11:06am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "utils.h"
#include "sync_primitives.h"
#include "parameters.h"

namespace Plugin
{
	class ProcessorTree;
}

namespace Generation
{
	class BaseProcessor;
}

namespace Interface
{
	class BaseControl;
}

namespace Framework
{
	class ParameterValue;
	class PresetUpdate;

	// most of the updates need to happen outside of processing, but parts of them could be unbounded in time
	// (i.e. allocations) however those can happen regardless if we're currently processing or not
	// so we can specify *when* we need to wait by injecting a synchronisation function into the update
	class WaitingUpdate : public juce::UndoableAction
	{
	public:
		void setWaitFunction(std::function<utils::ScopedLock()> waitFunction) { waitFunction_ = std::move(waitFunction); }
	protected:
		auto wait() const { return waitFunction_(); }
		std::function<utils::ScopedLock()> waitFunction_ = []()
		{ 
			COMPLEX_ASSERT_FALSE("Wait function for update hasn't been set");
			std::atomic<bool> temp{};
			return utils::ScopedLock{ temp, utils::WaitMechanism::Wait };
		};
		bool isDone_ = false;
	};
	
	class AddProcessorUpdate final : public WaitingUpdate
	{
	public:
		AddProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId, 
			size_t destinationSubProcessorIndex, std::string_view newSubProcessorType) noexcept : processorTree_(processorTree),
			newSubProcessorType_(newSubProcessorType), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~AddProcessorUpdate() override;

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree &processorTree_;

		std::string_view newSubProcessorType_;
		Generation::BaseProcessor *addedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;

		friend class PresetUpdate;
	};

	class CopyProcessorUpdate final : public WaitingUpdate
	{
	public:
		CopyProcessorUpdate(Plugin::ProcessorTree &processorTree, Generation::BaseProcessor &processorCopy, 
			u64 destinationProcessorId, size_t destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
			processorCopy(processorCopy), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~CopyProcessorUpdate() override;

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree &processorTree_;

		Generation::BaseProcessor &processorCopy;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class MoveProcessorUpdate final : public WaitingUpdate
	{
	public:
		MoveProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex, u64 sourceProcessorId, size_t sourceSubProcessorIndex) noexcept :
			processorTree_(processorTree), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex), sourceProcessorId_(sourceProcessorId),
			sourceSubProcessorIndex_(sourceSubProcessorIndex) { }

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree &processorTree_;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
		u64 sourceProcessorId_;
		size_t sourceSubProcessorIndex_;
	};

	class UpdateProcessorUpdate final : public WaitingUpdate
	{
	public:
		UpdateProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex, std::string_view newSubProcessorType) noexcept : processorTree_(processorTree),
			newSubProcessorType_(newSubProcessorType), destinationProcessorId_(destinationProcessorId),
			destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~UpdateProcessorUpdate() override;

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree &processorTree_;

		std::string_view newSubProcessorType_;
		Generation::BaseProcessor *savedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;
	};

	class DeleteProcessorUpdate final : public WaitingUpdate
	{
	public:
		DeleteProcessorUpdate(Plugin::ProcessorTree &processorTree, u64 destinationProcessorId,
			size_t destinationSubProcessorIndex) noexcept : processorTree_(processorTree),
			destinationProcessorId_(destinationProcessorId), destinationSubProcessorIndex_(destinationSubProcessorIndex) { }

		~DeleteProcessorUpdate() override;

		bool perform() override;
		bool undo() override;

	private:
		Plugin::ProcessorTree &processorTree_;

		Generation::BaseProcessor *deletedProcessor_ = nullptr;

		u64 destinationProcessorId_;
		size_t destinationSubProcessorIndex_;

		friend class PresetUpdate;
	};

	class ParameterUpdate final : public WaitingUpdate
	{
	public:
		ParameterUpdate(Interface::BaseControl *parameterUI, double oldValue, double newValue) :
			baseParameter_(parameterUI), oldValue_(oldValue), newValue_(newValue) { }

		bool perform() override;
		bool undo() override;

		void setDetailsChange(ParameterDetails details) noexcept { details_ = details; }

	private:
		Interface::BaseControl *baseParameter_;
		std::optional<ParameterDetails> details_{};
		double oldValue_;
		double newValue_;
		bool firstTime_ = true;

		friend class PresetUpdate;
	};

	class PresetUpdate final : public WaitingUpdate
	{
	public:
		PresetUpdate(Plugin::ProcessorTree &processorTree) : processorTree_(processorTree) { }

		bool perform() override;
		bool undo() override;

		void pushParameterChanges(ParameterValue *parameter);

	private:
		Plugin::ProcessorTree &processorTree_;
		std::vector<ParameterUpdate> parametersChanged_{};
		std::vector<AddProcessorUpdate> addedProcessors_{};
		std::vector<DeleteProcessorUpdate> deletedProcessors_{};
		bool firstTime_ = true;
	};
}
