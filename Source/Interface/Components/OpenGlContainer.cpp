/*
	==============================================================================

		OpenGlContainer.cpp
		Created: 8 Feb 2024 9:42:09pm
		Author:  theuser27

	==============================================================================
*/

#include "OpenGlComponent.h"

#include "Framework/sync_primitives.h"
#include "OpenGlContainer.h"
#include "Plugin/Renderer.h"

namespace Interface
{
	OpenGlContainer::OpenGlContainer(juce::String name) : BaseComponent(std::move(name)) { }
	OpenGlContainer::~OpenGlContainer() = default;

	void OpenGlContainer::destroyAllOpenGlComponents()
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent.deinitialise();
	}

	void OpenGlContainer::addOpenGlComponent(gl_ptr<OpenGlComponent> openGlComponent, bool toBeginning)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
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
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };
		
		if (openGlComponent == nullptr)
			return;

		if (std::erase_if(openGlComponents_, [&](auto &&value) { return value.get() == openGlComponent; }))
		{
			removeChildComponent(openGlComponent);
			if (removeChild)
				openGlComponent->setParentSafe(nullptr);
		}
	}

	void OpenGlContainer::removeAllOpenGlComponents(bool removeChild)
	{
		utils::ScopedLock g{ isRendering_, utils::WaitMechanism::WaitNotify };

		for (size_t i = openGlComponents_.size(); i > 0; --i)
		{
			removeChildComponent(openGlComponents_[i - 1].get());
			if (removeChild)
				openGlComponents_[i - 1]->setParentSafe(nullptr);
		}

		openGlComponents_.clear();
	}

	float OpenGlContainer::getValue(Skin::ValueId valueId) const noexcept { return renderer_->getSkin()->getValue(this, valueId); }
	float OpenGlContainer::getValue(Skin::SectionOverride skinOverride, Skin::ValueId valueId) const noexcept 
	{ return renderer_->getSkin()->getValue(skinOverride, valueId); }
	juce::Colour OpenGlContainer::getColour(Skin::ColorId colorId) const noexcept { return juce::Colour{ renderer_->getSkin()->getColor(this, colorId) }; }
	juce::Colour OpenGlContainer::getColour(Skin::SectionOverride skinOverride, Skin::ColorId colorId) const noexcept 
	{ return juce::Colour{ renderer_->getSkin()->getColor(skinOverride, colorId) }; }
}
