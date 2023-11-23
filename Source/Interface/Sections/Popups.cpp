/*
	==============================================================================

		Popups.cpp
		Created: 2 Feb 2023 7:49:54pm
		Author:  theuser27

	==============================================================================
*/

#include "Popups.h"

namespace Interface
{
	PopupDisplay::PopupDisplay() : BaseSection("Popup Display")
	{
		body_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment);
		border_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleBorderFragment);
		text_ = makeOpenGlComponent<PlainTextComponent>("Popup Text", "");
		
		addOpenGlComponent(body_);
		addOpenGlComponent(border_);
		addOpenGlComponent(text_);

		text_->setJustification(Justification::centred);
		text_->setFontType(PlainTextComponent::kValues);

		setSkinOverride(Skin::kPopupBrowser);
	}

	void PopupDisplay::resized()
	{
		Rectangle<int> bounds = getLocalBounds();
		float rounding = getValue(Skin::kBodyRoundingTop);

		body_->setBounds(bounds);
		body_->setRounding(rounding);
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(rounding);
		border_->setThickness(1.0f, true);
		border_->setColor(getColour(Skin::kBorder));

		text_->setBounds(bounds);
		text_->setColor(getColour(Skin::kNormalText));
	}

	void PopupDisplay::setContent(String text, Rectangle<int> bounds,
		BubbleComponent::BubblePlacement placement, Skin::SectionOverride sectionOverride)
	{
		static constexpr int kHeight = 24;

		setSkinOverride(sectionOverride);

		float height = std::round(scaleValue(kHeight));
		Font font = Fonts::instance()->getDDinFont().withPointHeight(height * 0.5f);
		int padding = (int)height / 4;
		int buffer = padding * 2 + 2;
		int width = font.getStringWidth(text) + buffer;

		int middle_x = bounds.getX() + bounds.getWidth() / 2;
		int middle_y = bounds.getY() + bounds.getHeight() / 2;

		if (placement == BubbleComponent::above)
			setBounds(middle_x - width / 2, bounds.getY() - height, width, height);
		else if (placement == BubbleComponent::below)
			setBounds(middle_x - width / 2, bounds.getBottom(), width, height);
		else if (placement == BubbleComponent::left)
			setBounds(bounds.getX() - width, middle_y - height / 2, width, height);
		else if (placement == BubbleComponent::right)
			setBounds(bounds.getRight(), middle_y - height / 2, width, height);

		text_->setTextHeight(height * 0.5f);
		text_->setText(std::move(text));
	}

	PopupList::PopupList() : BaseSection("Popup List")
	{
		rows_ = makeOpenGlComponent<OpenGlImage>("Popup List Items");
		rows_->setColor(Colours::white);
		rows_->setCustomRenderFunction([this](OpenGlWrapper &openGl, bool animate)
			{
				OpenGlComponent::setViewPort(this, getLocalBounds(), openGl);

				rows_->setTopLeft(-1.0f, 1.0f);
				rows_->setTopRight(1.0f, 1.0f);
				rows_->setBottomLeft(-1.0f, -1.0f);
				rows_->setBottomRight(1.0f, -1.0f);

				rows_->render(openGl, animate);
			});
		addOpenGlComponent(rows_);
		
		highlight_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		highlight_->setTargetComponent(this);
		highlight_->setAdditive(true);
		highlight_->setCustomRenderFunction([this](OpenGlWrapper &openGl, bool animate)
			{
				if (selected_ >= 0 && show_selected_)
				{
					moveQuadToRow(*highlight_, selected_);
					highlight_->setColor(getColour(Skin::kWidgetPrimary1).darker(0.8f));
					highlight_->render(openGl, animate);
				}
			});
		addOpenGlComponent(highlight_);

		hover_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		hover_->setTargetComponent(this);
		hover_->setAdditive(true);
		hover_->setCustomRenderFunction([this](OpenGlWrapper &openGl, bool animate)
			{
				if (hovered_ < 0)
					return;
				
				moveQuadToRow(*hover_, hovered_);
				if (show_selected_)
					hover_->setColor(getColour(Skin::kLightenScreen));
				else
					hover_->setColor(getColour(Skin::kWidgetPrimary1).darker(0.8f));
				hover_->render(openGl, animate);
			});
		addOpenGlComponent(hover_);

		scroll_bar_ = std::make_unique<OpenGlScrollBar>();
		addAndMakeVisible(scroll_bar_.get());
		addOpenGlComponent(scroll_bar_->getGlComponent());
		scroll_bar_->addListener(this);
	}

	void PopupList::resized()
	{
		Colour lighten = getColour(Skin::kLightenScreen);
		scroll_bar_->setColor(lighten);

		if (getScrollableRange() > getHeight())
		{
			int scroll_bar_width = scaleValueRoundInt(kScrollBarWidth);
			int scroll_bar_height = getHeight();
			scroll_bar_->setVisible(true);
			scroll_bar_->setBounds(getWidth() - scroll_bar_width, 0, scroll_bar_width, scroll_bar_height);
			setScrollBarRange();
		}
		else
			scroll_bar_->setVisible(false);

		redoImage();
	}

	void PopupList::setSelections(PopupItems selections)
	{
		selections_ = std::move(selections);
		selected_ = std::min(selected_, (int)selections_.size() - 1);
		hovered_ = std::min(hovered_, (int)selections_.size() - 1);

		for (int i = 0; i < selections_.size(); ++i)
			if (selections_.items[i].selected)
				selected_ = i;
		
		resized();
	}

	int PopupList::getRowFromPosition(float mouse_position)
	{
		int index = std::floor((mouse_position + getViewPosition()) / getRowHeight());
		if (index < selections_.size() && index >= 0 && selections_.items[index].id < 0)
			return -1;
		return index;
	}

	int PopupList::getBrowseWidth() const
	{
		static constexpr int kMinWidth = 150;

		Font font = getFont();
		int max_width = scaleValueRoundInt(kMinWidth);
		int buffer = getTextPadding() * 2 + 2;
		for (int i = 0; i < selections_.size(); ++i)
			max_width = std::max(max_width, font.getStringWidth(selections_.items[i].name) + buffer);

		return max_width;
	}

	void PopupList::mouseMove(const MouseEvent &e)
	{
		int row = getRowFromPosition(e.position.y);
		if (row >= selections_.size() || row < 0)
			row = -1;
		hovered_ = row;
	}

	void PopupList::mouseDrag(const MouseEvent &e)
	{
		int row = getRowFromPosition(e.position.y);
		if (e.position.x < 0 || e.position.x > getWidth() || row >= selections_.size() || row < 0)
			row = -1;
		hovered_ = row;
	}

	void PopupList::mouseExit(const MouseEvent &)
	{ hovered_ = -1; }

	int PopupList::getSelection(const MouseEvent &e)
	{
		float click_y_position = e.position.y;
		int row = getRowFromPosition(click_y_position);
		if (row < selections_.size() && row >= 0)
			return row;

		return -1;
	}

	void PopupList::mouseUp(const MouseEvent &e)
	{
		if (e.position.x < 0 || e.position.x > getWidth())
			return;

		select(getSelection(e));
	}

	void PopupList::mouseDoubleClick(const MouseEvent &e)
	{
		int selection = getSelection(e);
		if (selection != selected_ || selection < 0)
			return;

		for (Listener *listener : listeners_)
			listener->doubleClickedSelected(this, selections_.items[selection].id, selection);
	}

	void PopupList::select(int selection)
	{
		if (selection < 0 || selection >= selections_.size())
			return;

		selected_ = selection;
		for (int i = 0; i < selections_.size(); ++i)
			selections_.items[i].selected = false;
		selections_.items[selected_].selected = true;

		for (Listener *listener : listeners_)
			listener->newSelection(this, selections_.items[selection].id, selection);
	}

	void PopupList::redoImage()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		int rowHeight = getRowHeight();
		int imageWidth = getWidth();
		int imageHeight = std::max(rowHeight * selections_.size(), getHeight());
		Image rowsImage(Image::ARGB, imageWidth, imageHeight, true);
		Graphics g(rowsImage);

		Colour textColor = getColour(Skin::kTextComponentText);
		Colour lighten = getColour(Skin::kLightenScreen);
		int padding = getTextPadding();
		int width = getWidth() - 2 * padding;

		g.setColour(textColor);
		g.setFont(getFont());

		for (int i = 0; i < selections_.size(); ++i)
		{
			if (selections_.items[i].id < 0)
			{
				g.setColour(lighten);
				g.drawRect(padding, (int)std::truncf((float)rowHeight * ((float)i + 0.5f)), width, 1);
			}
			else
			{
				g.setColour(textColor);
				g.drawText(selections_.items[i].name, padding, rowHeight * i,
					width, rowHeight, Justification::centredLeft, true);
			}
		}
		rows_->setOwnImage(std::move(rowsImage));
	}

	void PopupList::moveQuadToRow(OpenGlQuad &quad, int row)
	{
		int row_height = getRowHeight();
		float view_height = getHeight();
		float open_gl_row_height = 2.0f * row_height / view_height;
		float offset = row * open_gl_row_height - 2.0f * getViewPosition() / view_height;

		float y = 1.0f - offset;
		quad.setQuad(0, -1.0f, y - open_gl_row_height, 2.0f, open_gl_row_height);
	}

	void PopupList::resetScrollPosition()
	{
		view_position_ = 0;
		setScrollBarRange();
	}

	void PopupList::mouseWheelMove(const MouseEvent &, const MouseWheelDetails &wheel)
	{
		view_position_ -= wheel.deltaY * kScrollSensitivity;
		view_position_ = std::max(0.0f, view_position_);
		float scaled_height = getHeight();
		int scrollable_range = getScrollableRange();
		view_position_ = std::min(view_position_, 1.0f * scrollable_range - scaled_height);
		setScrollBarRange();
	}

	void PopupList::setScrollBarRange()
	{
		static constexpr float kScrollStepRatio = 0.05f;

		float scaled_height = getHeight();
		scroll_bar_->setRangeLimits(0.0f, getScrollableRange());
		scroll_bar_->setCurrentRange(getViewPosition(), scaled_height, dontSendNotification);
		scroll_bar_->setSingleStepSize(scroll_bar_->getHeight() * kScrollStepRatio);
		scroll_bar_->cancelPendingUpdate();
	}

	int PopupList::getScrollableRange()
	{
		int row_height = getRowHeight();
		int selections_height = row_height * static_cast<int>(selections_.size());
		return std::max(selections_height, getHeight());
	}

	SinglePopupSelector::SinglePopupSelector() : BaseSection("Popup Selector")
	{
		body_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment);
		border_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleBorderFragment);

		addOpenGlComponent(body_);
		addOpenGlComponent(border_);

		popup_list_ = std::make_unique<PopupList>();
		popup_list_->addListener(this);
		addSubSection(popup_list_.get());
		popup_list_->setAlwaysOnTop(true);
		popup_list_->setWantsKeyboardFocus(false);

		setSkinOverride(Skin::kPopupBrowser);
	}

	void SinglePopupSelector::resized()
	{
		BaseSection::resized();

		Rectangle<int> bounds = getLocalBounds();
		int rounding = getValue(Skin::kBodyRoundingTop);
		popup_list_->setBounds(1, rounding, getWidth() - 2, getHeight() - 2 * rounding);

		body_->setBounds(bounds);
		body_->setRounding(getValue(Skin::kBodyRoundingTop));
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(getValue(Skin::kBodyRoundingTop));
		border_->setThickness(1.0f, true);
		border_->setColor(getColour(Skin::kBorder));
	}

	void SinglePopupSelector::setPosition(Point<int> position, Rectangle<int> bounds)
	{
		int rounding = getValue(Skin::kBodyRoundingTop);
		int width = popup_list_->getBrowseWidth();
		int height = popup_list_->getBrowseHeight() + 2 * rounding;
		int x = position.x;
		int y = position.y;
		if (x + width > bounds.getRight())
			x -= width;
		if (y + height > bounds.getBottom())
			y = bounds.getBottom() - height;
		setBounds(x, y, width, height);
	}

	DualPopupSelector::DualPopupSelector() : BaseSection("Dual Popup Selector")
	{
		body_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleFragment);
		border_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kRoundedRectangleBorderFragment);
		divider_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		
		addOpenGlComponent(body_);
		addOpenGlComponent(border_);
		addOpenGlComponent(divider_);

		left_list_ = std::make_unique<PopupList>();
		left_list_->addListener(this);
		addSubSection(left_list_.get());
		left_list_->setAlwaysOnTop(true);
		left_list_->setWantsKeyboardFocus(false);
		left_list_->showSelected(true);

		right_list_ = std::make_unique<PopupList>();
		right_list_->addListener(this);
		addSubSection(right_list_.get());
		right_list_->setAlwaysOnTop(true);
		right_list_->setWantsKeyboardFocus(false);
		right_list_->showSelected(true);

		setSkinOverride(Skin::kPopupBrowser);
	}

	void DualPopupSelector::resized()
	{
		BaseSection::resized();

		Rectangle<int> bounds = getLocalBounds();
		int rounding = getValue(Skin::kBodyRoundingTop);
		int height = getHeight() - 2 * rounding;
		left_list_->setBounds(1, rounding, getWidth() / 2 - 2, height);
		int right_x = left_list_->getRight() + 1;
		right_list_->setBounds(right_x, rounding, getWidth() - right_x - 1, height);

		body_->setBounds(bounds);
		body_->setRounding(getValue(Skin::kBodyRoundingTop));
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(getValue(Skin::kBodyRoundingTop));
		border_->setThickness(1.0f, true);

		divider_->setBounds(getWidth() / 2 - 1, 1, 1, getHeight() - 2);

		Colour border = getColour(Skin::kBorder);
		border_->setColor(border);
		divider_->setColor(border);
	}

	void DualPopupSelector::setPosition(Point<int> position, int width, Rectangle<int> bounds)
	{
		int rounding = (int)getValue(Skin::kBodyRoundingTop);
		int height = left_list_->getBrowseHeight() + 2 * rounding;
		int x = position.x;
		int y = position.y;
		if (x + width > bounds.getRight())
			x -= width;
		if (y + height > bounds.getBottom())
			y = bounds.getBottom() - height;
		setBounds(x, y, width, height);
	}

	void DualPopupSelector::newSelection(PopupList *list, int id, int index)
	{
		if (list != left_list_.get())
		{
			callback_(id);
			return;
		}

		PopupItems right_items = left_list_->getSelectionItems(index);
		if (right_items.size() == 0)
		{
			callback_(id);
			right_list_->setSelections(right_items);
			return;
		}

		int right_selection = right_list_->getSelected();
		if (right_selection < 0 || right_selection >= right_items.size() ||
			right_list_->getSelectionItems(right_selection).name != right_items.items[right_selection].name)
			right_selection = 0;
		

		right_list_->setSelections(right_items);
		right_list_->select(right_selection);      
	}
}
