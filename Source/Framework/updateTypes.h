/*
	==============================================================================

		updateTypes.h
		Created: 16 Nov 2022 2:11:06am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Generation/PluginModule.h"

namespace Interface
{
	class ModuleSection;
}

namespace Framework
{
	class ProcessorUpdate : public juce::UndoableAction
	{
	public:
		ProcessorUpdate(Plugin::ProcessorTree *moduleTree, utils::GeneralOperations updateType, u64 destModuleId, u32 destSubModuleIndex,
			std::string_view newSubModuleType = {}, u64 sourceModuleId = -1, u32 sourceSubModuleIndex = -1) noexcept :
			processorTree_(moduleTree), updateType_(updateType), destModuleId_(destModuleId), destSubModuleIndex_(destSubModuleIndex),
			newSubModuleType_(newSubModuleType), sourceModuleId_(sourceModuleId), sourceSubModuleIndex_(sourceSubModuleIndex) { }

		// if the update has been deleted/overwritten in the undo history 
		// destroy the updated/deleted module
		~ProcessorUpdate() override { if (savedModule_) processorTree_->deleteProcessor(savedModule_->getProcessorId()); }

		bool perform() override;
		bool undo() override;

		int getSizeInUnits() override { return (int)sizeof(*this); }
		void doCopy()
		{
			savedModule_ = processorTree_->getProcessor(sourceModuleId_)->
				getSubProcessor(sourceSubModuleIndex_)->createCopy(destModuleId_);
			hasCopied_ = true;
		}

	private:
		Plugin::ProcessorTree *processorTree_;
		utils::GeneralOperations updateType_;
		u64 destModuleId_;
		u32 destSubModuleIndex_;
		// if we're creating a new module
		std::string_view newSubModuleType_;
		// if we're copying/moving from a different module
		u64 sourceModuleId_;
		u32 sourceSubModuleIndex_;

		// the following data members are used for bookkeeping
		Generation::BaseProcessor *savedModule_ = nullptr;
		bool hasCopied_ = false;
	};

	class ParameterUpdate : public juce::UndoableAction
	{
	public:
		ParameterUpdate(Interface::ModuleSection *moduleSection, float newValue) :
			moduleSection_(moduleSection), newValue_(newValue) { }

		bool perform() override;
		bool undo() override;

		int getSizeInUnits() override { return (int)sizeof(*this); }

	private:
		Interface::ModuleSection *moduleSection_;
		float newValue_ = 0.0f;
	};

	// (eventually) for any kind of changes in the modulator amounts
	// TODO: modulator updates for the undo manager when you get to modulators
	/*struct ModulatorUpdate : public juce::UndoableAction
	{

	};*/
}
