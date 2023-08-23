/*
	==============================================================================

		update_types.cpp
		Created: 16 Nov 2022 2:11:06am
		Author:  theuser27

	==============================================================================
*/

#include "update_types.h"
#include "Framework/parameter_value.h"
#include "Interface/Components/BaseControl.h"

namespace Framework
{
	bool AddProcessorUpdate::perform()
	{
		COMPLEX_ASSERT(!newSubProcessorType_.empty() && "A type of subProcessor to add was not provided");

		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		// if we've already undone once and we're redoing, we need to pass the already created module
		if (!addedProcessor_)
			addedProcessor_ = destinationPointer->createSubProcessor(newSubProcessorType_);

		auto g = wait();
		destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, addedProcessor_);

		return true;
	}

	bool AddProcessorUpdate::undo()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		addedProcessor_ = destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

		return true;
	}

	bool CopyProcessorUpdate::perform()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, &processorCopy);

		return true;
	}

	bool CopyProcessorUpdate::undo()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

		return true;
	}

	bool MoveProcessorUpdate::perform()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);
		auto sourcePointer = processorTree_->getProcessor(sourceProcessorId_);

		auto g = wait();
		auto movedProcessor = sourcePointer->deleteSubProcessor(sourceSubProcessorIndex_);
		destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, movedProcessor);

		return true;
	}

	bool MoveProcessorUpdate::undo()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);
		auto sourcePointer = processorTree_->getProcessor(sourceProcessorId_);

		auto g = wait();
		auto movedProcessor = destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);
		sourcePointer->insertSubProcessor(sourceSubProcessorIndex_, movedProcessor);

		return true;
	}

	bool UpdateProcessorUpdate::perform()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		// if we've already undone once and we're redoing, we need to pass the already created module
		if (!savedProcessor_)
			savedProcessor_ = destinationPointer->createSubProcessor(newSubProcessorType_);

		auto g = wait();
		savedProcessor_ = destinationPointer->updateSubProcessor(destinationSubProcessorIndex_, savedProcessor_);

		return true;
	}

	bool UpdateProcessorUpdate::undo()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		savedProcessor_ = destinationPointer->updateSubProcessor(destinationSubProcessorIndex_, savedProcessor_);

		return true;
	}

	bool DeleteProcessorUpdate::perform()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		deletedProcessor_ = destinationPointer->deleteSubProcessor(destinationSubProcessorIndex_);

		return true;
	}

	bool DeleteProcessorUpdate::undo()
	{
		auto destinationPointer = processorTree_->getProcessor(destinationProcessorId_);

		auto g = wait();
		destinationPointer->insertSubProcessor(destinationSubProcessorIndex_, deletedProcessor_);

		return true;
	}

	bool ParameterUpdate::perform()
	{
		// if this is being performed right after being added to the UndoManager, then the value change was already made
		// on the other hand, if this is being called on a redo, we need to change the value appropriately
		if (!firstTime_)
		{
			baseParameter_->setValueSafe(newValue_);
			baseParameter_->updateValue();
			baseParameter_->setValueToHost();
		}
		
		firstTime_ = false;
		return true;
	}

	bool ParameterUpdate::undo()
	{
		baseParameter_->setValueSafe(oldValue_);
		baseParameter_->updateValue();
		baseParameter_->setValueToHost();
		return true;
	}

}
