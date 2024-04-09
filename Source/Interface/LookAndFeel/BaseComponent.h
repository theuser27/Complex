/*
	==============================================================================

		BaseComponent.h
		Created: 11 Dec 2023 4:49:17pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "AppConfig.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "Framework/utils.h"
#include "Framework/sync_primitives.h"

namespace Interface
{
	template<typename T>
	class shared_value
	{
		struct atomic_holder
		{
			T load() const noexcept { return value.load(std::memory_order_relaxed); }
			void store(T newValue) noexcept { value.store(newValue, std::memory_order_relaxed); }

			std::atomic<T> value;
		};

		struct guard_holder
		{
			T load() const noexcept
			{
				utils::ScopedLock g{ guard, utils::WaitMechanism::Spin };
				return value;
			}
			void store(T newValue) noexcept
			{
				utils::ScopedLock g{ guard, utils::WaitMechanism::Spin };
				value = std::move(newValue);
			}

			mutable std::atomic<bool> guard = false;
			T value;
		};

		// if the type cannot fit into an atomic we can have an atomic_bool to guard it
		using holder = std::conditional_t<sizeof(T) <= sizeof(std::uintptr_t), atomic_holder, guard_holder>;  // NOLINT(bugprone-sizeof-expression)
	public:
		shared_value() = default;
		shared_value(const shared_value &other) noexcept { value.store(other.value.load()); }
		shared_value(T newValue) noexcept { value.store(std::move(newValue)); }
		shared_value &operator=(const shared_value &other) noexcept { return shared_value::operator=(other.value.load()); }
		shared_value &operator=(T newValue) noexcept { value.store(std::move(newValue)); return *this; }
		operator T() const noexcept { return get(); }

		[[nodiscard]] T get() const noexcept { return value.load(); }
	private:
		holder value{};
	};

	template<typename T>
	class shared_value<std::unique_ptr<T>>
	{
	public:
		shared_value() = default;
		explicit shared_value(std::unique_ptr<T> &&value) noexcept : value(std::move(value)) { }
		shared_value(shared_value &&other) noexcept
		{
			utils::ScopedLock g{ other.guard, utils::WaitMechanism::Spin, false };
			value = std::move(value.value);
		}
		shared_value &operator=(shared_value &&other) noexcept
		{
			utils::ScopedLock g{ other.guard, utils::WaitMechanism::Spin, false };
			return shared_value::operator=(std::move(other.value));
		}
		shared_value &operator=(std::unique_ptr<T> &&newValue) noexcept
		{
			utils::ScopedLock g{ guard, utils::WaitMechanism::Spin, false };
			value = std::move(newValue);
			return *this;
		}

		[[nodiscard]] std::add_pointer_t<std::remove_all_extents_t<T>> lock() noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::Spin, false);
			return value.get();
		}

		[[nodiscard]] std::add_pointer_t<std::remove_all_extents_t<const T>> lock() const noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::Spin, false);
			return value.get();
		}

		void unlock() const noexcept
		{
			guard.store(false, std::memory_order_release);
			guard.notify_one();
		}
	private:
		mutable std::atomic<bool> guard = false;
		std::unique_ptr<T> value{};
	};

	template<typename T>
	class shared_value<std::vector<T>>
	{
	public:
		shared_value() = default;
		explicit shared_value(std::vector<T> &&value) noexcept { value = std::move(value); }
		shared_value(shared_value &&value) noexcept
		{
			utils::ScopedLock g{ value.guard, utils::WaitMechanism::Spin };
			value = std::move(value.value);
		}
		shared_value &operator=(shared_value &&other) noexcept
		{
			utils::ScopedLock g{ value.guard, utils::WaitMechanism::Spin };
			return shared_value::operator=(std::move(other.value));
		}
		shared_value &operator=(std::vector<T> &&newValue) noexcept
		{
			utils::ScopedLock g{ value.guard, utils::WaitMechanism::Spin };
			value = std::move(newValue);
			return *this;
		}

		[[nodiscard]] std::vector<T> &lock() noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::Spin, false);
			return value;
		}

		[[nodiscard]] const std::vector<T> &lock() const noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::Spin, false);
			return value;
		}

		void unlock() const noexcept
		{
			guard.store(false, std::memory_order_release);
			guard.notify_one();
		}
	private:
		mutable std::atomic<bool> guard = false;
		std::vector<T> value{};
	};

	template<typename T>
	class shared_value_block
	{
	public:
		shared_value_block() = default;
		explicit shared_value_block(std::vector<T> &&value) noexcept { value = std::move(value); }
		shared_value_block(shared_value_block &&other) noexcept
		{
			utils::ScopedLock g{ other.guard, utils::WaitMechanism::Sleep };
			value = std::move(other.value);
		}
		shared_value_block &operator=(shared_value_block &&other) noexcept
		{
			utils::ScopedLock g{ other.guard, utils::WaitMechanism::Sleep };
			return shared_value_block::operator=(std::move(other.value));
		}
		shared_value_block &operator=(T &&newValue) noexcept
		{
			utils::ScopedLock g{ guard, utils::WaitMechanism::WaitNotify };
			value = std::move(newValue);
			return *this;
		}

		[[nodiscard]] T &lock() noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::WaitNotify, false);
			return value;
		}

		[[nodiscard]] const T &lock() const noexcept
		{
			utils::lockAtomic(guard, utils::WaitMechanism::WaitNotify, false);
			return value;
		}

		void unlock() const noexcept
		{
			guard.store(false, std::memory_order_release);
			guard.notify_one();
		}
	private:
		mutable std::atomic<bool> guard = false;
		T value{};
	};

	class BaseComponent : public juce::Component
	{
	public:
		enum RedirectMouse { RedirectMouseWheel, RedirectMouseDown, RedirectMouseDrag, RedirectMouseUp, RedirectMouseMove,
			RedirectMouseEnter, RedirectMouseExit, RedirectMouseDoubleClick };

		BaseComponent(juce::String name = juce::String()) : Component(name) { }

		void parentHierarchyChanged() override;

		// if you get an error here declare setBounds(int, int, int, int) in juce::Component as virtual
		void setBounds(int x, int y, int w, int h) override;
		using Component::setBounds;

		void setVisible(bool shouldBeVisible) override
		{
			isVisibleSafe_ = shouldBeVisible;
			Component::setVisible(shouldBeVisible);
		}

		// if you get an error here declare setAlwaysOnTop(bool) in juce::Component as virtual
		void setAlwaysOnTop(bool shouldStayOnTop) final
		{
			isAlwaysOnTopSafe_ = shouldStayOnTop;
			Component::setAlwaysOnTop(shouldStayOnTop);
		}

		bool isVisibleSafe() const noexcept { return isVisibleSafe_.get(); }
		bool isAlwaysOnTopSafe() const noexcept { return isAlwaysOnTopSafe_.get(); }

		juce::Rectangle<int> getBoundsSafe() const noexcept { return boundsSafe_.get(); }
		juce::Rectangle<int> getLocalBoundsSafe() const noexcept { return boundsSafe_.get().withZeroOrigin(); }
		juce::Point<int> getPositionSafe() const noexcept { return boundsSafe_.get().getPosition(); }

		int getWidthSafe() const noexcept { return getLocalBoundsSafe().getWidth(); }
		int getHeightSafe() const noexcept { return getLocalBoundsSafe().getHeight(); }

		BaseComponent *getParentSafe() const noexcept { return parentSafe_.get(); }
		void setParentSafe(BaseComponent *parent) noexcept { parentSafe_ = parent; }
		
		bool redirectMouse(RedirectMouse type, const juce::MouseEvent &e,
			const juce::MouseWheelDetails *wheel = nullptr, bool findFromParent = true) const;
		bool needsToRedirectMouse(const juce::MouseEvent &e) const noexcept;
		void setRedirectMouseToComponent(BaseComponent *component) { redirectMouse_ = component; }
		void setRedirectMouseModifiers(juce::ModifierKeys redirectMods) { redirectMods_ = redirectMods; }

		// method assumes that component is indeed a child of this
		void clipChildBounds(const BaseComponent *component, juce::Rectangle<int> &bounds) const noexcept;

	protected:
		void addUnclippedChild(BaseComponent *child);
		void removeUnclippedChild(BaseComponent *child);

		shared_value<BaseComponent *> parentSafe_ = nullptr;
		shared_value<juce::Rectangle<int>> boundsSafe_{};
		shared_value<bool> isVisibleSafe_ = false;
		shared_value<bool> isAlwaysOnTopSafe_ = false;
		shared_value<std::vector<BaseComponent *>> unclippedChildren_;

		juce::ModifierKeys redirectMods_{};
		BaseComponent *redirectMouse_ = nullptr;

	private:
		JUCE_DECLARE_WEAK_REFERENCEABLE(BaseComponent)
	};
}
