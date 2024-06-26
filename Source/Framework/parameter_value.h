/*
	==============================================================================

		parameter_value.h
		Created: 11 Jul 2022 3:22:48pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <variant>
#include <any>
#include <vector>
#include <memory>

#include "utils.h"
#include "sync_primitives.h"
#include "simd_utils.h"
#include "parameters.h"

namespace Plugin
{
	class ProcessorTree;
}

namespace Interface
{
	class BaseControl;
}

namespace Framework
{
	template<typename T>
	concept ParameterRepresentation = std::same_as<T, float> || std::same_as<T, u32> || SimdValue<T>;

	struct atomic_simd_float
	{
		atomic_simd_float(simd_float value) : value(value) { }

		simd_float load() const noexcept
		{
			utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
			simd_float currentValue = value;
			return currentValue;
		}

		void store(simd_float newValue) noexcept
		{
			utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
			value = newValue;
		}

		simd_float add(const simd_float &other) noexcept
		{
			utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
			value += other;
			simd_float result = value;
			return result;
		}

	private:
		simd_float value = 0.0f;
		mutable std::atomic<bool> guard = false;
	};

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
		atomic_simd_float currentValue_{ 0.0f };
		atomic_simd_float previousValue_{ 0.0f };
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
		ParameterValue() = default;
	public:
		ParameterValue(const ParameterValue &) = delete;
		ParameterValue(ParameterValue &&) = delete;
		ParameterValue &operator=(const ParameterValue &) = delete;
		ParameterValue &operator=(ParameterValue &&) = delete;

		ParameterValue(ParameterDetails details, u64) noexcept :
			details_(details) { initialise(); }

		ParameterValue(const ParameterValue &other, u64) noexcept : 
			details_(other.details_)
		{
			utils::ScopedLock g{ other.waitLock_, utils::WaitMechanism::Spin };
			normalisedValue_ = other.normalisedValue_;
			modulations_ = other.modulations_;
			normalisedInternalValue_ = other.normalisedInternalValue_;
			internalValue_ = other.internalValue_;
		}

		void initialise(std::optional<float> value = {})
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

			normalisedValue_ = value.value_or(details_.defaultNormalisedValue);
			modulations_ = 0.0f;
			normalisedInternalValue_ = value.value_or(details_.defaultNormalisedValue);
			internalValue_ = (!value.has_value()) ? details_.defaultValue :
				(float)scaleValue(value.value(), details_);
			isDirty_ = false;
		}
		
		// prefer calling this only once if possible
		template<ParameterRepresentation T>
		T getInternalValue(float sampleRate = kDefaultSampleRate, bool isNormalised = false) const noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			
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
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			
			if (std::holds_alternative<Interface::BaseControl *>(control))
			{
				auto variant = decltype(control){ std::in_place_index<0>, parameterLink_.UIControl };
				parameterLink_.UIControl = std::get<0>(control);
				return variant;
			}
			else
			{
				auto variant = decltype(control){ std::in_place_index<1>, parameterLink_.hostControl };
				parameterLink_.hostControl = std::get<1>(control);
				return variant;
			}
		}

		void addModulator(std::weak_ptr<ParameterModulator> modulator, i64 index = -1)
		{
			COMPLEX_ASSERT(!modulator.expired() && "You're trying to add an empty modulator to parameter");

			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

			if (index < 0) parameterLink_.modulators.emplace_back(std::move(modulator));
			else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, std::move(modulator));

			isDirty_ = true;
		}

		auto updateModulator(std::weak_ptr<ParameterModulator> modulator, size_t index)
		{
			COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

			auto replacedModulator = parameterLink_.modulators[index];
			parameterLink_.modulators[index] = std::move(modulator);
			
			isDirty_ = true;

			return replacedModulator;
		}

		auto deleteModulator(size_t index)
		{
			COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };

			auto deletedModulator = parameterLink_.modulators[index];
			parameterLink_.modulators.erase(parameterLink_.modulators.begin() + (std::ptrdiff_t)index);

			isDirty_ = true;

			return deletedModulator;
		}

		void updateValues(float sampleRate, std::optional<float> value = {}) noexcept;

		float getNormalisedValue() const noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			return normalisedValue_;
		}
		ParameterDetails getParameterDetails() const noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			return details_;
		}
		auto *getParameterLink() noexcept { return &parameterLink_; }

		void setParameterDetails(const ParameterDetails &details) noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			details_ = details;
			isDirty_ = true;
		}

		void setRuntimeDisplayName(std::string name) noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			runtimeStrings_.displayName = std::move(name);
			details_.displayName = runtimeStrings_.displayName;
		}

		void setRuntimeDisplayUnits(std::string units) noexcept
		{
			utils::ScopedLock g{ waitLock_, utils::WaitMechanism::Spin };
			runtimeStrings_.displayUnits = std::move(units);
			details_.displayUnits = runtimeStrings_.displayUnits;
		}

		void setRuntimeStringLookup(const std::vector<std::string> &lookup) noexcept;

		std::any serialiseToJson() const;
		static std::unique_ptr<ParameterValue> deserialiseFromJson(Plugin::ProcessorTree *processorTree, 
			std::any jsonData, std::string_view processorId);

	private:
		struct RuntimeStrings
		{
			std::string displayName{};
			std::string displayUnits{};
			std::string stringLookupData{};
			std::vector<std::string_view> stringLookup{};
		};

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
		RuntimeStrings runtimeStrings_{};

		mutable std::atomic<bool> waitLock_ = false;
		bool isDirty_ = false;
	};
}
