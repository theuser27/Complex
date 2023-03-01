/*
	==============================================================================

		BaseProcessor.cpp
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#include "BaseProcessor.h"

namespace Generation
{
	BaseProcessor::BaseProcessor(Plugin::ProcessorTree *moduleTree, u64 parentModuleId,
		std::string_view moduleType) noexcept : processorTree_(moduleTree), processorId_(processorTree_->getId(this)),
		 processorType_(moduleType) { parentProcessorId_ = parentModuleId; }

	BaseProcessor::BaseProcessor(const BaseProcessor &other, u64 parentModuleId) noexcept : 
		processorTree_(other.processorTree_), processorId_(processorTree_->getId(this)), processorType_(other.processorType_)
	{
		parentProcessorId_ = parentModuleId;

		processorParameters_.data.reserve(other.processorParameters_.data.size());
		for (auto &parameterPair : other.processorParameters_.data)
			processorParameters_.data.emplace_back(parameterPair.first,
				std::make_shared<Framework::ParameterValue>(*parameterPair.second, processorId_));

		subProcessors_.reserve(other.subProcessors_.size());
		for (auto &subProcessor : other.subProcessors_)
			subProcessors_.emplace_back(subProcessor->createCopy(processorId_));

		dataBuffer_.copy(other.dataBuffer_);
	}

	BaseProcessor::BaseProcessor(BaseProcessor &&other, u64 parentModuleId) noexcept : 
		processorTree_(other.processorTree_), processorId_(processorTree_->getId(this)), processorType_(other.processorType_)
	{
		parentProcessorId_ = parentModuleId;

		processorParameters_.data.reserve(other.processorParameters_.data.size());
		for (auto &parameterPair : other.processorParameters_.data)
			processorParameters_.data.emplace_back(parameterPair.first,
				std::make_shared<Framework::ParameterValue>(*parameterPair.second, processorId_));

		subProcessors_ = std::move(other.subProcessors_);
		for (auto &subProcessor : subProcessors_)
			subProcessor->setParentProcessorId(processorId_);

		dataBuffer_.swap(other.dataBuffer_);
	}


	BaseProcessor &BaseProcessor::operator=(const BaseProcessor &other) noexcept
	{
		COMPLEX_ASSERT(processorType_ == other.processorType_ && "Object to copy is not of the same type");

		if (this != &other)
		{
			processorParameters_.data.reserve(other.processorParameters_.data.size());
			for (size_t i = 0; i < other.processorParameters_.data.size(); i++)
				processorParameters_.data.emplace_back(other.processorParameters_.data[i].first,
					std::make_shared<Framework::ParameterValue>(*other.processorParameters_[i], processorId_));

			subProcessors_.reserve(other.subProcessors_.size());
			for (size_t i = 0; i < other.subProcessors_.size(); i++)
				subProcessors_.emplace_back(other.subProcessors_[i]->createCopy(processorId_));

			dataBuffer_.copy(other.dataBuffer_);
		}
		return *this;
	}

	BaseProcessor &BaseProcessor::operator=(BaseProcessor &&other) noexcept
	{
		COMPLEX_ASSERT(processorType_ == other.processorType_ && "Object to move is not of the same type");

		if (this != &other)
		{
			processorParameters_.data.reserve(other.processorParameters_.data.size());
			for (size_t i = 0; i < other.processorParameters_.data.size(); i++)
				processorParameters_.data.emplace_back(other.processorParameters_.data[i].first,
					std::make_shared<Framework::ParameterValue>(*other.processorParameters_[i], processorId_));

			subProcessors_ = std::move(other.subProcessors_);
			for (size_t i = 0; i < subProcessors_.size(); i++)
				subProcessors_[i]->setParentProcessorId(processorId_);

			dataBuffer_.swap(other.dataBuffer_);
		}
		return *this;
	}

	void BaseProcessor::createProcessorParameters(const Framework::ParameterDetails *details, size_t size) noexcept
	{
		using namespace Framework;
		for (size_t i = 0; i < size; i++)
			processorParameters_.data.emplace_back(details[i].name,
				std::make_shared<ParameterValue>(Parameters::getDetails(details[i].name), processorId_));
	}

	void BaseProcessor::updateParameters(UpdateFlag flag, bool updateSubModuleParameters)
	{
		if (flag == UpdateFlag::NoUpdates)
			return;

		for (size_t i = 0; i < processorParameters_.data.size(); i++)
			if (processorParameters_[i]->getParameterDetails().updateFlag == flag)
				processorParameters_[i]->updateValues();

		if (updateSubModuleParameters)
			for (auto &subModule : subProcessors_)
				subModule->updateParameters(flag);
	}
}

namespace Plugin
{
	u64 ProcessorTree::getId(Generation::BaseProcessor *newModulePointer, bool isRootModule) noexcept
	{
		if (isRootModule)
			return processorIdCounter_.load(std::memory_order_acquire);

		// litmus test that getId is not called on an already initialised module
		COMPLEX_ASSERT(newModulePointer->getParentProcessorId() == 0 && "We are adding a module that already exists??");

		std::unique_ptr<Generation::BaseProcessor> newModule{ newModulePointer };
		u64 newModuleId = processorIdCounter_.fetch_add(1, std::memory_order_acq_rel);

		allProcessors_->add(newModuleId, std::move(newModule));
		if ((float)allProcessors_->data.size() / allProcessors_->data.capacity() >= expandThreshold)
			resizeAllParameters();

		return newModuleId;
	}

	std::unique_ptr<Generation::BaseProcessor> ProcessorTree::deleteProcessor(u64 moduleId) noexcept
	{
		auto iter = allProcessors_->find(moduleId);
		std::unique_ptr<Generation::BaseProcessor> deletedModule = std::move(iter->second);
		allProcessors_->data.erase(iter);
		return std::move(deletedModule);
	}

	Generation::BaseProcessor *ProcessorTree::getProcessor(u64 moduleId) const noexcept
	{
		auto moduleIter = allProcessors_->find(moduleId);
		return (moduleIter != allProcessors_->data.end()) ?
			moduleIter->second.get() : nullptr;
	}

	std::weak_ptr<Framework::ParameterValue> ProcessorTree::getProcessorParameter(u64 parentModuleId, std::string_view parameter) const noexcept
	{
		auto modulePointer = getProcessor(parentModuleId);
		if (!modulePointer)
			return {};

		auto parameterIter = modulePointer->processorParameters_.find(parameter);
		return (parameterIter == modulePointer->processorParameters_.data.end()) ?
			std::weak_ptr<Framework::ParameterValue>() : parameterIter->second;
	}

	void ProcessorTree::resizeAllParameters() noexcept
	{
		auto newAllParameters = std::make_unique<Framework::VectorMap<u64,
			std::unique_ptr<Generation::BaseProcessor>>>(allProcessors_->data.size() * expandAmount);

		std::move(allProcessors_->data.begin(), allProcessors_->data.end(), newAllParameters->data.begin());
		allProcessors_.swap(newAllParameters);
	}
}