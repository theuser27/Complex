/*
	==============================================================================

		PluginModule.cpp
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#include "PluginModule.h"

namespace Generation
{
	PluginModule::PluginModule(AllModules *globalModulesState, u64 parentModuleId,
		std::string_view moduleType) noexcept : globalModulesState_(globalModulesState), parentModuleId_(parentModuleId),
		moduleId_(globalModulesState_->getId()), moduleType_(moduleType) { }

	PluginModule::PluginModule(const PluginModule &other, u64 parentModuleId) noexcept : globalModulesState_(other.globalModulesState_),
		parentModuleId_(parentModuleId), moduleId_(globalModulesState_->getId()), moduleType_(other.moduleType_)
	{
		moduleParameters_.data.reserve(other.moduleParameters_.data.size());
		for (size_t i = 0; i < other.moduleParameters_.data.size(); i++)
			moduleParameters_.data.emplace_back(other.moduleParameters_.data[i].first,
				std::make_shared<Framework::ParameterValue>(*other.moduleParameters_[i], moduleId_));

		subModules_.reserve(other.subModules_.size());
		for (size_t i = 0; i < other.subModules_.size(); i++)
			subModules_.emplace_back(other.subModules_[i]->createCopy(moduleId_));
	}

	PluginModule::PluginModule(PluginModule &&other, u64 parentModuleId) noexcept : globalModulesState_(other.globalModulesState_),
		parentModuleId_(parentModuleId), moduleId_(globalModulesState_->getId()), moduleType_(other.moduleType_)
	{
		moduleParameters_.data.reserve(other.moduleParameters_.data.size());
		for (size_t i = 0; i < other.moduleParameters_.data.size(); i++)
			moduleParameters_.data.emplace_back(other.moduleParameters_.data[i].first,
				std::make_shared<Framework::ParameterValue>(*other.moduleParameters_[i], moduleId_));

		subModules_ = std::move(other.subModules_);
		for (size_t i = 0; i < subModules_.size(); i++)
			subModules_[i]->setParentModuleId(moduleId_);
	}


	PluginModule &PluginModule::operator=(const PluginModule &other) noexcept
	{
		if (this != &other && moduleType_ == other.moduleType_)
		{
			moduleParameters_.data.reserve(other.moduleParameters_.data.size());
			for (size_t i = 0; i < other.moduleParameters_.data.size(); i++)
				moduleParameters_.data.emplace_back(other.moduleParameters_.data[i].first,
					std::make_shared<Framework::ParameterValue>(*other.moduleParameters_[i], moduleId_));

			subModules_.reserve(other.subModules_.size());
			for (size_t i = 0; i < other.subModules_.size(); i++)
				subModules_.emplace_back(other.subModules_[i]->createCopy(moduleId_));
		}
		return *this;
	}

	PluginModule &PluginModule::operator=(PluginModule &&other) noexcept
	{
		if (this != &other && moduleType_ == other.moduleType_)
		{
			moduleParameters_.data.reserve(other.moduleParameters_.data.size());
			for (size_t i = 0; i < other.moduleParameters_.data.size(); i++)
				moduleParameters_.data.emplace_back(other.moduleParameters_.data[i].first,
					std::make_shared<Framework::ParameterValue>(*other.moduleParameters_[i], moduleId_));

			subModules_ = std::move(other.subModules_);
			for (size_t i = 0; i < subModules_.size(); i++)
				subModules_[i]->setParentModuleId(moduleId_);
		}
		return *this;
	}

	void PluginModule::createModuleParameters(const Framework::ParameterDetails *details, size_t size) noexcept
	{
		using namespace Framework;
		for (size_t i = 0; i < size; i++)
			moduleParameters_.data.emplace_back(details[i].name,
				std::make_shared<ParameterValue>(Parameters::getDetails(details[i].name), moduleId_));
	}

	void PluginModule::addSubModulesToList() noexcept
	{
		for (auto &subModule : subModules_)
			globalModulesState_->addModule(subModule);
	}

	void PluginModule::updateParameters(UpdateFlag flag, bool updateSubModuleParameters)
	{
		if (flag == UpdateFlag::NoUpdates)
			return;

		for (size_t i = 0; i < moduleParameters_.data.size(); i++)
			if (moduleParameters_[i]->getParameterDetails()->updateFlag == flag)
				moduleParameters_[i]->updateValues();

		if (updateSubModuleParameters)
			for (auto &subModule : subModules_)
				subModule->updateParameters(flag);
	}

	std::weak_ptr<Framework::ParameterValue> AllModules::getModuleParameter(u64 parentModuleId, std::string_view parameter) const noexcept
	{
		// we block until no one is doing anything
		utils::spinAndLock(currentlyInUse, false);

		auto moduleIter = allModules->find(parentModuleId);
		if (moduleIter == allModules->data.end())
			return std::weak_ptr<Framework::ParameterValue>();

		currentlyInUse.store(false, std::memory_order_release);

		auto lockedModule = moduleIter->second.lock();
		if (!lockedModule)
			return std::weak_ptr<Framework::ParameterValue>();

		auto parameterIter = lockedModule->moduleParameters_.find(parameter);
		return (parameterIter == lockedModule->moduleParameters_.data.end()) ?
			std::weak_ptr<Framework::ParameterValue>() : parameterIter->second;
	}

	bool AllModules::addModule(std::shared_ptr<PluginModule> newPointer) noexcept
	{
		if (auto flag = getUpdateFlag();
			!(flag == UpdateFlag::AfterProcess || flag == UpdateFlag::BeforeProcess))
			return false;

		// we block until no one is doing anything
		utils::spinAndLock(currentlyInUse, false);

		COMPLEX_ASSERT(allModules->find(newPointer->getModuleId()) == allModules->data.end()
			&& "We are adding a module that already exists??");
	
		allModules->add(newPointer->getModuleId(), newPointer);
		if ((float)allModules->data.size() /
			allModules->data.capacity() >= expandThreshold)
			resizeAllParameters();
		
		currentlyInUse.store(false, std::memory_order_release);
		return true;
	}

	bool AllModules::deleteModule(u64 moduleId) noexcept
	{
		// we're not checking for the update flag 
		// because this function is only called from destructors

		utils::spinAndLock(currentlyInUse, false);
		allModules->erase(moduleId);
		currentlyInUse.store(false, std::memory_order_release);

		return true;
	}

	void AllModules::resizeAllParameters() noexcept
	{
		auto newAllParameters = std::make_unique<Framework::VectorMap<u64,
			std::weak_ptr<PluginModule>>>(allModules->data.size() * expandAmount);

		std::copy(allModules->data.begin(), allModules->data.end(), newAllParameters->data.begin());
		allModules.swap(newAllParameters);
	}
}