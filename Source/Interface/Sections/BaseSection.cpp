/*
	==============================================================================

		BaseSection.cpp
		Created: 9 Oct 2022 8:28:36pm
		Author:  theuser27

	==============================================================================
*/

#include "BaseSection.h"

#include "../Components/PresetSelector.h"
#include "MainInterface.h"
#include "../Components/BaseSlider.h"

namespace Interface
{
	BaseSection::BaseSection(std::string_view name) : Component(name.data())
	{
		setWantsKeyboardFocus(true);
	}

	void BaseSection::resized()
	{
		Component::resized();
		if (off_overlay_)
		{
			off_overlay_->setBounds(getLocalBounds());
			off_overlay_->setColor(findColour(Skin::kBackground, true).withMultipliedAlpha(0.8f));
		}
		/*if (activator_)
			activator_->setBounds(getPowerButtonBounds());*/
	}

	void BaseSection::paintBackground(Graphics &g)
	{
		paintBody(g);
		paintKnobShadows(g);
		paintChildrenBackgrounds(g);
		paintBorder(g);
	}

	void BaseSection::setSkinValues(const Skin &skin, bool topLevel)
	{
		skin.applyComponentColors(this, skinOverride_, topLevel);
		skin.applyComponentValues(this, skinOverride_, topLevel);
		for (auto &subSection : subSections_)
			subSection->setSkinValues(skin, false);

		for (auto &openGlComponent : openGlComponents_)
			openGlComponent->setSkinValues(skin);
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
			parent->popupDisplay(source, text, placement, primary);
	}

	void BaseSection::hidePopupDisplay(bool primary)
	{
		if (auto *parent = findParentComponentOfClass<MainInterface>())
			parent->hideDisplay(primary);
	}

	Path BaseSection::getRoundedPath(Rectangle<float> bounds, float topRounding, float bottomRounding) const
	{
		float topRoundness = (topRounding != 0.0f) ? topRounding : findValue(Skin::kBodyRoundingTop);
		float bottomRoundness = (bottomRounding != 0.0f) ? bottomRounding : findValue(Skin::kBodyRoundingBottom);

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
		g.setColour(findColour(Skin::kBody, true));
		g.fillPath(path);
	}

	void BaseSection::paintBorder(Graphics &g, Rectangle<int> bounds, float topRounding, float bottomRounding) const
	{
		auto path = getRoundedPath(bounds.toFloat().reduced(0.5f), topRounding, bottomRounding);
		g.setColour(findColour(Skin::kBorder, true));
		g.strokePath(path, PathStrokeType (1.0f));
	}

	void BaseSection::paintTabShadow(Graphics &g, Rectangle<int> bounds)
	{
		static constexpr float kCornerScale = 0.70710678119f;
		// TODO: redo to match the different rounding, hardcoded to topRounding for now
		int corner_size = findValue(Skin::kBodyRoundingTop);
		int shadow_size = getComponentShadowWidth();
		int corner_and_shadow = corner_size + shadow_size;

		float corner_shadow_offset = corner_size - corner_and_shadow * kCornerScale;
		float corner_ratio = corner_size * 1.0f / corner_and_shadow;

		Colour shadow_color = findColour(Skin::kShadow, true);
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

	void BaseSection::setSizeRatio(float ratio)
	{
		size_ratio_ = ratio;

		for (auto &subSection : subSections_)
			subSection->setSizeRatio(ratio);
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
		g.saveState();

		Rectangle<int> bounds = getLocalArea(child, child->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());
		child->paintBackground(g);

		g.restoreState();
	}

	void BaseSection::paintChildShadow(Graphics &g, BaseSection *child)
	{
		g.saveState();

		Rectangle<int> bounds = getLocalArea(child, child->getLocalBounds());
		g.setOrigin(bounds.getTopLeft());
		child->paintBackgroundShadow(g);
		child->paintChildrenShadows(g);

		g.restoreState();
	}

	void BaseSection::paintOpenGlBackground(Graphics &g, OpenGlComponent *openGlComponent)
	{
		g.saveState();

		Rectangle<int> bounds = getLocalArea(openGlComponent, openGlComponent->getLocalBounds());
		g.reduceClipRegion(bounds);
		g.setOrigin(bounds.getTopLeft());
		openGlComponent->paint(g);

		g.restoreState();
	}

	void BaseSection::drawTextComponentBackground(Graphics &g, Rectangle<int> bounds, bool extend_to_label)
	{
		if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
			return;

		g.setColour(findColour(Skin::kTextComponentBackground, true));
		int y = bounds.getY();
		int rounding = bounds.getHeight() / 2;

		if (extend_to_label)
		{
			int label_bottom = bounds.getBottom() + findValue(Skin::kTextComponentLabelOffset);
			rounding = findValue(Skin::kLabelBackgroundRounding);
			g.fillRoundedRectangle(bounds.toFloat(), rounding);

			int extend_y = y + bounds.getHeight() / 2;
			g.fillRect(bounds.getX(), extend_y, bounds.getWidth(), label_bottom - extend_y - rounding);
		}
		else
			g.fillRoundedRectangle(bounds.toFloat(), rounding);
	}

	void BaseSection::initOpenGlComponents(OpenGlWrapper &open_gl)
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent->init(open_gl);

		for (auto &subSection : subSections_)
			subSection->initOpenGlComponents(open_gl);
	}

	void BaseSection::renderOpenGlComponents(OpenGlWrapper &open_gl, bool animate)
	{
		// TODO: change order of current and subSections if draw order is switched

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && !openGlComponent->isAlwaysOnTop())
			{
				openGlComponent->render(open_gl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &subSection : subSections_)
		{
			if (subSection->isVisible() && !subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(open_gl, animate);
		}

		for (auto &openGlComponent : openGlComponents_)
		{
			if (openGlComponent->isVisible() && openGlComponent->isAlwaysOnTop())
			{
				openGlComponent->render(open_gl, animate);
				COMPLEX_ASSERT(juce::gl::glGetError() == juce::gl::GL_NO_ERROR);
			}
		}

		for (auto &subSection : subSections_)
		{
			if (subSection->isVisible() && subSection->isAlwaysOnTop())
				subSection->renderOpenGlComponents(open_gl, animate);
		}
	}

	void BaseSection::destroyOpenGlComponents(OpenGlWrapper &open_gl)
	{
		for (auto &openGlComponent : openGlComponents_)
			openGlComponent->destroy(open_gl);

		for (auto &subSection : subSections_)
			subSection->destroyOpenGlComponents(open_gl);
	}

	void BaseSection::guiChanged(BaseButton *button)
	{
		if (button == activator_)
			setActive(activator_->getToggleStateValue().getValue());
	}

	void BaseSection::addButton(BaseButton *button, bool show)
	{
		buttons_[button->getName().toStdString()] = button;
		button->addListener(this);
		if (show)
			addAndMakeVisible(button);
		addOpenGlComponent(button->getGlComponent());
	}

	void BaseSection::addSlider(BaseSlider *slider, bool show, bool listen)
	{
		sliders_[slider->getName().toRawUTF8()] = slider;
		if (listen)
			slider->addListener(this);
		if (show)
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

	void BaseSection::addSubSection(BaseSection *subSection, bool show)
	{
		subSection->setParent(this);

		if (show)
			addAndMakeVisible(subSection);

		subSections_.push_back(subSection);
	}

	void BaseSection::removeSubSection(BaseSection *section)
	{
		auto location = std::find(subSections_.begin(), subSections_.end(), section);
		if (location != subSections_.end())
			subSections_.erase(location);
	}

	void BaseSection::addOpenGlComponent(OpenGlComponent *openGlComponent, bool toBeginning)
	{
		if (openGlComponent == nullptr)
			return;

		COMPLEX_ASSERT(std::find(openGlComponents_.begin(), openGlComponents_.end(),
			openGlComponent) == openGlComponents_.end());

		openGlComponent->setParent(this);
		if (toBeginning)
			openGlComponents_.insert(openGlComponents_.begin(), openGlComponent);
		else
			openGlComponents_.push_back(openGlComponent);
		addAndMakeVisible(openGlComponent);
	}

	void BaseSection::setActivator(GeneralButton *activator)
	{
		createOffOverlay();

		activator_ = activator;
		activator->setPowerButton();
		activator_->getGlComponent()->setAlwaysOnTop(true);
		activator->addButtonListener(this);
		setActive(activator_->getToggleStateValue().getValue());
	}

	void BaseSection::createOffOverlay()
	{
		if (off_overlay_)
			return;

		off_overlay_ = std::make_unique<OffOverlay>();
		addOpenGlComponent(off_overlay_.get(), true);
		off_overlay_->setVisible(false);
		off_overlay_->setAlwaysOnTop(true);
		off_overlay_->setInterceptsMouseClicks(false, false);
	}

	void BaseSection::paintJointControlBackground(Graphics &g, int x, int y, int width, int height)
	{
		float rounding = findValue(Skin::kLabelBackgroundRounding);
		g.setColour(findColour(Skin::kLabelBackground, true));
		g.fillRect(x + rounding, y * 1.0f, width - 2.0f * rounding, height / 2.0f);

		int label_height = findValue(Skin::kLabelBackgroundHeight);
		int half_label_height = label_height / 2;
		int side_width = height;
		g.setColour(findColour(Skin::kTextComponentBackground, true));
		g.fillRoundedRectangle(x, y + half_label_height, width, height - half_label_height, rounding);
		g.fillRoundedRectangle(x, y, side_width, height, rounding);
		g.fillRoundedRectangle(x + width - side_width, y, side_width, height, rounding);

		Colour label_color = findColour(Skin::kLabelBackground, true);
		if (label_color.getAlpha() == 0)
			label_color = findColour(Skin::kBody, true);
		g.setColour(label_color);
		int rect_width = std::max(width - 2 * side_width, 0);
		g.fillRect(x + side_width, y, rect_width, half_label_height);
		g.fillRoundedRectangle(x + side_width, y, rect_width, label_height, rounding);
	}

	void BaseSection::paintJointControl(Graphics &g, int x, int y, int width, int height, const std::string &name)
	{
		paintJointControlBackground(g, x, y, width, height);

		setLabelFont(g);
		g.setColour(findColour(Skin::kBodyText, true));
		g.drawText(name, x, y, width, findValue(Skin::kLabelBackgroundHeight), Justification::centred, false);
	}

	void BaseSection::placeJointControls(int x, int y, int width, int height,
		BaseSlider *left, BaseSlider *right, Component *widget)
	{
		int width_control = height;
		left->setBounds(x, y, width_control, height);

		if (widget)
		{
			int label_height = findValue(Skin::kLabelBackgroundHeight);
			widget->setBounds(x + width_control, y + label_height, width - 2 * width_control, height - label_height);
		}
		right->setBounds(x + width - width_control, y, width_control, height);
	}

	void BaseSection::placeRotaryOption(Component *option, BaseSlider *rotary)
	{
		int width = findValue(Skin::kRotaryOptionWidth);
		int offset_x = findValue(Skin::kRotaryOptionXOffset) - width / 2;
		int offset_y = findValue(Skin::kRotaryOptionYOffset) - width / 2;
		Point<int> point = rotary->getBounds().getCentre() + Point<int>(offset_x, offset_y);
		option->setBounds(point.x, point.y, width, width);
	}

	void BaseSection::placeKnobsInArea(Rectangle<int> area, std::vector<Component *> knobs)
	{
		int widget_margin = findValue(Skin::kWidgetMargin);
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
		int slider_width = std::floor(LinearSlider::kLinearWidthPercent * total_width * 0.5f) * 2.0f + extra;
		return (total_width - slider_width) / 2;
	}

	/*Rectangle<int> BaseSection::getPowerButtonBounds() const noexcept
	{
		int title_width = getTitleWidth();
		return Rectangle<int>(getPowerButtonOffset(), 0, title_width, title_width);
	}*/

	float BaseSection::getDisplayScale() const
	{
		if (getWidth() <= 0)
			return 1.0f;

		Component *top_level = getTopLevelComponent();
		Rectangle<int> global_bounds = top_level->getLocalArea(this, getLocalBounds());
		float display_scale = Desktop::getInstance().getDisplays().getDisplayForRect(top_level->getScreenBounds())->scale;
		return display_scale * (1.0f * global_bounds.getWidth()) / getWidth();
	}

	Font BaseSection::getLabelFont() const noexcept
	{
		float height = findValue(Skin::kLabelHeight);
		return Fonts::instance()->getInterMediumFont().withPointHeight(height);
	}

	void BaseSection::setLabelFont(Graphics &g)
	{
		g.setColour(findColour(Skin::kBodyText, true));
		g.setFont(getLabelFont());
	}

	void BaseSection::drawLabelConnectionForComponents(Graphics &g, Component *left, Component *right)
	{
		int label_offset = findValue(Skin::kLabelOffset);
		int background_height = findValue(Skin::kLabelBackgroundHeight);
		g.setColour(findColour(Skin::kLabelConnection, true));
		int background_y = left->getBounds().getBottom() - background_height + label_offset;

		int rect_width = right->getBounds().getCentreX() - left->getBounds().getCentreX();
		g.fillRect(left->getBounds().getCentreX(), background_y, rect_width, background_height);
	}

	void BaseSection::drawLabelBackground(Graphics &g, Rectangle<int> bounds, bool text_component)
	{
		int background_rounding = findValue(Skin::kLabelBackgroundRounding);
		g.setColour(findColour(Skin::kLabelBackground, true));
		Rectangle<float> label_bounds = getLabelBackgroundBounds(bounds, text_component).toFloat();
		g.fillRoundedRectangle(label_bounds, background_rounding);
		if (text_component && !findColour(Skin::kTextComponentBackground, true).isTransparent())
			g.fillRect(label_bounds.withHeight(label_bounds.getHeight() / 2));
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
		int background_height = findValue(Skin::kLabelBackgroundHeight);
		int label_offset = text_component ? findValue(Skin::kTextComponentLabelOffset) : findValue(Skin::kLabelOffset);
		int background_y = bounds.getBottom() - background_height + label_offset;
		return Rectangle<int>(bounds.getX(), background_y, bounds.getWidth(), background_height);
	}

	void BaseSection::drawLabel(Graphics &g, String text, Rectangle<int> component_bounds, bool text_component)
	{
		if (component_bounds.getWidth() <= 0 || component_bounds.getHeight() <= 0)
			return;

		drawLabelBackground(g, component_bounds, text_component);
		g.setColour(findColour(Skin::kBodyText, true));
		Rectangle<int> background_bounds = getLabelBackgroundBounds(component_bounds, text_component);
		g.drawText(text, component_bounds.getX(), background_bounds.getY(),
			component_bounds.getWidth(), background_bounds.getHeight(), Justification::centred, false);
	}

	void BaseSection::drawTextBelowComponent(Graphics &g, String text, Component *component, int space, int padding)
	{
		int height = findValue(Skin::kLabelBackgroundHeight);
		g.drawText(text, component->getX() - padding, component->getBottom() + space,
			component->getWidth() + 2 * padding, height, Justification::centred, false);
	}

	void BaseSection::setActive(bool active)
	{
		if (active_ == active)
			return;

		if (off_overlay_)
			off_overlay_->setVisible(!active);

		active_ = active;
		for (auto &slider : sliders_)
			slider.second->setActive(active);
		for (auto &subSection : subSections_)
			subSection->setActive(active);

		repaintBackground();
	}

	void BaseSection::updateAllValues()
	{
		for (auto &slider : sliders_)
			slider.second->updateValue();

		for (auto &button : buttons_)
			button.second->updateValue();

		for (auto &subSection : subSections_)
			subSection->updateAllValues();
	}

}
