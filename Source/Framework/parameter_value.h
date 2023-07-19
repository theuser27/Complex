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
			utils::ScopedSpinLock g(other.waitLock_);
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

		void initialise(std::optional<float> value = {})
		{
			utils::ScopedSpinLock lock(waitLock_);

			normalisedValue_ = value.value_or(details_.defaultNormalisedValue);
			modulations_ = 0.0f;
			normalisedInternalValue_ = value.value_or(details_.defaultNormalisedValue);
			internalValue_ = (!value.has_value()) ? details_.defaultValue :
				(float)Framework::scaleValue(value.value(), details_);
		}
		
		// prefer calling this only once if possible
		template<commonConcepts::ParameterRepresentation T>
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

		auto changeControl(std::variant<Interface::ParameterUI *, ParameterBridge *> control) noexcept
		{
			utils::ScopedSpinLock lock(waitLock_);
			
			if (std::holds_alternative<Interface::ParameterUI *>(control))
			{
				auto variant = std::variant<Interface::ParameterUI *,
					ParameterBridge *>{ std::in_place_index<0>, parameterLink_.UIControl };
				parameterLink_.UIControl = std::get<0>(control);
				return variant;
			}
			else
			{
				auto variant = std::variant<Interface::ParameterUI *,
					ParameterBridge *>{ std::in_place_index<1>, parameterLink_.hostControl };
				parameterLink_.hostControl = std::get<1>(control);
				return variant;
			}
		}

		void addModulator(std::weak_ptr<ParameterModulator> modulator, i64 index = -1)
		{
			COMPLEX_ASSERT(!modulator.expired() && "You're trying to add an empty modulator to parameter");

			utils::ScopedSpinLock lock(waitLock_);

			if (index < 0) parameterLink_.modulators.emplace_back(std::move(modulator));
			else parameterLink_.modulators.emplace(parameterLink_.modulators.begin() + index, std::move(modulator));
		}

		auto updateModulator(std::weak_ptr<ParameterModulator> modulator, size_t index)
		{
			COMPLEX_ASSERT(!modulator.expired() && "You're updating with an empty modulator");

			utils::ScopedSpinLock lock(waitLock_);

			auto replacedModulator = parameterLink_.modulators[index];
			parameterLink_.modulators[index] = std::move(modulator);
			return replacedModulator;
		}

		auto deleteModulator(size_t index)
		{
			COMPLEX_ASSERT(index < parameterLink_.modulators.size() && "You're have given an index that's too large");

			utils::ScopedSpinLock lock(waitLock_);

			auto deletedModulator = parameterLink_.modulators[index];
			parameterLink_.modulators.erase(parameterLink_.modulators.begin() + (std::ptrdiff_t)index);
			return deletedModulator;
		}

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

		ParameterLink parameterLink_{ nullptr, nullptr, {}, this };

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

		Framework::ParameterDetails getParameterDetails() const noexcept { return details_; }
		virtual void setParameterDetails(const Framework::ParameterDetails &details) { details_ = details; }

		Framework::ParameterLink *getParameterLink() const noexcept { return parameterLink_; }
		// returns the replaced link
		Framework::ParameterLink *setParameterLink(Framework::ParameterLink *parameterLink) noexcept
		{
			auto replacedLink = parameterLink_;
			if (replacedLink)
				replacedLink->parameter->changeControl(std::variant<ParameterUI *,
					Framework::ParameterBridge *>(std::in_place_index<0>, nullptr));

			parameterLink_ = parameterLink;

			if (parameterLink_)
				parameterLink_->parameter->changeControl(this);

			return replacedLink;
		}

		void setValueFromHost(double newValue) noexcept { setValueSafe(newValue); }
		void setValueToHost() noexcept;

		// called on the UI thread from:
		// - on a timer in MainInterface to watch for updates from the host
		// - the UndoManager when undo/redo happens
		virtual bool updateValue() = 0;

		double getValueSafe() const noexcept
		{ return value_.load(std::memory_order_acquire); }

		double getValueSafeScaled(float sampleRate = kDefaultSampleRate) const noexcept
		{ return Framework::scaleValue(getValueSafe(), details_, sampleRate, true); }

		void setValueSafe(double newValue) noexcept
		{ value_.store(newValue, std::memory_order_release); }

		void beginChange(double oldValue) noexcept { valueBeforeChange_ = oldValue; hasBegunChange_ = true; }
		virtual void endChange() = 0;

		bool hasParameter() const noexcept { return hasParameter_; }

	protected:
		std::atomic<double> value_ = 0.0;
		double valueBeforeChange_ = 0.0;
		bool hasBegunChange_ = false;

		bool hasParameter_ = false;

		Framework::ParameterLink *parameterLink_ = nullptr;
		Framework::ParameterDetails details_{};
	};

	struct PopupItems
	{
		std::vector<PopupItems> items{};
		std::string name{};
		int id = 0;
		bool selected = false;
		bool isActive = false;

		PopupItems() = default;
		PopupItems(std::string name) : name(std::move(name)) { }

		PopupItems(int id, std::string name, bool selected = false, bool active = false) :
			name(std::move(name)), id(id), selected(selected), isActive(active)
		{
		}

		void addItem(int subId, std::string subName, bool subSelected = false, bool active = false)
		{
			items.emplace_back(subId, std::move(subName), subSelected, active);
		}

		void addItem(const PopupItems &item) { items.push_back(item); }
		void addItem(PopupItems &&item) { items.emplace_back(std::move(item)); }
		int size() const noexcept { return (int)items.size(); }
	};
}
