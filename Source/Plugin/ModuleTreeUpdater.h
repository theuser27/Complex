/*
	==============================================================================

		ModuleTreeUpdater.h
		Created: 9 Oct 2022 4:50:24am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Framework/fifo_queue.h"
#include "Generation/PluginModule.h"
#include "Generation/SoundEngine.h"
#include "Framework/updateTypes.h"

namespace Plugin
{
	class ModuleTreeUpdater
	{
	public:
		ModuleTreeUpdater(ProcessorTree *moduleTree) noexcept : processorTree_(moduleTree) {}

		void updateStructureFromParameter() noexcept
		{

		}

		/*void updatePluginStructure() noexcept
		{
			// TODO: split this function for parameter- and structural-based changes
			using namespace Framework;

			utils::ScopedSpinLock scopedLock(processorTree_->waitLock_);

			// checks for changes in the module type parameter
			// and changes the module accordingly
			for (size_t i = 0; i < processorTree_->allProcessors_->data.size(); i++)
			{
				if (processorTree_->getUpdateFlag() != UpdateFlag::AfterProcess)
					return;

				auto modulePointer = processorTree_->allProcessors_->operator[](i).lock();
				if (modulePointer->getProcessorType() != kPluginModules[(u32)PluginModuleTypes::EffectModule])
					continue;

				auto moduleEffectIndex = modulePointer->getParameter(effectModuleParameterList[1].name).lock()->getInternalValue<u32>();
				if (static_cast<Generation::baseEffect *>(modulePointer->getSubProcessor(0).lock().get())->
					getEffectType() == kEffectModuleNames[moduleEffectIndex])
					continue;

				modulePointer->insertSubProcessor(0, kEffectModuleNames[moduleEffectIndex]);
			}

			for (size_t i = 0; i < updatesQueue_.getSize(); i++)
			{
				if (processorTree_->getUpdateFlag() != UpdateFlag::AfterProcess)
					return;

				auto update = updatesQueue_.pop_front();
				auto moduleInstance = processorTree_->getProcessor(update.moduleId);
				std::shared_ptr<Generation::BaseProcessor> deletedModule{};
				if (update.newSubModuleType == kPluginModules[(u32)PluginModuleTypes::Effect])
					update.newSubModuleType = update.newEffectType;

				switch (update.updateType)
				{
				case utils::GeneralOperations::Update:
					deletedModule = moduleInstance->deleteSubProcessor(update.subModuleIndex);
				case utils::GeneralOperations::Add:
					moduleInstance->insertSubProcessor(update.subModuleIndex, update.newSubModuleType);
					break;
				case utils::GeneralOperations::UpdateCopy:
					deletedModule = moduleInstance->deleteSubProcessor(update.subModuleIndex);
				case utils::GeneralOperations::AddCopy:
					moduleInstance->copySubModule(update.newInstance, update.subModuleIndex);
					break;
				case utils::GeneralOperations::UpdateMove:
					deletedModule = moduleInstance->deleteSubProcessor(update.subModuleIndex);
				case utils::GeneralOperations::AddMove:
					moduleInstance->moveSubModule(update.newInstance, update.subModuleIndex);
					break;
				case utils::GeneralOperations::Remove:
					deletedModule = moduleInstance->deleteSubProcessor(update.subModuleIndex);
					break;
				default:
					break;
				}

				if (deletedModule)
				{
					// TODO: store in undo manager
				}
			}
		}*/

		void pushUpdates(Framework::ProcessorUpdate ) noexcept
		{

		}

	private:
		// i doubt we'll ever go above this number of elements
		//Framework::FIFOQueue<ProcessorUpdate> updatesQueue_{ 64 };
		ProcessorTree *processorTree_ = nullptr;
	};
}