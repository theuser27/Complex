/*
	==============================================================================

		BaseProcessor.cpp
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#include "BaseProcessor.h"
#include "Framework/parameter_value.h"
#include "../Plugin/ProcessorTree.h"

namespace Generation
{
	BaseProcessor::BaseProcessor(Plugin::ProcessorTree *processorTree, u64 parentProcessorId,
		std::string_view processorType) noexcept : processorTree_(processorTree), processorId_(processorTree_->getId(this)),
		 processorType_(processorType) { parentProcessorId_ = parentProcessorId; }
	BaseProcessor::~BaseProcessor() noexcept = default;

	BaseProcessor::BaseProcessor(const BaseProcessor &other, u64 parentProcessorId) noexcept :
		processorTree_(other.processorTree_), processorId_(processorTree_->getId(this)), processorType_(other.processorType_)
	{
		parentProcessorId_ = parentProcessorId;

		processorParameters_.data.reserve(other.processorParameters_.data.size());
		for (auto &parameterPair : other.processorParameters_.data)
			processorParameters_.data.emplace_back(parameterPair.first,
				std::make_unique<Framework::ParameterValue>(*parameterPair.second, processorId_));

		subProcessors_.reserve(other.subProcessors_.size());
		for (auto &subProcessor : other.subProcessors_)
			subProcessors_.emplace_back(subProcessor->createCopy(processorId_));

		dataBuffer_.copy(other.dataBuffer_);
	}

	BaseProcessor::BaseProcessor(BaseProcessor &&other, u64 parentProcessorId) noexcept :
		processorTree_(other.processorTree_), processorId_(processorTree_->getId(this)), processorType_(other.processorType_)
	{
		parentProcessorId_ = parentProcessorId;

		processorParameters_.data.reserve(other.processorParameters_.data.size());
		for (auto &parameterPair : other.processorParameters_.data)
			processorParameters_.data.emplace_back(parameterPair.first,
				std::make_unique<Framework::ParameterValue>(*parameterPair.second, processorId_));

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
					std::make_unique<Framework::ParameterValue>(*other.processorParameters_[i], processorId_));

			subProcessors_.reserve(other.subProcessors_.size());
			for (auto &subProcessor : other.subProcessors_)
				subProcessors_.emplace_back(subProcessor->createCopy(processorId_));

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
					std::make_unique<Framework::ParameterValue>(*other.processorParameters_[i], processorId_));

			subProcessors_ = std::move(other.subProcessors_);
			for (auto &subProcessor : subProcessors_)
				subProcessor->setParentProcessorId(processorId_);

			dataBuffer_.swap(other.dataBuffer_);
		}
		return *this;
	}

	void BaseProcessor::initialise() noexcept
	{
		for (auto &processorParameter : processorParameters_.data)
			processorParameter.second->initialise();
	}

	Framework::ParameterValue * BaseProcessor::getParameter(std::string_view parameterName) const noexcept
	{
		const auto parameterIter = processorParameters_.find(parameterName);
		COMPLEX_ASSERT(parameterIter != processorParameters_.data.end() && "Parameter was not found");
		return parameterIter->second.get();
	}

	Framework::ParameterValue * BaseProcessor::getParameterUnchecked(size_t index) const noexcept 
	{ return processorParameters_[index].get(); }

	size_t BaseProcessor::getParameterCount() const noexcept { return processorParameters_.data.size(); }

	void BaseProcessor::updateParameters(Framework::UpdateFlag flag, float sampleRate, bool updateSubModuleParameters)
	{
		if (flag == Framework::UpdateFlag::NoUpdates)
			return;

		for (size_t i = 0; i < processorParameters_.data.size(); i++)
			if (processorParameters_[i]->getParameterDetails().updateFlag == flag)
				processorParameters_[i]->updateValues(sampleRate);

		if (updateSubModuleParameters)
			for (auto &subModule : subProcessors_)
				subModule->updateParameters(flag, sampleRate);
	}

	void BaseProcessor::createProcessorParameters(std::span<const std::string_view> parameterNames)
	{
		using namespace Framework;

		for (size_t i = 0; i < parameterNames.size(); i++)
			processorParameters_.data.emplace_back(parameterNames[i],
				std::make_unique<ParameterValue>(Parameters::getDetailsEnum(parameterNames[i]), processorId_));
	}
}
