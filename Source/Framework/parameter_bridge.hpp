
// Created: 2022-07-28 04:21:59

#pragma once

#include "satomi.hpp"
#include "stl_utils.hpp"
#include "memory.hpp"

namespace Plugin
{
	struct State;
}

namespace Framework
{
	struct ParameterLink;

	class ParameterBridge
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() { }
			virtual void parameterLinkReset(ParameterBridge *bridge,
				ParameterLink *newLink, ParameterLink *oldLink) = 0;
		};
		
		static constexpr float kDefaultParameterValue = 0.5f;

		ParameterBridge() = delete;
		ParameterBridge(const ParameterBridge &) = delete;
		ParameterBridge(ParameterBridge &&) = delete;
		ParameterBridge &operator=(const ParameterBridge &) = delete;
		ParameterBridge &operator=(ParameterBridge &&) = delete;

		ParameterBridge(Plugin::State *state,
			u64 parameterIndex = u64(-1), ParameterLink *link = nullptr) noexcept;
		~ParameterBridge() noexcept;

		auto *getParameterLink() const noexcept { return parameterLinkPointer_.load(satomi::memory_order_acquire); }
		void resetParameterLink(ParameterLink *link, bool getValueFromParameter = true) noexcept;
		static void notifyParameterChange() noexcept;

		void beginChangeGesture();
		void setValueFromUI(float newValue) noexcept;
		void endChangeGesture();

		void updateUIParameter() noexcept;

		void setCustomName(utils::string name);

		bool isMappedToParameter() const noexcept { return parameterLinkPointer_.load(satomi::memory_order_acquire); }

		float getValue() const { return value_.load(satomi::memory_order_relaxed); }
		void setValue(float newValue)
		{
			auto oldValue = value_.exchange(newValue, satomi::memory_order_relaxed);
			if (oldValue != newValue)
				wasValueSet_.store(true, satomi::memory_order_relaxed);
		}
		float getDefaultValue() const;

		utils::string getName() const;
		utils::string getName(int maximumStringLength) const;
		void getName(char *buffer, usize maximumStringLength) const;
		utils::string getLabel() const;
		void getText(float value, char *buffer, usize maximumStringLength) const;
		float getValueForText(utils::string_view text) const;

		bool isAutomatable() const { return true; }
		int getNumSteps() const;
		bool isDiscrete() const;
		bool isBoolean() const;

		void addListener(Listener *listener) { listeners_.emplace_back(listener); }
		void removeListener(Listener *listener) noexcept { listeners_.erase(listener); }

		const u64 parameterIndex;
	private:
		utils::pair<satomi::atomic<bool>, utils::string> name_{};
		satomi::atomic<float> value_ = kDefaultParameterValue;
		satomi::atomic<bool> wasValueSet_ = false;
		satomi::atomic<ParameterLink *> parameterLinkPointer_ = nullptr;

		Plugin::State *state = nullptr;
		utils::vector<Listener *> listeners_{};
	};
}