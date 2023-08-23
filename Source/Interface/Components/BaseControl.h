/*
	==============================================================================

		BaseControl.h
		Created: 31 Jul 2023 7:37:15pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/parameter_value.h"
#include "Interface/LookAndFeel/Skin.h"
#include "Interface/Components/OpenGlImageComponent.h"

namespace Interface
{
	class BaseSection;

	class BaseControl
	{
	public:
		BaseControl() = default;
		virtual ~BaseControl();

		Framework::ParameterDetails getParameterDetails() const noexcept { return details_; }
		virtual void setParameterDetails(const Framework::ParameterDetails &details) { details_ = details; }

		Framework::ParameterLink *getParameterLink() const noexcept { return parameterLink_; }
		// returns the replaced link
		Framework::ParameterLink *setParameterLink(Framework::ParameterLink *parameterLink) noexcept;

		Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
			bool getValueFromParameter = true);

		String getName() const { return name_; }
		void setName(String name) { name_ = std::move(name); }

		void setValueFromHost() noexcept;
		void setValueFromParameter() noexcept;
		void setValueToHost() const noexcept;
		void setValueToParameter() const noexcept;

		// called on the UI thread from:
		// - on a timer in MainInterface to watch for updates from the host
		// - the UndoManager when undo/redo happens
		bool updateValue();

		double getValueSafe() const noexcept { return value_.load(std::memory_order_acquire); }
		double getValueSafeScaled(float sampleRate = kDefaultSampleRate) const noexcept
		{ return Framework::scaleValue(getValueSafe(), details_, sampleRate, true); }
		void setValueSafe(double newValue) noexcept { value_.store(newValue, std::memory_order_release); }

		// TODO: get rid of these when you have this inherit from Component
		virtual double getValueInternal() const noexcept = 0;
		virtual void setValueInternal(double value, NotificationType notification) noexcept = 0;
		virtual void setBoundsInternal(Rectangle<int> bounds) = 0;

		void beginChange(double oldValue) noexcept;
		void endChange();

		bool hasParameter() const noexcept { return hasParameter_; }

		virtual void addListener([[maybe_unused]] BaseSection *listener) { }
		virtual void removeListener([[maybe_unused]] BaseSection *listener) { }


		auto getDrawBounds() const noexcept { return drawBounds_; }
		auto getComponents() noexcept { return std::span{ components_ }; }
		auto getAddedHitBox() const noexcept { return addedHitBox_; }
		// returns tight bounds around all of the contained elements (drawn components, label, etc)
		// by the end of this method drawBounds need to have been set to encompass the drawn components
		[[nodiscard]] virtual Rectangle<int> getOverallBoundsForHeight(int height) = 0;
		// sets the normal bounds based on drawBounds + position + added sizes
		void setOverallBounds(Point<int> position);
		void setAddedHitBox(BorderSize<int> addedHitBox) noexcept { addedHitBox_ = addedHitBox; }

		virtual void positionExtraElements(Rectangle<int> anchorBounds) = 0;
		void addLabel();
		void removeLabel();
		auto getLabelComponent() noexcept { return label_; }

		virtual void refresh() = 0;
		virtual void drawShadow(Graphics &) { }

		float findValue(Skin::ValueId valueId) const;
		Colour getColour(Skin::ColorId colourId) const noexcept;
		auto *getParentSection() noexcept { return parent_; }

		bool isActive() const noexcept { return isActive_; }

	protected:
		Rectangle<int> getUnionOfAllElements() const noexcept
		{
			auto bounds = drawBounds_;
			for (size_t i = 0; i < extraElements_.data.size(); i++)
				bounds = bounds.getUnion(extraElements_[i]);				
			return bounds;
		}

		std::atomic<double> value_ = 0.0;
		double valueBeforeChange_ = 0.0;
		bool hasBegunChange_ = false;

		bool hasParameter_ = false;
		bool isActive_ = true;

		Framework::ParameterLink *parameterLink_ = nullptr;
		Framework::ParameterDetails details_{};
		String name_{};

		// drawBounds is a space inside getLocalBounds() where components are being drawn
		Rectangle<int> drawBounds_{};
		// this determines how much to extend the bounds relative to drawBounds so that the hitbox is larger
		BorderSize<int> addedHitBox_{};
		std::vector<gl_ptr<OpenGlComponent>> components_{};

		// extra stuff like label or modifying control (i.e. textSelector changing behaviour of a knob)
		// and their bounds relative to the drawBounds
		Framework::VectorMap<Component *, Rectangle<int>> extraElements_{};
		gl_ptr<PlainTextComponent> label_ = nullptr;
		BubbleComponent::BubblePlacement labelPlacement_ = BubbleComponent::right;

		BaseSection *parent_ = nullptr;
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
			name(std::move(name)), id(id), selected(selected), isActive(active) { }

		void addItem(int subId, std::string subName, bool subSelected = false, bool active = false)
		{ items.emplace_back(subId, std::move(subName), subSelected, active); }

		void addItem(const PopupItems &item) { items.push_back(item); }
		void addItem(PopupItems &&item) { items.emplace_back(std::move(item)); }
		int size() const noexcept { return (int)items.size(); }
	};
}
