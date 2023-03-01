/*
  ==============================================================================

    updateTypes.cpp
    Created: 16 Nov 2022 2:11:06am
    Author:  theuser27

  ==============================================================================
*/

#include "UpdateTypes.h"

namespace Framework
{
	bool ProcessorUpdate::perform()
	{
		auto destModulePointer = processorTree_->getProcessor(destModuleId_);
		auto sourceModulePointer = processorTree_->getProcessor(sourceModuleId_);

		switch (updateType_)
		{
		case utils::GeneralOperations::Update:
			// if we've already undone once and we're redoing, we need to pass the already created module
			if (!savedModule_)
				savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, newSubModuleType_);
			else
				savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::Add:
			// if we've already undone once and we're redoing, we need to pass the already created module
			if (!savedModule_)
				destModulePointer->insertSubProcessor(destSubModuleIndex_, newSubModuleType_);
			else
				destModulePointer->insertSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::UpdateCopy:
			if (!hasCopied_) doCopy();
			savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::UpdateMove:
			savedModule_ = sourceModulePointer->deleteSubProcessor(sourceSubModuleIndex_);
			savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::AddCopy:
			if (!hasCopied_) doCopy();
			destModulePointer->insertSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::AddMove:
			savedModule_ = sourceModulePointer->deleteSubProcessor(sourceSubModuleIndex_);
			destModulePointer->insertSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::Remove:
			savedModule_ = destModulePointer->deleteSubProcessor(destSubModuleIndex_);
			break;
		default:
			break;
		}

		return true;
	}

	bool ProcessorUpdate::undo()
	{
		auto destModulePointer = processorTree_->getProcessor(destModuleId_);
		auto sourceModulePointer = processorTree_->getProcessor(sourceModuleId_);

		switch (updateType_)
		{
		case utils::GeneralOperations::Update:
			savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::Add:
			savedModule_ = destModulePointer->deleteSubProcessor(destSubModuleIndex_);
			break;
		case utils::GeneralOperations::UpdateCopy:
			savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::UpdateMove:
			savedModule_ = destModulePointer->updateSubProcessor(destSubModuleIndex_, {}, savedModule_);
			sourceModulePointer->insertSubProcessor(sourceSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::AddCopy:
			savedModule_ = destModulePointer->deleteSubProcessor(destSubModuleIndex_);
			break;
		case utils::GeneralOperations::AddMove:
			savedModule_ = destModulePointer->deleteSubProcessor(destSubModuleIndex_);
			sourceModulePointer->insertSubProcessor(sourceSubModuleIndex_, {}, savedModule_);
			break;
		case utils::GeneralOperations::Remove:
			destModulePointer->insertSubProcessor(destSubModuleIndex_, {}, savedModule_);
			break;
		default:
			break;
		}

		return true;
	}
}