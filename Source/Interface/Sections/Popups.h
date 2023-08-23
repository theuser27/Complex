/*
	==============================================================================

		Popups.h
		Created: 2 Feb 2023 7:49:54pm
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "BaseSection.h"

namespace Interface
{
	class PopupDisplay : public BaseSection
	{
	public:
		PopupDisplay();

		void resized() override;

		void setContent(const std::string &text, Rectangle<int> bounds, 
			BubbleComponent::BubblePlacement placement, Skin::SectionOverride sectionOverride = Skin::kPopupBrowser);

	private:
		gl_ptr<OpenGlQuad> body_;
		gl_ptr<OpenGlQuad> border_;
		gl_ptr<PlainTextComponent> text_;
	};

	class PopupList : public BaseSection, ScrollBar::Listener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

			virtual void newSelection(PopupList *list, int id, int index) = 0;
			virtual void doubleClickedSelected([[maybe_unused]] PopupList *list, 
				[[maybe_unused]] int id, [[maybe_unused]] int index) { }
		};

		static constexpr float kRowHeight = 16.0f;
		static constexpr float kScrollSensitivity = 200.0f;
		static constexpr float kScrollBarWidth = 15.0f;

		PopupList();

		void mouseMove(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseDoubleClick(const MouseEvent &e) override;
		void paintBackground(Graphics &) override { }
		void paintBackgroundShadow(Graphics &) override { }
		void resized() override;

		void setSelections(PopupItems selections);
		PopupItems getSelectionItems(int index) const { return selections_.items[index]; }
		int getRowFromPosition(float mouse_position);
		int getRowHeight() const noexcept { return scaleValueRoundInt(kRowHeight); }
		int getTextPadding() const noexcept { return getRowHeight() / 4; }
		int getBrowseWidth() const;
		int getBrowseHeight() const noexcept { return getRowHeight() * selections_.size(); }
		Font getFont() const
		{
			auto fontsInstance = Fonts::instance();
			Font usedFont = fontsInstance->getInterVFont();
			fontsInstance->setFontFromAscent(usedFont, (float)getRowHeight() * 0.5f);
			return usedFont;
		}
		int getSelection(const MouseEvent &e);

		void setSelected(int selection) { selected_ = selection; }
		int getSelected() const { return selected_; }
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;
		void resetScrollPosition();
		void scrollBarMoved([[maybe_unused]] ScrollBar *scroll_bar, double range_start) override 
		{ view_position_ = (float)range_start; }
		void setScrollBarRange();
		int getScrollableRange();

		void addListener(Listener *listener) { listeners_.push_back(listener); }
		void showSelected(bool show) { show_selected_ = show; }
		void select(int select);

	private:
		int getViewPosition() const
		{
			int view_height = getHeight();
			return std::clamp((int)view_position_, 0, selections_.size() * getRowHeight() - view_height);
		}
		void redoImage();
		void moveQuadToRow(OpenGlQuad &quad, int row);

		std::vector<Listener *> listeners_;
		PopupItems selections_;
		int selected_ = -1;
		int hovered_ = -1;
		bool show_selected_ = false;

		float view_position_ = 0.0f;
		std::unique_ptr<OpenGlScrollBar> scroll_bar_;
		gl_ptr<OpenGlImage> rows_;
		gl_ptr<OpenGlQuad> highlight_;
		gl_ptr<OpenGlQuad> hover_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PopupList)
	};

	class SinglePopupSelector : public BaseSection, public PopupList::Listener
	{
	public:
		SinglePopupSelector();

		void paintBackground(Graphics &) override { }
		void paintBackgroundShadow(Graphics &) override { }
		void resized() override;

		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(Point<int> position, Rectangle<int> bounds);

		void newSelection([[maybe_unused]] PopupList *list, int id, 
			[[maybe_unused]] int index) override
		{
			if (id >= 0)
			{
				cancel_ = {};
				callback_(id);
				setVisible(false);
			}
			else
				cancel_();
		}

		void focusLost(FocusChangeType) override
		{
			setVisible(false);
			if (cancel_)
				cancel_();
		}

		void setCallback(std::function<void(int)> callback) { callback_ = std::move(callback); }
		void setCancelCallback(std::function<void()> cancel) { cancel_ = std::move(cancel); }

		void showSelections(PopupItems selections) { popup_list_->setSelections(std::move(selections)); }

	private:
		gl_ptr<OpenGlQuad> body_;
		gl_ptr<OpenGlQuad> border_;

		std::function<void(int)> callback_{};
		std::function<void()> cancel_{};
		std::unique_ptr<PopupList> popup_list_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SinglePopupSelector)
	};

	class DualPopupSelector : public BaseSection, public PopupList::Listener
	{
	public:
		DualPopupSelector();

		void paintBackground(Graphics &) override { }
		void paintBackgroundShadow(Graphics &) override { }
		void resized() override;
		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(Point<int> position, int width, Rectangle<int> bounds);

		void newSelection(PopupList *list, int id, int index) override;
		void doubleClickedSelected([[maybe_unused]] PopupList *list, 
			[[maybe_unused]] int id, [[maybe_unused]] int index) override { setVisible(false); }

		void focusLost(FocusChangeType) override { setVisible(false); }

		void setCallback(std::function<void(int)> callback) { callback_ = std::move(callback); }

		void showSelections(const PopupItems &selections)
		{
			left_list_->setSelections(selections);

			for (int i = 0; i < selections.size(); ++i)
			{
				if (selections.items[i].selected)
					right_list_->setSelections(selections.items[i]);
			}
		}

	private:
		gl_ptr<OpenGlQuad> body_;
		gl_ptr<OpenGlQuad> border_;
		gl_ptr<OpenGlQuad> divider_;

		std::function<void(int)> callback_{};
		std::unique_ptr<PopupList> left_list_;
		std::unique_ptr<PopupList> right_list_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualPopupSelector)
	};

}
