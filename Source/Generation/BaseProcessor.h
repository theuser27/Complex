/*
	==============================================================================

		BaseProcessor.h
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/common.h"
#include "Framework/vector_map.h"
#include "Framework/simd_buffer.h"
#include "Framework/parameters.h"
#include "Framework/parameter_value.h"

namespace Generation
{
	class BaseProcessor;
}

namespace Plugin
{
	class ProcessorUpdate;

	// global state for keeping track of all parameters lists through global module number
	class ProcessorTree
	{
	protected:
		ProcessorTree() = default;
		virtual ~ProcessorTree() = default;

	public:
		ProcessorTree(const ProcessorTree &) = delete;
		ProcessorTree(ProcessorTree &&) = delete;
		ProcessorTree &operator=(const ProcessorTree &other) = delete;
		ProcessorTree &operator=(ProcessorTree &&other) = delete;

		// do not call this function manually, it's always called when you instantiate a BaseProcessor derived type
		// may block and allocate if expandThreshold has been reached
		u64 getId(Generation::BaseProcessor *newModulePointer, bool isRootModule = false) noexcept;
		std::unique_ptr<Generation::BaseProcessor> deleteProcessor(u64 moduleId) noexcept;

		// remember to use a scoped lock before calling these 2 methods
		Generation::BaseProcessor* getProcessor(u64 moduleId) const noexcept;
		// finds a parameter from a specified module with a specified name
		std::weak_ptr<Framework::ParameterValue> getProcessorParameter(u64 parentModuleId, std::string_view parameter) const noexcept;

		UpdateFlag getUpdateFlag() const noexcept { return updateFlag_.load(std::memory_order_acquire); }
		void setUpdateFlag(UpdateFlag newFlag) noexcept { updateFlag_.store(newFlag, std::memory_order_release); }

	protected:
		void resizeAllParameters() noexcept;

		std::unique_ptr<Framework::VectorMap<u64, std::unique_ptr<Generation::BaseProcessor>>> allProcessors_ =
			std::make_unique<Framework::VectorMap<u64, std::unique_ptr<Generation::BaseProcessor>>>(64);

		// used to give out non-repeating ids for all PluginModules
		std::atomic<u64> processorIdCounter_ = 0;
		// used for checking whether it's ok to update parameters/plugin structure
		std::atomic<UpdateFlag> updateFlag_ = UpdateFlag::AfterProcess;

		std::atomic<u32> samplesPerBlock_{};
		std::atomic<float> sampleRate_ = kDefaultSampleRate;

		mutable std::atomic<bool> waitLock_ = false;

		static constexpr size_t expandAmount = 2;
		static constexpr float expandThreshold = 0.75f;

		friend class ProcessorUpdate;
	};
}

namespace Generation
{
	class BaseProcessor
	{
	public:
		BaseProcessor() = delete;
		BaseProcessor(const BaseProcessor &) = delete;
		BaseProcessor(BaseProcessor &&) = delete;

		BaseProcessor(Plugin::ProcessorTree *moduleTree, u64 parentModuleId, std::string_view moduleType) noexcept;
		BaseProcessor(const BaseProcessor &other, u64 parentModuleId) noexcept;
		BaseProcessor &operator=(const BaseProcessor &other) noexcept;
		BaseProcessor(BaseProcessor &&other, u64 parentModuleId) noexcept;
		BaseProcessor &operator=(BaseProcessor &&other) noexcept;

		virtual ~BaseProcessor() noexcept = default;

		virtual void initialise() noexcept
		{
			for (auto &processorParameter : processorParameters_.data)
				processorParameter.second->initialise();
		}

		[[nodiscard]] virtual BaseProcessor *createCopy(u64 parentModuleId) const noexcept = 0;

		void clearSubProcessors() noexcept { subProcessors_.clear(); }

		BaseProcessor *getSubProcessor(size_t index) const noexcept { return subProcessors_[index]; }
		u32 getIndexOfSubProcessor(const BaseProcessor *subModule) noexcept 
		{ return subProcessors_.begin() - std::find(subProcessors_.begin(), subProcessors_.end(), subModule); }

		// the following functions are to be called outside of processing time
		virtual void insertSubProcessor([[maybe_unused]] u32 index, [[maybe_unused]] std::string_view newModuleType = {},
			[[maybe_unused]] BaseProcessor *newSubModule = nullptr) noexcept { }
		virtual BaseProcessor *deleteSubProcessor([[maybe_unused]] u32 index) noexcept { return nullptr; };
		virtual BaseProcessor *updateSubProcessor([[maybe_unused]] u32 index, [[maybe_unused]] std::string_view newModuleType = {},
			[[maybe_unused]] BaseProcessor *newSubModule = nullptr) noexcept { return nullptr; }
		
		[[nodiscard]] std::weak_ptr<Framework::ParameterValue> getParameter(std::string_view parameter) const noexcept
		{
			const auto parameterIter = processorParameters_.find(parameter);
			return (parameterIter == processorParameters_.data.end()) ?
				std::weak_ptr<Framework::ParameterValue>() : parameterIter->second;
		}
		void updateParameters(UpdateFlag flag, bool updateSubModuleParameters = true);

		void randomiseParameters();
		void setAllParametersRandomisation(bool toRandomise = true);
		void setParameterRandomisation(std::string_view name, bool toRandomise = true);

		std::string_view getProcessorType() const noexcept { return processorType_; }
		u64 getProcessorId() const noexcept { return processorId_; }
		
		u64 getParentProcessorId() const noexcept { return parentProcessorId_; }
		void setParentProcessorId(u64 newParentModuleId) noexcept { parentProcessorId_ = newParentModuleId; }


		// i use this function so that i don't freak out when i see "new"
		// the processors get added to the tree when they get their moduleId
		// this is NOT a memory leak
		template<typename T, typename ... Args>
		T *createSubProcessor(Args&& ... args) const
		{
			// default construction, no extra arguments
			if constexpr (sizeof...(Args) == 0)
			{
				return new T(processorTree_, processorId_);
			}
			else
			{
				// copy constructor
				if constexpr (std::is_base_of_v<BaseProcessor, std::remove_cvref_t<decltype(utils::getNthElement<0>(args))>>)
				{
					return new T(args);
				}
				else
				{
					// default construction, with extra arguments
					return new T(processorTree_, processorId_, std::forward<Args>(args)...);
				}
			}
		}

	protected:
		void createProcessorParameters(const Framework::ParameterDetails *details, size_t size) noexcept;

		// data contextual to every individual module
		Framework::SimdBuffer<std::complex<float>, simd_float> dataBuffer_{};
		std::vector<BaseProcessor *> subProcessors_{};
		Framework::VectorMap<std::string_view, std::shared_ptr<Framework::ParameterValue>> processorParameters_{};

		// data contextual to the base BaseProcessor
		u64 parentProcessorId_ = 0;

		Plugin::ProcessorTree *const processorTree_ = nullptr;
		const u64 processorId_ = 0;
		const std::string_view processorType_{};

		friend class Plugin::ProcessorTree;
	};
}

REFL_AUTO(type(Generation::BaseProcessor))
