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
#include "Framework/parameter_value.h"

// this macro is used to give every type an identifier separate from its name
// the rationale for using a separate value from the name is because names can change
// and further down the line that could brick stuff
#define DEFINE_CLASS_TYPE(Token) static constexpr std::string_view getClassType() noexcept { return Token; }

namespace Plugin
{
	class ProcessorTree;
}

namespace Generation
{
	class BaseProcessor
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void insertedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *newSubProcessor) { }
			virtual void deletedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *deletedSubProcessor) { }
			virtual void updatedSubProcessor([[maybe_unused]] size_t index, [[maybe_unused]] BaseProcessor *oldSubProcessor, 
				[[maybe_unused]] BaseProcessor *newSubProcessor) { }
		};

		BaseProcessor() = delete;
		BaseProcessor(const BaseProcessor &) = delete;
		BaseProcessor(BaseProcessor &&) = delete;

		BaseProcessor(Plugin::ProcessorTree *processorTree, u64 parentProcessorId, std::string_view processorType) noexcept;
		BaseProcessor(const BaseProcessor &other, u64 parentProcessorId) noexcept;
		BaseProcessor &operator=(const BaseProcessor &other) noexcept;
		BaseProcessor(BaseProcessor &&other, u64 parentProcessorId) noexcept;
		BaseProcessor &operator=(BaseProcessor &&other) noexcept;

		virtual ~BaseProcessor() noexcept = default;

		virtual void initialise() noexcept
		{
			for (auto &processorParameter : processorParameters_.data)
				processorParameter.second->initialise();
		}

		[[nodiscard]] virtual BaseProcessor *createSubProcessor(std::string_view type) const noexcept = 0;
		[[nodiscard]] virtual BaseProcessor *createCopy(std::optional<u64> parentModuleId) const noexcept = 0;
		void clearSubProcessors() noexcept { subProcessors_.clear(); }

		BaseProcessor *getSubProcessor(size_t index) const noexcept { return subProcessors_[index]; }
		size_t getIndexOfSubProcessor(const BaseProcessor *subModule) noexcept 
		{ return subProcessors_.begin() - std::ranges::find(subProcessors_, subModule); }

		// the following functions are to be called outside of processing time
		virtual bool insertSubProcessor([[maybe_unused]] size_t index,
			[[maybe_unused]] BaseProcessor *newSubProcessor) noexcept { return false; }
		virtual BaseProcessor *deleteSubProcessor([[maybe_unused]] size_t index) noexcept { return nullptr; }
		virtual BaseProcessor *updateSubProcessor([[maybe_unused]] size_t index,
			[[maybe_unused]] BaseProcessor *newSubProcessor) noexcept { return nullptr; }
		
		[[nodiscard]] auto *getParameter(std::string_view parameterId) const noexcept
		{
			const auto parameterIter = processorParameters_.find(Framework::Parameters::getEnumString(parameterId));
			COMPLEX_ASSERT(parameterIter != processorParameters_.data.end() && "Parameter was not found");
			return parameterIter->second.get();
		}
		[[nodiscard]] auto *getParameter(nested_enum::NestedEnum auto parameterEnum) const noexcept
		{
			const auto parameterIter = processorParameters_.find(parameterEnum.enum_string(false));
			COMPLEX_ASSERT(parameterIter != processorParameters_.data.end() && "Parameter was not found");
			return parameterIter->second.get();
		}
		[[nodiscard]] Framework::ParameterValue *getParameterUnchecked(size_t index) const noexcept
		{ return processorParameters_[index].get(); }
		auto getParameterCount() const noexcept { return processorParameters_.data.size(); }

		void updateParameters(Framework::UpdateFlag flag, float sampleRate, bool updateSubModuleParameters = true);

		//void randomiseParameters();
		//void setAllParametersRandomisation(bool toRandomise = true);
		//void setParameterRandomisation(std::string_view name, bool toRandomise = true);

		auto getProcessingTime() const noexcept { return processingTime_.load(std::memory_order_acquire); }
		std::string_view getProcessorType() const noexcept { return processorType_; }
		u64 getProcessorId() const noexcept { return processorId_; }
		auto *getProcessorTree() const noexcept { return processorTree_; }
		
		u64 getParentProcessorId() const noexcept { return parentProcessorId_; }
		void setParentProcessorId(u64 newParentModuleId) noexcept { parentProcessorId_ = newParentModuleId; }


		// i use this is a helper function so that i don't freak out when i see "new"
		// the processors get added to the tree when they get their processorId
		// this is NOT a memory leak
		// only subProcessors that have a defined class type can be created, this is by design
		template<std::derived_from<BaseProcessor> T, typename ... Args>
		T *makeSubProcessor(Args&& ... args) const
			requires requires { T::getClassType(); }
		{
			// default construction, no extra arguments
			if constexpr (sizeof...(Args) == 0)
			{
				return new T(processorTree_, processorId_);
			}
			else
			{
				// copy constructor
				if constexpr (std::is_base_of_v<BaseProcessor, std::remove_cvref_t<decltype(utils::getNthElement<0>(args...))>>)
				{
					return new T(std::forward<Args>(args)...);
				}
				else
				{
					// default construction, with extra arguments
					return new T(processorTree_, processorId_, std::forward<Args>(args)...);
				}
			}
		}

		void addListener(Listener *listener) { listeners_.push_back(listener); }

	protected:
		template<nested_enum::NestedEnum E>
		void createProcessorParameters()
		{
			using namespace Framework;

			constexpr auto enumStrings = E::template enum_strings<nested_enum::OuterNodes>();
			
			for (size_t i = 0; i < enumStrings.size(); i++)
				processorParameters_.data.emplace_back(enumStrings[i],
					std::make_unique<ParameterValue>(Parameters::getDetailsEnum(enumStrings[i]), processorId_));
		}

		void setProcessingTime(std::chrono::duration<double, std::micro> time) noexcept 
		{ processingTime_.store(time, std::memory_order_release); }

		// data contextual to every individual module
		Framework::SimdBuffer<std::complex<float>, simd_float> dataBuffer_{};
		std::vector<BaseProcessor *> subProcessors_{};
		Framework::VectorMap<std::string_view, std::unique_ptr<Framework::ParameterValue>> processorParameters_{};
		std::atomic<std::chrono::duration<double, std::micro>> processingTime_{};

		// data contextual to the base BaseProcessor
		u64 parentProcessorId_ = 0;

		Plugin::ProcessorTree *const processorTree_ = nullptr;
		const u64 processorId_ = 0;
		const std::string_view processorType_{};

		std::vector<Listener *> listeners_{};
	};
}
