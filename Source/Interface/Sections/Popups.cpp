/*
	==============================================================================

		Popups.cpp
		Created: 2 Feb 2023 7:49:54pm
		Author:  theuser27

	==============================================================================
*/

#include "Popups.h"

#include "../LookAndFeel/Fonts.h"
#include "../Components/OpenGlImageComponent.h"
#include "../Components/OpenGlScrollBar.h"

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

	PopupDisplay::~PopupDisplay() = default;

	void PopupDisplay::resized()
	{
		Rectangle<int> bounds = getLocalBounds();
		float rounding = getValue(Skin::kBodyRoundingTop);

		body_->setBounds(bounds);
		body_->setRounding(rounding);
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(rounding);
		border_->setThickness(1.0f);
		border_->setColor(getColour(Skin::kBorder));

		text_->setBounds(bounds);
		text_->setColor(getColour(Skin::kNormalText));
	}

	void PopupDisplay::setContent(String text, Rectangle<int> bounds,
		BubblePlacement placement, Skin::SectionOverride sectionOverride)
	{
		static constexpr int kHeight = 24;

		setSkinOverride(sectionOverride);

		float floatHeight = std::round(scaleValue(kHeight));
		int height = (int)floatHeight;
		Font font = Fonts::instance()->getDDinFont().withPointHeight(floatHeight * 0.5f);
		int padding = height / 4;
		int buffer = padding * 2 + 2;
		int width = font.getStringWidth(text) + buffer;

		int middle_x = bounds.getX() + bounds.getWidth() / 2;
		int middle_y = bounds.getY() + bounds.getHeight() / 2;

		if (placement == BubblePlacement::above)
			setBounds(middle_x - width / 2, bounds.getY() - height, width, height);
		else if (placement == BubblePlacement::below)
			setBounds(middle_x - width / 2, bounds.getBottom(), width, height);
		else if (placement == BubblePlacement::left)
			setBounds(bounds.getX() - width, middle_y - height / 2, width, height);
		else if (placement == BubblePlacement::right)
			setBounds(bounds.getRight(), middle_y - height / 2, width, height);

		text_->setTextHeight(floatHeight * 0.5f);
		text_->setText(std::move(text));
	}

	PopupList::PopupList() : BaseSection("Popup List")
	{
		rows_ = makeOpenGlComponent<OpenGlImageComponent>("Popup List Items");
		rows_->setColor(Colours::white);
		rows_->setTargetComponent(this);
		rows_->setPaintFunction([this](Graphics &g)
			{
				int rowHeight = getRowHeight();
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
			});
		addOpenGlComponent(rows_);
		
		highlight_ = makeOpenGlComponent<OpenGlQuad>(Shaders::kColorFragment);
		highlight_->setTargetComponent(this);
		highlight_->setAdditive(true);
		highlight_->setRenderFunction([this](OpenGlWrapper &openGl, bool animate)
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
		hover_->setRenderFunction([this](OpenGlWrapper &openGl, bool animate)
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

		scroll_bar_ = std::make_unique<OpenGlScrollBar>(true);
		addAndMakeVisible(scroll_bar_.get());
		addOpenGlComponent(scroll_bar_->getGlComponent());
		scroll_bar_->addListener(this);
	}

	PopupList::~PopupList() = default;

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

		rows_->setCustomDrawBounds({ 0, 0, getWidth(),
			std::max(getRowHeight() * selections_.size(), getHeight()) });
		rows_->redrawImage();
	}

	void PopupList::setSelections(PopupItems selections)
	{
		selections_ = std::move(selections);
		selected_ = std::min(selected_.get(), (int)selections_.size() - 1);
		hovered_ = std::min(hovered_.get(), (int)selections_.size() - 1);

		for (int i = 0; i < selections_.size(); ++i)
			if (selections_.items[i].selected)
				selected_ = i;
		
		resized();
	}

	int PopupList::getRowFromPosition(float mousePosition)
	{
		int index = (int)std::floor((mousePosition + (float)getViewPosition()) / (float)getRowHeight());
		if (index < selections_.size() && index >= 0 && selections_.items[index].id < 0)
			return -1;
		return index;
	}

	int PopupList::getBrowseWidth() const
	{
		static constexpr int kPopupMinWidth = 150;

		Font font = getFont();
		int max_width = scaleValueRoundInt(kPopupMinWidth);
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
		if (e.position.x < 0.0f || e.position.x > (float)getWidth() || row >= selections_.size() || row < 0)
			row = -1;
		hovered_ = row;
	}

	void PopupList::mouseExit(const MouseEvent &)
	{ hovered_ = -1; }

	Font PopupList::getFont() const
	{
		auto fontsInstance = Fonts::instance();
		Font usedFont = fontsInstance->getInterVFont();
		fontsInstance->setFontFromAscent(usedFont, (float)getRowHeight() * 0.5f);
		return usedFont;
	}

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
		if (e.position.x < 0 || e.position.x > (float)getWidth())
			return;

		select(getSelection(e));
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

	void PopupList::moveQuadToRow(OpenGlQuad &quad, int row)
	{
		int rowHeight = getRowHeight();
		float viewHeight = (float)getHeightSafe();
		float openGlRowHeight = 2.0f * rowHeight / viewHeight;
		float offset = (float)row * openGlRowHeight - 2.0f * (float)getViewPosition() / viewHeight;

		float y = 1.0f - offset;
		quad.setQuad(0, -1.0f, y - openGlRowHeight, 2.0f, openGlRowHeight);
	}

	void PopupList::mouseWheelMove(const MouseEvent &, const MouseWheelDetails &wheel)
	{
		viewPosition_ = viewPosition_.get() - wheel.deltaY * kScrollSensitivity;
		viewPosition_ = std::max(0.0f, viewPosition_.get());
		viewPosition_ = std::min(viewPosition_.get(), 1.0f * (float)getScrollableRange() - (float)getHeight());
		setScrollBarRange();
	}

	void PopupList::setScrollBarRange()
	{
		static constexpr double kScrollStepRatio = 0.05f;

		float scaledHeight = (float)getHeight();
		scroll_bar_->setRangeLimits(0.0f, getScrollableRange());
		scroll_bar_->setCurrentRange(getViewPosition(), scaledHeight, dontSendNotification);
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

	SinglePopupSelector::~SinglePopupSelector() = default;

	void SinglePopupSelector::resized()
	{
		BaseSection::resized();

		Rectangle<int> bounds = getLocalBounds();
		int rounding = (int)getValue(Skin::kBodyRoundingTop);
		popup_list_->setBounds(1, rounding, getWidth() - 2, getHeight() - 2 * rounding);

		body_->setBounds(bounds);
		body_->setRounding(getValue(Skin::kBodyRoundingTop));
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(getValue(Skin::kBodyRoundingTop));
		border_->setThickness(1.0f);
		border_->setColor(getColour(Skin::kBorder));
	}

	void SinglePopupSelector::setPosition(Point<int> position, Rectangle<int> bounds)
	{
		int rounding = (int)getValue(Skin::kBodyRoundingTop);
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

	DualPopupSelector::~DualPopupSelector() = default;

	void DualPopupSelector::resized()
	{
		BaseSection::resized();

		Rectangle<int> bounds = getLocalBounds();
		int rounding = (int)getValue(Skin::kBodyRoundingTop);
		int height = getHeight() - 2 * rounding;
		left_list_->setBounds(1, rounding, getWidth() / 2 - 2, height);
		int right_x = left_list_->getRight() + 1;
		right_list_->setBounds(right_x, rounding, getWidth() - right_x - 1, height);

		body_->setBounds(bounds);
		body_->setRounding(getValue(Skin::kBodyRoundingTop));
		body_->setColor(getColour(Skin::kBody));

		border_->setBounds(bounds);
		border_->setRounding(getValue(Skin::kBodyRoundingTop));
		border_->setThickness(1.0f);

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
