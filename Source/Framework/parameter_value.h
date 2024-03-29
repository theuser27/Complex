/*
	==============================================================================

		parameter_value.h
		Created: 11 Jul 2022 3:22:48pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <variant>

#include "utils.h"
#include "simd_utils.h"
#include "parameters.h"

namespace Interface
{
	class BaseControl;
}

namespace Framework
{
	class ParameterBridge;
	class ParameterValue;

	class ParameterModulator
	{
	public:
		ParameterModulator() = default;
		virtual ~ParameterModulator() = default;

		// deltaValue is the difference between the current and previous values
		virtual simd_float getDeltaValue() { return currentValue_.load() - previousValue_.load(); }

	protected:
		utils::atomic_simd_float currentValue_{ 0.0f };
		utils::atomic_simd_float previousValue_{ 0.0f };
	};

	struct ParameterLink
	{
		// the lifetime of the UIControl and parameter are the same, so there's no danger of accessing freed memory
		// as for the hostControl, in the destructor of the UI element, tied to the BaseProcessor,
		// we reset it's pointer to the ParameterLink and so it cannot access the parameter/UIControl
		Interface::BaseControl *UIControl = nullptr;
		ParameterBridge *hostControl = nullptr;
		std::vector<std::weak_ptr<ParameterModulator>> modulators{};
		ParameterValue *parameter = nullptr;
	};

	class ParameterValue
	{
	public:
		ParameterValue() = delete;
		ParameterValue(const ParameterValue &) = delete;
		ParameterValue(ParameterValue &&) = delete;
		ParameterValue &operator=(const ParameterValue &) = delete;
		ParameterValue &operator=(ParameterValue &&) = delete;

		ParameterValue(ParameterDetails details, u64 parentModuleId) noexcept :
			details_(details), parentProcessorId_(parentModuleId) { initialise(); }

		ParameterValue(const ParameterValue &other, u64 parentModuleId) noexcept : 
			details_(other.details_), parentProcessorId_(parentModuleId)
		{
			utils::ScopedSpinLock g(other.waitLock_);
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
		}

		ParameterValue(ParameterValue &&other, u64 parentModuleId) noexcept : 
			ParameterValue(other, parentModuleId) { }

		void initialise(std::optional<float> value = {});
		
		// prefer calling this only once if possible
		template<CommonConcepts::ParameterRepresentation T>
		T getInternalValue(float sampleRate = kDefaultSampleRate, bool isNormalised = false) const noexcept
		{
			utils::ScopedSpinLock lock(waitLock_);
			
			T result;

			if constexpr (std::is_same_v<T, simd_float>)
			{
				COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
				COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");

				if (isNormalised)
					result = normalisedInternalValue_;
				else
					result = internalValue_;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				COMPLEX_ASSERT(details_.scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
				COMPLEX_ASSERT(details_.scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");

				if (details_.isStereo)
				{
					simd_float modulations = modulations_;
					simd_float difference = utils::getStereoDifference(modulations);
					if (isNormalised)
						result = (modulations - difference)[0];
					else
						result = scaleValue(modulations - difference, details_, sampleRate)[0];
				}
				else
				{
					if (isNormalised)
						result = normalisedInternalValue_[0];
					else
						result = internalValue_[0];
				}					
			}
			else if constexpr (std::is_same_v<T, simd_int>)
			{
				COMPLEX_ASSERT(details_.scale == ParameterScale::Toggle || details_.scale == ParameterScale::Indexed &&
					"Parameter is supposed to be either a toggle or choice control");

				if (details_.scale == ParameterScale::Toggle)
					result = utils::reinterpretToInt(internalValue_);
				else
					result = utils::toInt(internalValue_);
			}
			else if constexpr (std::is_same_v<T, u32>)
			{
				COMPLEX_ASSERT(details_.scale == ParameterScale::Toggle || details_.scale == ParameterScale::Indexed &&
					"Parameter is supposed to be either a toggle or choice control");

				if (details_.scale == ParameterScale::Toggle)
					result = static_cast<u32>(utils::reinterpretToInt(internalValue_)[0]);
				else if (details_.isStereo)
				{
					simd_int difference = utils::getStereoDifference(utils::toInt(modulations_));
					result = static_cast<u32>(scaleValue(modulations_ - utils::toFloat(difference), details_, sampleRate)[0]);
				}
				else
					result = utils::toInt(internalValue_)[0];
			}

			return result;
		}

		auto changeControl(std::variant<Interface::BaseControl *, ParameterBridge *> control) noexcept
			-> std::variant<Interface::BaseControl *, ParameterBridge *>;

		void addModulator(std::weak_ptr<ParameterModulator> modulator, i64 index = -1);

		auto updateModulator(std::weak_ptr<ParameterModulator> modulator, size_t index) noexcept
			-> std::weak_ptr<ParameterModulator>;

		auto deleteModulator(size_t index) noexcept
			-> std::weak_ptr<ParameterModulator>;

		void updateValues(float sampleRate, std::optional<float> value = {}) noexcept;

		float getNormalisedValue() const noexcept
		{
			utils::ScopedSpinLock guard(waitLock_);
			return normalisedValue_;
		}
		ParameterDetails getParameterDetails() const noexcept
		{
			utils::ScopedSpinLock guard(waitLock_);
			return details_;
		}
		auto *getParameterLink() noexcept { return &parameterLink_; }
		u64 getParentProcessorId() const noexcept { return parentProcessorId_; }

		void setParameterDetails(const ParameterDetails &details) noexcept
		{
			utils::ScopedSpinLock guard(waitLock_);
			details_ = details;
			isDirty_ = true;
		}

	private:
		// normalised, received from gui changes or from host when mapped out
		float normalisedValue_ = 0.0f;
		// value of all internal modulations
		simd_float modulations_ = 0.0f;
		// after adding modulations
		simd_float normalisedInternalValue_ = 0.0f;
		// after adding modulations and scaling
		simd_float internalValue_ = 0.0f;

		ParameterLink parameterLink_{ nullptr, nullptr, {}, this };

		ParameterDetails details_;
		const u64 parentProcessorId_;

		mutable std::atomic<bool> waitLock_ = false;
		bool isDirty_ = false;
	};
}
