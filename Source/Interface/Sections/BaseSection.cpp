/*
	==============================================================================

		BaseSection.cpp
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#include "BaseSection.h"

#include "MainInterface.h"

namespace Interface
{
	BaseSection::BaseSection(std::string_view name) : Component(name.data())
	{
		setWantsKeyboardFocus(true);
	}

	void BaseSection::resized()
	{
		Component::resized();
		if (offOverlay_)
		{
			offOverlay_->setBounds(getLocalBounds());
			offOverlay_->setColor(getColour(Skin::kBackground).withMultipliedAlpha(0.8f));
		}
		if (activator_)
			activator_->setBounds(getPowerButtonBounds());
	}

	void BaseSection::paintBackground(Graphics &g)
	{
		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
	}

	void BaseSection::repaintBackground()
	{
		if (!isShowing())
			return;

		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->repaintChildBackground(this);
	}

	void BaseSection::showPopupSelector(Component *source, Point<int> position, const PopupItems &options,
		std::function<void(int)> callback, std::function<void()> cancel)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->popupSelector(source, position, options, callback, cancel);
	}

	void BaseSection::showPopupDisplay(Component *source, const std::string &text,
		BubbleComponent::BubblePlacement placement, bool primary)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->popupDisplay(source, text, placement, primary, getSectionOverride());
	}

	void BaseSection::hidePopupDisplay(bool primary)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->hideDisplay(primary);
	}

	Path BaseSection::getRoundedPath(Rectangle<float> bounds, float topRounding, float bottomRounding) const
	{
		float topRoundness = (topRounding != 0.0f) ? topRounding : getValue(Skin::kBodyRoundingTop);
		float bottomRoundness = (bottomRounding != 0.0f) ? bottomRounding : getValue(Skin::kBodyRoundingBottom);

		auto x = bounds.getX();
		auto y = bounds.getY();
		auto width = bounds.getWidth();
		auto height = bounds.getHeight();

		Path path{};
		path.startNewSubPath(x + width - topRoundness, y);
		path.quadraticTo(x + width, y, x + width, y + topRoundness);
		path.lineTo(x + width, y + height - bottomRoundness);
		path.quadraticTo(x + width, y + height, x + width - bottomRoundness, y + height);
		path.lineTo(x + bottomRoundness, y + height);
		path.quadraticTo(x, y + height, x, y + height - bottomRoundness);
		path.lineTo(x, y + topRoundness);
		path.quadraticTo(x, y, x + topRoundness, y);
		path.closeSubPath();

		return path;
	}

	void BaseSection::paintBody(Graphics &g, Rectangle<int> bounds, float topRounding, float bottomRounding) const
	{
		auto path = getRoundedPath(bounds.toFloat(), topRounding, bottomRounding);
		g.setColour(getColour(Skin::kBody));
		g.fillPath(path);
	}

	void BaseSection::paintBorder(Graphics &g, Rectangle<int> bounds, float topRounding, float bottomRounding) const
	{
		auto path = getRoundedPath(bounds.toFloat().reduced(0.5f), topRounding, bottomRounding);
		g.setColour(getColour(Skin::kBorder));
		g.strokePath(path, PathStrokeType (1.0f));
	}

	void BaseSection::paintTabShadow(Graphics &g, Rectangle<int> bounds)
	{
		static constexpr float kCornerScale = 0.70710678119f;
		// TODO: redo to match the different rounding, hardcoded to topRounding for now
		int corner_size = getValue(Skin::kBodyRoundingTop);
		int shadow_size = getComponentShadowWidth();
		int corner_and_shadow = corner_size + shadow_size;

		float corner_shadow_offset = corner_size - corner_and_shadow * kCornerScale;
		float corner_ratio = corner_size * 1.0f / corner_and_shadow;

		Colour shadow_color = getColour(Skin::kShadow);
		Colour transparent = shadow_color.withAlpha(0.0f);

		int left = bounds.getX();
		int top = bounds.getY();
		int right = bounds.getRight();
		int bottom = bounds.getBottom();

		g.setGradientFill(ColourGradient(shadow_color, left, 0, transparent, left - shadow_size, 0, false));
		g.fillRect(left - shadow_size, top + corner_size, shadow_size, bottom - top - corner_size * 2);

		g.setGradientFill(ColourGradient(shadow_color, right, 0, transparent, right + shadow_size, 0, false));
		g.fillRect(right, top + corner_size, shadow_size, bottom - top - corner_size * 2);

		g.setGradientFill(ColourGradient(shadow_color, 0, top, transparent, 0, top - shadow_size, false));
		g.fillRect(left + corner_size, top - shadow_size, right - left - corner_size * 2, shadow_size);

		g.setGradientFill(ColourGradient(shadow_color, 0, bottom, transparent, 0, bottom + shadow_size, false));
		g.fillRect(left + corner_size, bottom, right - left - corner_size * 2, shadow_size);

		ColourGradient top_left_corner(shadow_color, left + corner_size, top + corner_size,
			transparent, left + corner_shadow_offset, top + corner_shadow_offset, true);
		top_left_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(top_left_corner);
		g.fillRect(left - shadow_size, top - shadow_size, corner_and_shadow, corner_and_shadow);

		ColourGradient top_right_corner(shadow_color, right - corner_size, top + corner_size,
			transparent, right - corner_shadow_offset, top + corner_shadow_offset, true);
		top_right_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(top_right_corner);
		g.fillRect(right - corner_size, top - shadow_size, corner_and_shadow, corner_and_shadow);

		ColourGradient bottom_left_corner(shadow_color, left + corner_size, bottom - corner_size,
			transparent, left + corner_shadow_offset, bottom - corner_shadow_offset, true);
		bottom_left_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(bottom_left_corner);
		g.fillRect(left - shadow_size, bottom - corner_size, corner_and_shadow, corner_and_shadow);

		ColourGradient bottom_right_corner(shadow_color, right - corner_size, bottom - corner_size,
			transparent, right - corner_shadow_offset, bottom - corner_shadow_offset, true);
		bottom_right_corner.addColour(corner_ratio, shadow_color);
		g.setGradientFill(bottom_right_corner);
		g.fillRect(right - corner_size, bottom - corner_size, corner_and_shadow, corner_and_shadow);
	}

	void BaseSection::setScaling(float scale)
	{
		scaling_ = scale;

		for (auto &subSection : subSections_)
			subSection->setScaling(scale);
	}

	void BaseSection::reset()
	{
		for (auto &subSection : subSections_)
			subSection->reset();
	}

	void BaseSection::paintKnobShadows(Graphics &g)
	{
		for (auto &slider : sliders_)
			if (slider.second->isVisible() && slider.second->getWidth() && slider.second->getHeight())
				slider.second->drawShadow(g);
	}

	void BaseSection::paintChildrenShadows(Graphics &g)
	{
		for (auto &subSection : subSections_)
			if (subSection->isVisible())
				paintChildShadow(g, subSection);
	}

	void BaseSection::paintOpenGlChildrenBackgrounds(Graphics &g)
	{
		for (auto &openGlComponent : openGlComponents_)
			if (openGlComponent->isVisible())
				paintOpenGlBackground(g, openGlComponent);
	}

	void BaseSection::paintChildrenBackgrounds(Graphics &g)
	{
		for (auto &subSection : subSections_)
			if (subSection->isVisible())
				paintChildBackground(g, subSection);

		paintOpenGlChildrenBackgrounds(g);
	}

	void BaseSection::paintChildBackground(Graphics &g, BaseSection *child)
	{
		Graphics::ScopedSaveState s(g);

		Rectangle<int> bounds = getLocalArea(child, child->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());
		child->paintBackground(g);
	}

	void BaseSection::paintChildShadow(Graphics &g, BaseSection *child)
	{
		Graphics::ScopedSaveState s(g);

		Rectangle<int> bounds = getLocalArea(child, child->getLocalBounds());
		g.setOrigin(bounds.getTopLeft());
		child->paintBackgroundShadow(g);
		child->paintChildrenShadows(g);
	}

	void BaseSection::paintOpenGlBackground(Graphics &g, OpenGlComponent *openGlComponent)
	{
		Graphics::ScopedSaveState s(g);

		Rectangle<int> bounds = getLocalArea(openGlComponent, openGlComponent->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());
		openGlComponent->paint(g);
	}

	void BaseSection::drawTextComponentBackground(Graphics &g, Rectangle<int> bounds, bool extend_to_label)
	{
		if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
			return;

		g.setColour(getColour(Skin::kTextComponentBackground));
		int y = bounds.getY();
		int rounding = bounds.getHeight() / 2;

		if (extend_to_label)
		{
			int label_bottom = bounds.getBottom() + getValue(Skin::kTextComponentLabelOffset);
			rounding = getValue(Skin::kLabelBackgroundRounding);
			g.fillRoundedRectangle(bounds.toFloat(), rounding);

			int extend_y = y + bounds.getHeight() / 2;
			g.fillRect(bounds.getX(), extend_y, bounds.getWidth(), label_bottom - extend_y - rounding);
		}
		else
			g.fillRoundedRectangle(bounds.toFloat(), rounding);
	}

	void BaseSection::initOpenGlComponents(OpenGlWrapper &openGl)
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent->init(openGl);

		for (auto &subSection : subSections_)
			subSection->initOpenGlComponents(openGl);
	}

	void BaseSection::renderOpenGlComponents(OpenGlWrapper &openGl, bool animate)
	{
		// TODO: change order of current and subSections if draw order is switched

		for (auto &subSection : subSections_)
		{
			if (subSection->isVisible() && !subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(openGl, animate);
		}

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && !openGlComponent->isAlwaysOnTop())
			{
				openGlComponent->render(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &subSection : subSections_)
		{
			if (subSection->isVisible() && subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(openGl, animate);
		}

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && openGlComponent->isAlwaysOnTop())
			{
				openGlComponent->render(openGl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}
	}

	void BaseSection::destroyOpenGlComponents(OpenGlWrapper &openGl)
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent->destroy(openGl);

		for (auto &subSection : subSections_)
			subSection->destroyOpenGlComponents(openGl);
	}

	void BaseSection::guiChanged(BaseButton *button)
	{
		if (button == activator_)
			setActive(activator_->getToggleStateValue().getValue());
	}

	void BaseSection::resizeForText(TextSelector *textSelector, int requestedWidthChange)
	{
		auto currentBounds = textSelector->getBounds();
		auto currentDrawBox = textSelector->getDrawBox();
		textSelector->setBoundsAndDrawBounds(currentBounds.withWidth(currentBounds.getWidth() + requestedWidthChange),
			currentDrawBox.withWidth(currentDrawBox.getWidth() + requestedWidthChange));
	}

	void BaseSection::addSubSection(BaseSection *subSection, bool show)
	{
		subSection->setParent(this);
		subSection->setSkin(skin_);

		if (show)
			addAndMakeVisible(subSection);

		subSections_.push_back(subSection);
	}

	void BaseSection::removeSubSection(BaseSection *section)
	{
		auto location = std::ranges::find(subSections_.begin(), subSections_.end(), section);
		if (location != subSections_.end())
			subSections_.erase(location);
	}

	void BaseSection::addButton(BaseButton *button)
	{
		if (!button)
			return;

		buttons_[button->getName().toRawUTF8()] = button;
		button->addListener(this);

		addAndMakeVisible(button);
		addOpenGlComponent(button->getGlComponent());
	}

	void BaseSection::removeButton(BaseButton *button)
	{
		if (!button)
			return;

		removeOpenGlComponent(button->getGlComponent());
		removeChildComponent(button);

		button->removeListener(this);
		buttons_.erase(button->getName().toRawUTF8());
	}

	void BaseSection::addSlider(BaseSlider *slider)
	{
		if (!slider)
			return;

		sliders_[slider->getName().toRawUTF8()] = slider;
		slider->addListener(this);

		addAndMakeVisible(slider);
		if (slider->isImageOnTop())
		{
			addOpenGlComponent(slider->getQuadComponent());
			addOpenGlComponent(slider->getImageComponent());
		}
		else
		{
			addOpenGlComponent(slider->getImageComponent());
			addOpenGlComponent(slider->getQuadComponent());
		}
		addOpenGlComponent(slider->getTextEditorComponent());
	}

	void BaseSection::removeSlider(BaseSlider *slider)
	{
		if (!slider)
			return;

		removeOpenGlComponent(slider->getQuadComponent());
		removeOpenGlComponent(slider->getImageComponent());
		removeOpenGlComponent(slider->getTextEditorComponent());
		removeChildComponent(slider);

		slider->removeListener(this);
		sliders_.erase(slider->getName().toRawUTF8());
	}

	void BaseSection::addOpenGlComponent(OpenGlComponent *openGlComponent, bool toBeginning)
	{
		if (!openGlComponent)
			return;

		COMPLEX_ASSERT(std::ranges::find(openGlComponents_.begin(), openGlComponents_.end(),
			openGlComponent) == openGlComponents_.end());

		openGlComponent->setParent(this);
		if (toBeginning)
			openGlComponents_.insert(openGlComponents_.begin(), openGlComponent);
		else
			openGlComponents_.push_back(openGlComponent);
		addAndMakeVisible(openGlComponent);
	}

	void BaseSection::removeOpenGlComponent(OpenGlComponent *openGlComponent)
	{
		if (openGlComponent == nullptr)
			return;

		auto erasedElementsSize = std::erase(openGlComponents_, openGlComponent);
		if (erasedElementsSize)
			removeChildComponent(openGlComponent);
	}

	void BaseSection::setActivator(BaseButton *activator)
	{
		createOffOverlay();

		activator_ = activator;
		activator->setPowerButton();
		activator->addButtonListener(this);
		setActive(activator_->getToggleState());
	}

	void BaseSection::createOffOverlay()
	{
		if (offOverlay_)
			return;

		offOverlay_ = std::make_unique<OffOverlay>();
		addOpenGlComponent(offOverlay_.get(), true);
		offOverlay_->setVisible(false);
		offOverlay_->setAlwaysOnTop(true);
		offOverlay_->setInterceptsMouseClicks(false, false);
	}

	void BaseSection::placeRotaryOption(Component *option, BaseSlider *rotary)
	{
		int width = getValue(Skin::kRotaryOptionWidth);
		int offset_x = getValue(Skin::kRotaryOptionXOffset) - width / 2;
		int offset_y = getValue(Skin::kRotaryOptionYOffset) - width / 2;
		Point<int> point = rotary->getBounds().getCentre() + Point<int>(offset_x, offset_y);
		option->setBounds(point.x, point.y, width, width);
	}

	void BaseSection::placeKnobsInArea(Rectangle<int> area, std::vector<Component *> knobs)
	{
		int widget_margin = getValue(Skin::kWidgetMargin);
		float component_width = (area.getWidth() - (knobs.size() + 1) * widget_margin) / (1.0f * knobs.size());

		int y = area.getY();
		int height = area.getHeight() - widget_margin;
		float x = area.getX() + widget_margin;
		for (Component *knob : knobs)
		{
			int left = std::round(x);
			int right = std::round(x + component_width);
			if (knob)
				knob->setBounds(left, y, right - left, height);
			x += component_width + widget_margin;
		}
	}

	float BaseSection::getSliderOverlap() const noexcept
	{
		int total_width = getSliderWidth();
		int extra = total_width % 2;
		int slider_width = (int)(std::floor(LinearSlider::kLinearWidthPercent * total_width * 0.5f) * 2.0f) + extra;
		return (total_width - slider_width) / 2;
	}

	float BaseSection::getDisplayScale() const
	{
		if (getWidth() <= 0)
			return 1.0f;

		Component *topLevelComponent = getTopLevelComponent();
		float globalWidth = (float)topLevelComponent->getLocalArea(this, getLocalBounds()).getWidth();
		float displayScale = (float)Desktop::getInstance().getDisplays()
			.getDisplayForRect(topLevelComponent->getScreenBounds())->scale;
		return displayScale * globalWidth / (float)getWidth();
	}

	Font BaseSection::getLabelFont() const noexcept
	{
		float height = getValue(Skin::kLabelHeight);
		return Fonts::instance()->getInterVFont().withPointHeight(height);
	}

	void BaseSection::setLabelFont(Graphics &g)
	{
		g.setColour(getColour(Skin::kNormalText));
		g.setFont(getLabelFont());
	}

	Rectangle<int> BaseSection::getDividedAreaUnbuffered(Rectangle<int> full_area, int num_sections, int section, int buffer)
	{
		float component_width = (full_area.getWidth() - (num_sections + 1) * buffer) / (1.0f * num_sections);
		int x = full_area.getX() + std::round(section * (component_width + buffer) + buffer);
		int right = full_area.getX() + std::round((section + 1.0f) * (component_width + buffer));
		return Rectangle<int>(x, full_area.getY(), right - x, full_area.getHeight());
	}

	Rectangle<int> BaseSection::getDividedAreaBuffered(Rectangle<int> full_area, int num_sections, int section, int buffer)
	{
		Rectangle<int> area = getDividedAreaUnbuffered(full_area, num_sections, section, buffer);
		return area.expanded(buffer, 0);
	}

	Rectangle<int> BaseSection::getLabelBackgroundBounds(Rectangle<int> bounds, bool text_component)
	{
		int background_height = getValue(Skin::kLabelBackgroundHeight);
		int label_offset = text_component ? getValue(Skin::kTextComponentLabelOffset) : getValue(Skin::kLabelOffset);
		int background_y = bounds.getBottom() - background_height + label_offset;
		return Rectangle<int>(bounds.getX(), background_y, bounds.getWidth(), background_height);
	}

	void BaseSection::drawLabel(Graphics &g, String text, Rectangle<int> component_bounds, bool text_component)
	{
		if (component_bounds.getWidth() <= 0 || component_bounds.getHeight() <= 0)
			return;

		//drawLabelBackground(g, component_bounds, text_component);
		setLabelFont(g);
		g.setColour(getColour(Skin::kNormalText));
		Rectangle<int> background_bounds = getLabelBackgroundBounds(component_bounds, text_component);
		g.drawText(text, component_bounds.getX(), background_bounds.getY(),
			component_bounds.getWidth(), background_bounds.getHeight(), Justification::centred, false);
	}

	void BaseSection::drawSliderLabel(Graphics &g, BaseSlider *slider) const
	{
		Graphics::ScopedSaveState s(g);

		auto *label = slider->getLabelComponent();
		g.setOrigin(label->getPosition());
		label->paint(g);
	}

	void BaseSection::drawButtonLabel(Graphics &g, BaseButton *button) const
	{
		Graphics::ScopedSaveState s(g);

		auto *label = button->getLabelComponent();
		auto rectangle = label->getBounds();
		g.setFont(label->getFont());
		g.setColour(getColour(Skin::kNormalText));
		g.drawText(label->getLabelText(), rectangle, Justification::centred, false);
	}

	void BaseSection::drawSliderBackground(Graphics &g, BaseSlider *slider) const
	{
		Graphics::ScopedSaveState s(g);
		slider->paint(g);
	}

	void BaseSection::drawTextBelowComponent(Graphics &g, String text, Component *component, int space, int padding)
	{
		int height = getValue(Skin::kLabelBackgroundHeight);
		g.drawText(text, component->getX() - padding, component->getBottom() + space,
			component->getWidth() + 2 * padding, height, Justification::centred, false);
	}

	void BaseSection::setActive(bool active)
	{
		if (active_ == active)
			return;

		if (offOverlay_)
			offOverlay_->setVisible(!active);

		active_ = active;
		for (auto &slider : sliders_)
			slider.second->setActive(active);

		repaintBackground();
	}

	void BaseSection::updateAllValues()
	{
		for (auto &slider : sliders_)
			slider.second->updateValueFromParameter();

		for (auto &button : buttons_)
			button.second->updateValueFromParameter();

		for (auto &subSection : subSections_)
			subSection->updateAllValues();
	}

}
