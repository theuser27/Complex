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
	class OpenGlQuad;
	class PlainTextComponent;
	class OpenGlImageComponent;

	class PopupDisplay final : public BaseSection
	{
	public:
		PopupDisplay();
		~PopupDisplay() override;

		void resized() override;

		void setContent(juce::String text, juce::Rectangle<int> bounds, 
			BubblePlacement placement, Skin::SectionOverride sectionOverride = Skin::kPopupBrowser);

	private:
		gl_ptr<OpenGlQuad> body_;
		gl_ptr<OpenGlQuad> border_;
		gl_ptr<PlainTextComponent> text_;
	};

	class PopupList final : public BaseSection, OpenGlScrollBarListener
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void newSelection(PopupList *list, int id, int index) = 0;
		};

		static constexpr float kRowHeight = 16.0f;
		static constexpr float kScrollSensitivity = 200.0f;
		static constexpr float kScrollBarWidth = 15.0f;

		PopupList();
		~PopupList() override;

		void mouseEnter(const juce::MouseEvent &e) override { mouseMove(e); }
		void mouseMove(const juce::MouseEvent &e) override;
		void mouseDrag(const juce::MouseEvent &e) override;
		void mouseExit(const juce::MouseEvent &) override;
		void mouseUp(const juce::MouseEvent &e) override;
		void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &wheel) override;
		void resized() override;

		void setSelections(PopupItems selections);
		PopupItems getSelectionItems(int index) const { return selections_.items[index]; }
		int getRowFromPosition(float mouse_position);
		int getRowHeight() const noexcept { return scaleValueRoundInt(kRowHeight); }
		int getTextPadding() const noexcept { return getRowHeight() / 4; }
		int getBrowseWidth() const;
		int getBrowseHeight() const noexcept { return getRowHeight() * selections_.size(); }
		juce::Font getFont() const;
		int getSelection(const juce::MouseEvent &e);

		void setSelected(int selection) { selected_ = selection; }
		int getSelected() const { return selected_; }
		void scrollBarMoved(OpenGlScrollBar *, double rangeStart) override 
		{ viewPosition_ = (float)rangeStart; }
		void setScrollBarRange();
		int getScrollableRange();

		void addListener(Listener *listener) { listeners_.push_back(listener); }
		void showSelected(bool show) { show_selected_ = show; }
		void select(int select);

	private:
		int getViewPosition() const
		{
			int view_height = getHeightSafe();
			return utils::clamp((int)viewPosition_.get(), 0, selections_.size() * getRowHeight() - view_height);
		}
		void moveQuadToRow(OpenGlQuad &quad, int row);

		std::vector<Listener *> listeners_;
		PopupItems selections_;
		utils::shared_value<int> selected_ = -1;
		utils::shared_value<int> hovered_ = -1;
		utils::shared_value<bool> show_selected_ = false;
		utils::shared_value<float> viewPosition_ = 0.0f;

		std::unique_ptr<OpenGlScrollBar> scroll_bar_;
		gl_ptr<OpenGlImageComponent> rows_;
		gl_ptr<OpenGlQuad> highlight_;
		gl_ptr<OpenGlQuad> hover_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PopupList)
	};

	class SinglePopupSelector final : public BaseSection, public PopupList::Listener
	{
	public:
		SinglePopupSelector();
		~SinglePopupSelector() override;

		void paintBackground(juce::Graphics &) override { }
		void paintBackgroundShadow(juce::Graphics &) override { }
		void resized() override;

		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(juce::Point<int> position, juce::Rectangle<int> bounds);

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
		void setPopupSkinOverride(Skin::SectionOverride skinOverride) { popup_list_->setSkinOverride(skinOverride); }

	private:
		gl_ptr<OpenGlQuad> body_;
		gl_ptr<OpenGlQuad> border_;

		std::function<void(int)> callback_{};
		std::function<void()> cancel_{};
		std::unique_ptr<PopupList> popup_list_;

		JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SinglePopupSelector)
	};

	class DualPopupSelector final : public BaseSection, public PopupList::Listener
	{
	public:
		DualPopupSelector();
		~DualPopupSelector() override;

		void paintBackground(juce::Graphics &) override { }
		void paintBackgroundShadow(juce::Graphics &) override { }
		void resized() override;
		void visibilityChanged() override
		{
			if (isShowing() && isVisible())
				grabKeyboardFocus();
		}

		void setPosition(juce::Point<int> position, int width, juce::Rectangle<int> bounds);

		void newSelection(PopupList *list, int id, int index) override;

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
