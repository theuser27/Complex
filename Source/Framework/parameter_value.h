/*
	==============================================================================

		parameter_value.h
		Created: 11 Jul 2022 3:22:48pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <variant>

#include "common.h"
#include "utils.h"
#include "simd_utils.h"
#include "parameters.h"

namespace Interface
{
	class ParameterUI;
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
		virtual simd_float getDeltaValue() { return currentValue_.load() - previousValue_.load(); };

	protected:
		utils::atomic_simd_float currentValue_{ 0.0f };
		utils::atomic_simd_float previousValue_{ 0.0f };
	};

	struct ParameterLink
	{
		// the lifetime of the UIControl and parameter are the same, so there's no danger of accessing freed memory
		// as for the hostControl, in the destructor of the UI element, tied to the BaseProcessor,
		// we reset it's pointer to the ParameterLink and so it cannot access the parameter/UIControl
		Interface::ParameterUI *UIControl = nullptr;
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
			details_(details), parentProcessorId_(parentModuleId)
		{
			normalisedValue_ = details_.defaultNormalisedValue;
			modulations_ = 0.0f;
			normalisedInternalValue_ = details_.defaultNormalisedValue;
			internalValue_ = details_.defaultValue;
		}

		ParameterValue(const ParameterValue &other, u64 parentModuleId) noexcept : 
			details_(other.details_), parentProcessorId_(parentModuleId)
		{
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
		}

		ParameterValue(ParameterValue &&other, u64 parentModuleId) noexcept : 
			details_(other.details_), parentProcessorId_(parentModuleId)
		{
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
		}

		~ParameterValue() = default;

		void initialise()
		{
			utils::ScopedSpinLock lock(waitLock_);

			normalisedValue_ = details_.defaultNormalisedValue;
			modulations_ = 0.0f;
			normalisedInternalValue_ = details_.defaultNormalisedValue;
			internalValue_ = details_.defaultValue;
		}
		
		// prefer calling this only once if possible
		template<commonConcepts::ParameterRepresentation T>
		T getInternalValue(bool isNormalised = false) const noexcept
		{
			utils::ScopedSpinLock lock(waitLock_);
			
			T result{};

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
						result = utils::scaleValue(modulations - difference, details_)[0];
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
					result = static_cast<u32>(utils::scaleValue(modulations_ - utils::toFloat(difference), details_)[0]);
				}
				else
					result = utils::toInt(internalValue_)[0];
			}

			return result;
		}

		void changeControl(std::variant<Interface::ParameterUI *, ParameterBridge *> control) noexcept
		{
			utils::ScopedSpinLock lock(waitLock_);

			if (std::holds_alternative<Interface::ParameterUI *>(control)) 
				parameterLink_.UIControl = std::get<0>(control);
			else parameterLink_.hostControl = std::get<1>(control);
		}

		void changeModulators(utils::GeneralOperations operation, std::weak_ptr<ParameterModulator> modulator = {}, i32 index = -1)
		{
			utils::ScopedSpinLock lock(waitLock_);

			switch (operation)
			{
			case utils::GeneralOperations::Add:
			case utils::GeneralOperations::AddCopy:
			case utils::GeneralOperations::AddMove:
				COMPLEX_ASSERT(!modulator.expired() && "You're adding an empty modulator to parameter");
				COMPLEX_ASSERT(index < 0 && "You're updating instead of adding a modulator");

				if (index < 0) parameterLink_.modulators.emplace_back(std::move(modulator));
				else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, std::move(modulator));

				break;
			case utils::GeneralOperations::Update:
			case utils::GeneralOperations::UpdateCopy:
			case utils::GeneralOperations::UpdateMove:
				COMPLEX_ASSERT(index >= 0 && "You're haven't given an index for parameter to update");
				COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

				parameterLink_.modulators[index] = std::move(modulator);

				break;
			case utils::GeneralOperations::Remove:
				COMPLEX_ASSERT(index >= 0 && "You're haven't given an index for parameter to remove");
				COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

				parameterLink_.modulators.erase(parameterLink_.modulators.begin() + index);

				break;
			default:
				break;
			}
		}

		void updateValues() noexcept;

		u64 getParentProcessorId() const noexcept { return parentProcessorId_; }

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
		void setParameterDetails(ParameterDetails details) noexcept
		{
			utils::ScopedSpinLock guard(waitLock_);
			details_ = details;
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

		ParameterLink parameterLink_{};

		ParameterDetails details_;
		const u64 parentProcessorId_;

		mutable std::atomic<bool> waitLock_ = false;
	};
}

namespace Interface
{
	class ParameterUI
	{
	public:
		virtual ~ParameterUI() = default;

		const Framework::ParameterDetails *getParameterDetails() const noexcept { return &details_; }
		void setParameterDetails(const Framework::ParameterDetails &details) noexcept { details_ = details; }

		Framework::ParameterLink *getParameterLink() const noexcept { return parameterLink_; }
		void setParameterLink(Framework::ParameterLink *parameterLink) noexcept { parameterLink_ = parameterLink; }

		void setValueFromHost(double newValue) noexcept { setValueSafe(newValue); }

		virtual bool updateValue() = 0;

		double getValueSafe() const noexcept
		{ return value_.load(std::memory_order_acquire); }

		void setValueSafe(double newValue) noexcept
		{ value_.store(newValue, std::memory_order_release); }

	protected:
		std::atomic<double> value_ = 0.0;

		Framework::ParameterLink *parameterLink_ = nullptr;
		Framework::ParameterDetails details_{};
	};
}
