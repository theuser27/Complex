/*
	==============================================================================

		OpenGlContainer.h
		Created: 8 Feb 2024 9:42:09pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.h"
#include "Framework/open_gl_primitives.h"
#include "Interface/LookAndFeel/Skin.h"

namespace Interface
{
	class Renderer;

	class OpenGlContainer : public BaseComponent
	{
	public:
		OpenGlContainer(juce::String name = {});
		~OpenGlContainer() override;

		virtual void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate = false) = 0;
		virtual void destroyAllOpenGlComponents();

		void addOpenGlComponent(gl_ptr<OpenGlComponent> openGlComponent, bool toBeginning = false);
		void removeOpenGlComponent(OpenGlComponent *openGlComponent, bool removeChild = false);
		void removeAllOpenGlComponents(bool removeChild = false);

		Renderer *getRenderer() const noexcept { return renderer_; }
		float getValue(Skin::ValueId valueId) const noexcept;
		float getValue(Skin::SectionOverride skinOverride, Skin::ValueId valueId) const noexcept;
		juce::Colour getColour(Skin::ColorId colorId) const noexcept;
		juce::Colour getColour(Skin::SectionOverride skinOverride, Skin::ColorId colorId) const noexcept;
		auto getSectionOverride() const noexcept { return skinOverride_; }
		float scaleValue(float value) const noexcept { return scaling_ * value; }
		int scaleValueRoundInt(float value) const noexcept { return (int)std::round(scaling_ * value); }
		// returns the xPosition of the horizontally centered element
		static int centerHorizontally(int xPosition, int elementWidth, int containerWidth) noexcept
		{ return xPosition + (containerWidth - elementWidth) / 2; }
		// returns the yPosition of the vertically centered element
		static int centerVertically(int yPosition, int elementHeight, int containerHeight) noexcept
		{ return yPosition + (containerHeight - elementHeight) / 2; }

		virtual void setSkinOverride(Skin::SectionOverride skinOverride) noexcept { skinOverride_ = skinOverride; }
		virtual void setRenderer(Renderer *renderer) noexcept = 0;
		virtual void setScaling(float scale) noexcept = 0;
	protected:
		std::vector<gl_ptr<OpenGlComponent>> openGlComponents_;

		Renderer *renderer_ = nullptr;
		utils::shared_value<Skin::SectionOverride> skinOverride_ = Skin::kNone;
		utils::shared_value<float> scaling_ = 1.0f;

		std::atomic<bool> isRendering_ = false;
	};
}
