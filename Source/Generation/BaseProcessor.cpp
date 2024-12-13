/*
  ==============================================================================

    BaseProcessor.cpp
    Created: 11 Jul 2022 3:35:27am
    Author:  theuser27

  ==============================================================================
*/

#include "BaseProcessor.hpp"
#include "Framework/parameter_value.hpp"
#include "Framework/parameter_bridge.hpp"
#include "../Plugin/ProcessorTree.hpp"
#include "Interface/Sections/BaseSection.hpp"

namespace Generation
{
  BaseProcessor::BaseProcessor(Plugin::ProcessorTree *processorTree, std::string_view processorType) noexcept :
    processorTree_{ processorTree }, processorType_{ processorType } { }
  BaseProcessor::~BaseProcessor() noexcept = default;

  BaseProcessor::BaseProcessor(const BaseProcessor &other) noexcept : 
    processorTree_(other.processorTree_), processorType_{ other.processorType_ }
  {
    processorParameters_.data.reserve(other.processorParameters_.data.size());
    for (auto &parameterPair : other.processorParameters_.data)
      processorParameters_.data.emplace_back(parameterPair.first,
        utils::up<Framework::ParameterValue>::create(*parameterPair.second));

    subProcessors_.reserve(other.subProcessors_.size());
    for (auto &subProcessor : other.subProcessors_)
    {
      auto &newInstance = subProcessors_.emplace_back(subProcessor->createCopy());
      newInstance->setParentProcessorId(processorId_);
    }

    dataBuffer_.copy(other.dataBuffer_);
  }

  BaseProcessor::BaseProcessor(BaseProcessor &&other) noexcept :
    processorTree_(other.processorTree_), processorType_{ other.processorType_ }
  {
    processorParameters_.data.reserve(other.processorParameters_.data.size());
    for (auto &parameterPair : other.processorParameters_.data)
      processorParameters_.data.emplace_back(parameterPair.first,
        utils::up<Framework::ParameterValue>::create(*parameterPair.second));

    subProcessors_ = COMPLEX_MOV(other.subProcessors_);
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
      for (usize i = 0; i < other.processorParameters_.data.size(); i++)
        processorParameters_.data.emplace_back(other.processorParameters_.data[i].first,
          utils::up<Framework::ParameterValue>::create(*other.processorParameters_[i]));

      subProcessors_.reserve(other.subProcessors_.size());
      for (auto &subProcessor : other.subProcessors_)
      {
        auto &newInstance = subProcessors_.emplace_back(subProcessor->createCopy());
        newInstance->setParentProcessorId(processorId_);
      }

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
      for (usize i = 0; i < other.processorParameters_.data.size(); i++)
        processorParameters_.data.emplace_back(other.processorParameters_.data[i].first,
          utils::up<Framework::ParameterValue>::create(*other.processorParameters_[i]));

      subProcessors_ = COMPLEX_MOV(other.subProcessors_);
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

  Framework::ParameterValue *BaseProcessor::getParameter(std::string_view parameterId) const noexcept
  {
    const auto parameterIter = processorParameters_.find(parameterId);
    COMPLEX_ASSERT(parameterIter != processorParameters_.data.end() && "Parameter was not found");
    return parameterIter->second.get();
  }

  Framework::ParameterValue * BaseProcessor::getParameterUnchecked(usize index) const noexcept 
  { return processorParameters_[index].get(); }

  usize BaseProcessor::getParameterCount() const noexcept { return processorParameters_.data.size(); }

  void BaseProcessor::updateParameters(UpdateFlag flag, float sampleRate, bool updateChildrenParameters) noexcept
  {
    if (flag == UpdateFlag::NoUpdates)
      return;

    for (usize i = 0; i < processorParameters_.data.size(); i++)
      if (processorParameters_[i]->getUpdateFlag() == flag)
        processorParameters_[i]->updateValue(sampleRate);

    if (updateChildrenParameters)
      for (auto &subModule : subProcessors_)
        subModule->updateParameters(flag, sampleRate);
  }

  void BaseProcessor::remapParameters(std::optional<std::span<Framework::ParameterBridge *>> bridges, 
    bool bridgeValueFromParameters, bool remapOnlyBridges) noexcept
  {
    if (!bridges.has_value())
    {
      for (auto &parameter : processorParameters_.data)
      {
        auto bridge = parameter.second->getParameterLink()->hostControl;
        if (!bridge)
          continue;
        
        if (remapOnlyBridges)
        {
          if (bridge->getParameterLink())
            bridge->resetParameterLink(nullptr);
          else
            bridge->resetParameterLink(parameter.second->getParameterLink(), bridgeValueFromParameters);
        }
        else
        {
          bridge->resetParameterLink(nullptr);
          parameter.second->changeBridge(nullptr);
        }
      }

      return;
    }

    auto bridgeSpan = bridges.value();
    COMPLEX_ASSERT(bridgeSpan.size() == processorParameters_.data.size());

    for (usize i = 0; i < processorParameters_.data.size(); ++i)
    {
      auto *parameter = processorParameters_.data[i].second.get();
      auto *newBridge = bridgeSpan[i];
      if (auto *oldBridge = processorParameters_.data[i].second->changeBridge(newBridge))
        oldBridge->resetParameterLink(nullptr);
      
      newBridge->resetParameterLink(parameter->getParameterLink(), bridgeValueFromParameters);
    }
  }

  utils::up<Interface::ProcessorSection> &BaseProcessor::getSavedSection() noexcept { return savedSection_; }
  void BaseProcessor::setSavedSection(utils::up<Interface::ProcessorSection> savedSection) noexcept { savedSection_ = COMPLEX_MOV(savedSection); }

  void BaseProcessor::createProcessorParameters(std::span<const std::string_view> parameterIds)
  {
    using namespace Framework;

    for (const auto &id : parameterIds)
      processorParameters_.data.emplace_back(id, 
        utils::up<ParameterValue>::create(getParameterDetails(id).value()));
  }
}
