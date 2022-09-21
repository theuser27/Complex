/*
	==============================================================================

		parameter_value.h
		Created: 11 Jul 2022 3:22:48pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "common.h"
#include "utils.h"
#include "simd_utils.h"
#include "parameters.h"

namespace Framework
{
	class ParameterValueControl
	{
	public:
		float getValueInternal() const noexcept { return value_.load(std::memory_order_acquire); }
		virtual void setValueInternal(float newValue) noexcept { value_.store(newValue, std::memory_order_release); }

	protected:
		std::atomic<float> value_ = kDefaultParameterValue;
		static constexpr float kDefaultParameterValue = 0.5f;
	};

	class ParameterModulator
	{
	public:
		// deltaValue is the difference between the current and previous values
		virtual simd_float getDeltaValue() { return currentValue_.load() - previousValue_.load(); };

	protected:
		utils::atomic_simd_float currentValue_{ 0.0f };
		utils::atomic_simd_float previousValue_{ 0.0f };
	};

	class ParameterValue;

	struct ParameterLink
	{
		// the lifetime of the UIControl and parameter are the same, so there's no danger of accessing freed memory
		// as for the hostControl, in the destructor of the UI element, tied to the pluginModule,
		// we reset it's pointer to the ParameterLink and so it cannot access the parameter/UIControl
		ParameterValueControl *UIControl = nullptr;
		ParameterValueControl *hostControl = nullptr;
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

		ParameterValue(const ParameterDetails *info, u64 parentModuleId) noexcept : 
			info_(info), parentModuleId_(parentModuleId)
		{
			normalisedValue_ = info_->defaultNormalisedValue;
			modulations_ = 0.0f;
			normalisedInternalValue_ = info_->defaultNormalisedValue;
			internalValue_ = info_->defaultValue;
			parameterLink_.parameter = this;
		}

		ParameterValue(const ParameterValue &other, u64 parentModuleId) noexcept : 
			info_(other.info_), parentModuleId_(parentModuleId)
		{
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
			parameterLink_.parameter = this;
		}

		ParameterValue(ParameterValue &&other, u64 parentModuleId) noexcept : 
			info_(other.info_), parentModuleId_(parentModuleId)
		{
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
			parameterLink_.parameter = this;
		}

		~ParameterValue() = default;

		void initialise()
		{
			utils::spinAndLock(isUsed_, false);

			normalisedValue_ = info_->defaultNormalisedValue;
			modulations_ = 0.0f;
			normalisedInternalValue_ = info_->defaultNormalisedValue;
			internalValue_ = info_->defaultValue;

			isUsed_.store(false, std::memory_order_release);
		}
		
		// prefer calling this only once if possible
		template<commonConcepts::ParameterRepresentation T>
		T getInternalValue(bool isNormalised = false) const noexcept
		{
			utils::spinAndLock(isUsed_, false);
			
			T result{};

			if constexpr (std::is_same_v<T, simd_float>)
			{
				COMPLEX_ASSERT(info_->scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
				COMPLEX_ASSERT(info_->scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");

				if (isNormalised)
					result = normalisedInternalValue_;
				else
					result = internalValue_;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				COMPLEX_ASSERT(info_->scale != ParameterScale::Toggle && "Parameter isn't supposed to be a toggle control");
				COMPLEX_ASSERT(info_->scale != ParameterScale::Indexed && "Parameter isn't supposed to be a choice control");

				if (info_->isStereo)
				{
					simd_float modulations = modulations_;
					simd_float difference = utils::getStereoDifference(modulations);
					if (isNormalised)
						result = (modulations - difference)[0];
					else
						result = utils::scaleValue(modulations - difference, info_)[0];
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
				COMPLEX_ASSERT(info_->scale == ParameterScale::Toggle || info_->scale == ParameterScale::Indexed &&
					"Parameter is supposed to be either a toggle or choice control");

				if (info_->scale == ParameterScale::Toggle)
					result = utils::reinterpretToInt(internalValue_);
				else
					result = utils::toInt(internalValue_);
			}
			else if constexpr (std::is_same_v<T, u32>)
			{
				COMPLEX_ASSERT(info_->scale == ParameterScale::Toggle || info_->scale == ParameterScale::Indexed &&
					"Parameter is supposed to be either a toggle or choice control");

				if (info_->scale == ParameterScale::Toggle)
					result = static_cast<u32>(utils::reinterpretToInt(internalValue_)[0]);
				else if (info_->isStereo)
				{
					simd_int difference = utils::getStereoDifference(utils::toInt(modulations_));
					result = static_cast<u32>(utils::scaleValue(modulations_ - utils::toFloat(difference), info_)[0]);
				}
				else
					result = utils::toInt(internalValue_)[0];
			}

			isUsed_.store(false, std::memory_order_release);
			return result;
		}

		void changeControl(ParameterValueControl *control, bool isHostControl)
		{
			utils::spinAndLock(isUsed_, false);

			if (isHostControl) parameterLink_.hostControl = control;
			else parameterLink_.UIControl = control;

			isUsed_.store(false, std::memory_order_release);
		}

		void changeModulators(utils::GeneralOperations operation, std::weak_ptr<ParameterModulator> modulator = {}, i32 index = -1)
		{
			utils::spinAndLock(isUsed_, false);

			switch (operation)
			{
			case utils::GeneralOperations::Add:
				COMPLEX_ASSERT(!modulator.expired() && "You're adding an empty modulator to parameter");
				COMPLEX_ASSERT(index < 0 && "You're updating instead of adding a modulator");

				if (index < 0) parameterLink_.modulators.emplace_back(modulator);
				else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, modulator);

				break;
			case utils::GeneralOperations::Remove:
				COMPLEX_ASSERT(index >= 0 && "You're haven't given an index for parameter to remove");
				COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

				parameterLink_.modulators.erase(parameterLink_.modulators.begin() + index);

				break;
			case utils::GeneralOperations::Update:
				COMPLEX_ASSERT(index >= 0 && "You're haven't given an index for parameter to update");
				COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

				parameterLink_.modulators[index] = modulator;
				break;
			default:
				break;
			}

			isUsed_.store(false, std::memory_order_release);
		}

		void updateValues() noexcept
		{
			utils::spinAndLock(isUsed_, false);
			
			// if there's a set hostControl set, then we're automating this parameter
			ParameterValueControl *control = (parameterLink_.hostControl) ? parameterLink_.hostControl : parameterLink_.UIControl;
			bool isChanged = false;

			if (control)
			{
				float newNormalisedValue = control->getValueInternal();
				if (normalisedValue_ != newNormalisedValue)
				{
					normalisedValue_ = newNormalisedValue;
					isChanged = true;
				}
			}

			simd_float newModulations = modulations_;
			for (auto &modulator : parameterLink_.modulators)
			{
				// only getting the change from the previous used value
				if (auto modulatorPointer = modulator.lock())
					newModulations += modulatorPointer->getDeltaValue();
			}

			if (isChanged || !utils::completelyEqual(modulations_, newModulations))
			{
				modulations_ = newModulations;
				isChanged = true;
			}

			if (isChanged)
			{
				normalisedInternalValue_ = simd_float::clamp(modulations_ + normalisedValue_, 0.0f, 1.0f);
				internalValue_ = utils::scaleValue(normalisedInternalValue_, info_);
			}

			isUsed_.store(false, std::memory_order_release);
		}

		float getNormalisedValue() const noexcept { return normalisedValue_; }

		const ParameterDetails *getParameterDetails() const noexcept { return info_; }
		const u64 getParentModuleId() const noexcept { return parentModuleId_; }

		// TODO: move to the UI side later
		ParameterLink *getParameterLink() noexcept { return &parameterLink_; }

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

		const ParameterDetails *info_;
		const u64 parentModuleId_;

		mutable std::atomic<bool> isUsed_ = false;
	};
}