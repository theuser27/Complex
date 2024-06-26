/*
	==============================================================================

		BaseProcessor.h
		Created: 11 Jul 2022 3:35:27am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <any>
#include <optional>
#include <span>

#include "Interface/LookAndFeel/Miscellaneous.h"
#include "Framework/vector_map.h"
#include "Framework/simd_buffer.h"

namespace Framework
{
	class ParameterValue;
}

namespace Plugin
{
	class ProcessorTree;
}

namespace Generation
{
	class BaseProcessor
	{
	public:
		BaseProcessor() = delete;
		BaseProcessor(const BaseProcessor &) = delete;
		BaseProcessor(BaseProcessor &&) = delete;

		BaseProcessor(Plugin::ProcessorTree *processorTree, u64 parentProcessorId, std::string_view processorType) noexcept;
		BaseProcessor(const BaseProcessor &other, u64 parentProcessorId) noexcept;
		BaseProcessor &operator=(const BaseProcessor &other) noexcept;
		BaseProcessor(BaseProcessor &&other, u64 parentProcessorId) noexcept;
		BaseProcessor &operator=(BaseProcessor &&other) noexcept;

		virtual ~BaseProcessor() noexcept;

		virtual void initialise() noexcept;

		// return type is nlohmann::json but the header is too big to just put here
		[[nodiscard]] virtual std::any serialiseToJson() const;
		static void deserialiseFromJson(BaseProcessor *processor, std::any jsonData);
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
		
		[[nodiscard]] Framework::ParameterValue *getParameter(std::string_view parameterName) const noexcept;
		[[nodiscard]] Framework::ParameterValue *getParameterUnchecked(size_t index) const noexcept;
		size_t getParameterCount() const noexcept;

		void updateParameters(UpdateFlag flag, float sampleRate, bool updateSubModuleParameters = true);

		//void randomiseParameters();
		//void setAllParametersRandomisation(bool toRandomise = true);
		//void setParameterRandomisation(std::string_view name, bool toRandomise = true);

		std::string_view getProcessorType() const noexcept { return processorType_; }
		u64 getProcessorId() const noexcept { return processorId_; }
		auto *getProcessorTree() const noexcept { return processorTree_; }
		auto getDataBuffer() const noexcept { return Framework::SimdBufferView{ dataBuffer_ }; }
		
		u64 getParentProcessorId() const noexcept { return parentProcessorId_; }
		void setParentProcessorId(u64 newParentModuleId) noexcept { parentProcessorId_ = newParentModuleId; }


		// i use this is a helper function so that i don't freak out when i see "new"
		// the processors get added to the tree when they get their processorId
		// this is NOT a memory leak
		template<std::derived_from<BaseProcessor> T, typename ... Args>
		T *makeSubProcessor(Args&& ... args) const
		{
			// regular construction, no extra arguments
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
					// regular construction, with extra arguments
					return new T(processorTree_, processorId_, std::forward<Args>(args)...);
				}
			}
		}

		void addListener(BaseProcessorListener *listener) { listeners_.push_back(listener); }

	protected:
		void createProcessorParameters(std::span<const std::string_view> parameterNames);

		// data contextual to every individual module
		Framework::SimdBuffer<Framework::complex<float>, simd_float> dataBuffer_{};
		std::vector<BaseProcessor *> subProcessors_{};
		Framework::VectorMap<std::string_view, std::unique_ptr<Framework::ParameterValue>> processorParameters_;

		// data contextual to the base BaseProcessor
		u64 parentProcessorId_ = 0;

		Plugin::ProcessorTree *const processorTree_ = nullptr;
		const u64 processorId_ = 0;
		const std::string_view processorType_{};

		std::vector<BaseProcessorListener *> listeners_{};
	};
}
