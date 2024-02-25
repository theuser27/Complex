/*
	==============================================================================

		OpenGlContainer.cpp
		Created: 8 Feb 2024 9:42:09pm
		Author:  theuser27

	==============================================================================
*/

#include "OpenGlContainer.h"
#include "../LookAndFeel/Skin.h"
#include "Plugin/Renderer.h"

namespace Interface
{
	OpenGlContainer::OpenGlContainer(juce::String name) : BaseComponent(std::move(name)) { }

	void OpenGlContainer::destroyAllOpenGlComponents()
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent.deinitialise();
	}

	void OpenGlContainer::addOpenGlComponent(gl_ptr<OpenGlComponent> openGlComponent, bool toBeginning)
	{
		utils::ScopedSpinLock g(isRendering_);
		
		if (!openGlComponent)
			return;

		COMPLEX_ASSERT(std::ranges::find(openGlComponents_, openGlComponent) == openGlComponents_.end()
			&& "We're adding a component that is already a child of this container");

		auto *rawPointer = openGlComponent.get();
		rawPointer->setParentSafe(this);
		rawPointer->setContainer(this);
		if (toBeginning)
			openGlComponents_.insert(openGlComponents_.begin(), std::move(openGlComponent));
		else
			openGlComponents_.emplace_back(std::move(openGlComponent));
		addAndMakeVisible(rawPointer);
	}

	void OpenGlContainer::removeOpenGlComponent(OpenGlComponent *openGlComponent, bool removeChild)
	{
		utils::ScopedSpinLock g(isRendering_);
		
		if (openGlComponent == nullptr)
			return;

		if (std::erase_if(openGlComponents_, [&](auto &&value) { return value.get() == openGlComponent; }))
		{
			removeChildComponent(openGlComponent);
			if (removeChild)
				openGlComponent->setParentSafe(nullptr);
		}
	}

	float OpenGlContainer::getValue(Skin::ValueId valueId) const noexcept { return renderer_->getSkin()->getValue(this, valueId); }
	float OpenGlContainer::getValue(Skin::SectionOverride skinOverride, Skin::ValueId valueId) const noexcept 
	{ return renderer_->getSkin()->getValue(skinOverride, valueId); }
	Colour OpenGlContainer::getColour(Skin::ColorId colorId) const noexcept { return renderer_->getSkin()->getColor(this, colorId); }
	Colour OpenGlContainer::getColour(Skin::SectionOverride skinOverride, Skin::ColorId colorId) const noexcept 
	{ return renderer_->getSkin()->getColor(skinOverride, colorId); }
}
