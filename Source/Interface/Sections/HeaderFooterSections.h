/*
	==============================================================================

		HeaderFooterSections.h
		Created: 23 May 2023 6:00:44am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "Generation/SoundEngine.h"
#include "BaseSection.h"

namespace Interface
{
	class HeaderFooterSections : public BaseSection
	{
	public:
		static constexpr int kHeaderHeight = 40;
		static constexpr int kHeaderHorizontalEdgePadding = 10;
		static constexpr int kHeaderNumberBoxMargin = 12;

		static constexpr int kFooterHeight = 24;
		static constexpr int kFooterHorizontalEdgePadding = 16;

		static constexpr int kLabelToControlMargin = 4;

		HeaderFooterSections(Generation::SoundEngine &soundEngine);

		void paintBackground(Graphics &g) override;
		void resized() override;
		void sliderValueChanged(Slider *movedSlider) override;
		void resizeForText([[maybe_unused]] TextSelector *textSelector, 
			[[maybe_unused]] int requestedWidthChange) override { }

		void arrangeHeader();
		void arrangeFooter();

	private:
		std::unique_ptr<NumberBox> mixNumberBox_;
		std::unique_ptr<NumberBox> gainNumberBox_;
		std::unique_ptr<NumberBox> blockSizeNumberBox_;
		std::unique_ptr<NumberBox> overlapNumberBox_;
		std::unique_ptr<TextSelector> windowTypeSelector_;
		std::unique_ptr<NumberBox> windowAlphaNumberBox_;

		gl_ptr<OpenGlQuad> backgroundColour_;
		gl_ptr<OpenGlQuad> bottomBarColour_;

		bool showAlpha_ = false;
	};
}
