/*
	==============================================================================

		BaseControl.h
		Created: 31 Jul 2023 7:37:15pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Framework/vector_map.h"
#include "Framework/parameters.h"
#include "OpenGlContainer.h"

namespace Framework
{
	class ParameterValue;
	struct ParameterLink;
}

namespace Interface
{
	class PlainTextComponent;
	class BaseSection;

	class BaseControl : public OpenGlContainer
	{
	public:
		BaseControl();
		~BaseControl() override;

		// ====================================================== Parameter related
		Framework::ParameterDetails getParameterDetails() const noexcept { return details_; }
		virtual void setParameterDetails(const Framework::ParameterDetails &details) { details_ = details; }

		Framework::ParameterLink *getParameterLink() const noexcept { return parameterLink_; }
		// returns the replaced link
		Framework::ParameterLink *setParameterLink(Framework::ParameterLink *parameterLink) noexcept;

		virtual Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
			bool getValueFromParameter = true);

		bool setValueFromHost() noexcept;
		void setValueFromParameter() noexcept;
		void setValueToHost() const noexcept;
		void setValueToParameter() const noexcept;

		double getValueSafe() const noexcept { return value_.load(std::memory_order_acquire); }
		double getValueSafeScaled(float sampleRate = kDefaultSampleRate) const noexcept
		{ return Framework::scaleValue(getValueSafe(), details_, sampleRate, true); }
		void setValueSafe(double newValue) noexcept { value_.store(newValue, std::memory_order_release); }
		virtual void valueChanged() = 0;

		void beginChange(double oldValue) noexcept;
		void endChange();

		bool hasParameter() const noexcept { return hasParameter_; }

		virtual void addListener([[maybe_unused]] BaseSection *listener) { }
		virtual void removeListener([[maybe_unused]] BaseSection *listener) { }

		// ========================================================= Layout related
		void resized() override;
		void moved() override;
		void parentHierarchyChanged() override;

		auto getDrawBounds() const noexcept { return drawBounds_; }
		auto getAddedHitbox() const noexcept { return addedHitbox_; }
		
		// returns tight bounds around all contained elements (drawn components, label, etc.)
		// by the end of this method drawBounds need to have been set to encompass the drawn components
		virtual juce::Rectangle<int> setBoundsForSizes(int height, int width = 0) = 0;
		// sets the bounds based on drawBounds + position + added hitbox
		// call after initialising drawBounds with setBoundsForSizes
		void setPosition(juce::Point<int> position);
		void setBounds(int x, int y, int width, int height) final;
		using OpenGlContainer::setBounds;
		// sets extra outer size
		void setAddedHitbox(juce::BorderSize<int> addedHitBox) noexcept { addedHitbox_ = addedHitBox; }
		// positions extra external elements (label, combined control, etc.) relative to drawBounds
		virtual void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) = 0;
		void repositionExtraElements();

		// ====================================================== Rendering related
		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) final;
		void paint(juce::Graphics &) override { }
		// redraws components when an action occurs
		virtual void redoImage() = 0;
		// sets positions of all drawable components relative to drawBounds
		// drawBounds is guaranteed to be a valid area
		virtual void setComponentsBounds() = 0;
		// refreshes colours from Skin
		virtual void setColours() = 0;

		// ========================================================== Label related
		void addLabel();
		void removeLabel();
		
		// ========================================================== Miscellaneous
		using OpenGlContainer::getValue;
		bool isActive() const noexcept { return isActive_; }

		void setRenderer(Renderer *renderer) noexcept override final { renderer_ = renderer; }
		void setScaling(float scale) noexcept override final { scaling_ = scale; }
		void setLabelPlacement(juce::BubbleComponent::BubblePlacement placement) { labelPlacement_ = placement; }
		void setShouldRepaintOnHover(bool shouldRepaintOnHover) noexcept { shouldRepaintOnHover_ = shouldRepaintOnHover; }

	protected:
		juce::Rectangle<int> getUnionOfAllElements() const noexcept
		{
			auto bounds = drawBounds_;
			for (size_t i = 0; i < extraElements_.data.size(); i++)
				bounds = bounds.getUnion(extraElements_[i]);				
			return bounds;
		}

		// ============================================================== Variables
		std::atomic<double> value_ = 0.0;
		double valueBeforeChange_ = 0.0;
		bool hasBegunChange_ = false;

		bool hasParameter_ = false;
		bool isActive_ = true;

		Framework::ParameterLink *parameterLink_ = nullptr;
		Framework::ParameterDetails details_{};

		// drawBounds is a space inside getLocalBounds() where components are being drawn
		juce::Rectangle<int> drawBounds_{};
		bool isDrawBoundsSet_ = false;
		// this determines how much to extend the bounds relative to drawBounds so that the hitbox is larger
		juce::BorderSize<int> addedHitbox_{};

		// extra stuff like label or modifying control (i.e. textSelector changing behaviour of a knob)
		// and their bounds relative to the drawBounds
		Framework::VectorMap<BaseComponent *, juce::Rectangle<int>> extraElements_{};
		gl_ptr<PlainTextComponent> label_;
		juce::BubbleComponent::BubblePlacement labelPlacement_ = juce::BubbleComponent::right;

		bool shouldRepaintOnHover_ = true;

		BaseSection *parent_ = nullptr;
	};


}
