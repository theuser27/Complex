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
		static constexpr int kHeaderNumberBoxPadding = 6;
		static constexpr int kLabelToControlMargin = 4;
		static constexpr int kHorizontalEdgePadding = 4;
		static constexpr int kFooterHeight = 24;
		static constexpr int kFooterHorizontalEdgePadding = 16;

		static constexpr int kNumberBoxHeight = 16;

		HeaderFooterSections(Generation::SoundEngine &soundEngine);

		void paintBackground(Graphics &g) override;
		void resized() override;
		void renderOpenGlComponents(OpenGlWrapper &openGl, bool animate) override
		{ BaseSection::renderOpenGlComponents(openGl, animate); }

		void arrangeHeader();
		void arrangeFooter();

	private:
		std::unique_ptr<NumberBox> mixNumberBox_;
		std::unique_ptr<NumberBox> gainNumberBox_;
		std::unique_ptr<NumberBox> blockSizeNumberBox_;
		std::unique_ptr<NumberBox> overlapNumberBox_;
		std::unique_ptr<TextSelector> windowTypeSelector_;
	};
}
