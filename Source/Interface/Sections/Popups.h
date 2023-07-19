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
		PlainTextComponent text_{ "Popup Text", "" };
		OpenGlQuad body_{ Shaders::kRoundedRectangleFragment };
		OpenGlQuad border_{ Shaders::kRoundedRectangleBorderFragment };
	};

	class PopupList : public BaseSection, ScrollBar::Listener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

			virtual void newSelection(PopupList *list, int id, int index) = 0;
			virtual void doubleClickedSelected(PopupList *list, int id, int index) { }
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
		void paintBackground(Graphics &g) override { }
		void paintBackgroundShadow(Graphics &g) override { }
		void resized() override;

		void setSelections(PopupItems selections);
		PopupItems getSelectionItems(int index) const { return selections_.items[index]; }
		int getRowFromPosition(float mouse_position);
		int getRowHeight() const noexcept { return (int)std::round(scaling_ * kRowHeight); }
		int getTextPadding() const noexcept { return getRowHeight() / 4; }
		int getBrowseWidth() const;
		int getBrowseHeight() const noexcept { return getRowHeight() * selections_.size(); }
		Font getFont() const
		{
			auto fontsInstance = Fonts::instance();
			auto usedFont = fontsInstance->getInterVFont();
			fontsInstance->setFontForAscent(usedFont, (float)getRowHeight() * 0.5f);
			return usedFont;
		}
		int getSelection(const MouseEvent &e);

		void setSelected(int selection) { selected_ = selection; }
		int getSelected() const { return selected_; }
		void mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel) override;
		void resetScrollPosition();
		void scrollBarMoved(ScrollBar *scroll_bar, double range_start) override { view_position_ = range_start; }
		void setScrollBarRange();
		int getScrollableRange();

		void initOpenGlComponents(OpenGlWrapper &open_gl) override;
		void renderOpenGlComponents(OpenGlWrapper &open_gl, bool animate) override;
		void destroyOpenGlComponents(OpenGlWrapper &open_gl) override;
		void addListener(Listener *listener)
		{
			listeners_.push_back(listener);
		}
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
		OpenGlImage rows_;
		OpenGlQuad highlight_{ Shaders::kColorFragment };
		OpenGlQuad hover_{ Shaders::kColorFragment };

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PopupList)
	};

	class SinglePopupSelector : public BaseSection, public PopupList::Listener
	{
	public:
		SinglePopupSelector();

		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) override
		{ BaseSection::renderOpenGlComponents(openGl, animate); }

		void paintBackground(Graphics &g) override {}
		void paintBackgroundShadow(Graphics &g) override {}
		void resized() override;

		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(Point<int> position, Rectangle<int> bounds);

		void newSelection(PopupList *list, int id, int index) override
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

		void focusLost(FocusChangeType cause) override
		{
			setVisible(false);
			if (cancel_)
				cancel_();
		}

		void setCallback(std::function<void(int)> callback) { callback_ = std::move(callback); }
		void setCancelCallback(std::function<void()> cancel) { cancel_ = std::move(cancel); }

		void showSelections(const PopupItems &selections) { popup_list_->setSelections(selections); }

	private:
		OpenGlQuad body_{ Shaders::kRoundedRectangleFragment };
		OpenGlQuad border_{ Shaders::kRoundedRectangleBorderFragment };

		std::function<void(int)> callback_{};
		std::function<void()> cancel_{};
		std::unique_ptr<PopupList> popup_list_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SinglePopupSelector)
	};

	class DualPopupSelector : public BaseSection, public PopupList::Listener
	{
	public:
		DualPopupSelector();

		void paintBackground(Graphics &g) override {}
		void paintBackgroundShadow(Graphics &g) override {}
		void resized() override;
		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(Point<int> position, int width, Rectangle<int> bounds);

		void newSelection(PopupList *list, int id, int index) override;
		void doubleClickedSelected(PopupList *list, int id, int index) override { setVisible(false); }

		void focusLost(FocusChangeType cause) override { setVisible(false); }

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
		OpenGlQuad body_{ Shaders::kRoundedRectangleFragment };
		OpenGlQuad border_{ Shaders::kRoundedRectangleBorderFragment };
		OpenGlQuad divider_{ Shaders::kColorFragment };

		std::function<void(int)> callback_{};
		std::unique_ptr<PopupList> left_list_;
		std::unique_ptr<PopupList> right_list_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DualPopupSelector)
	};

}
