/*
	==============================================================================

		Skin.h
		Created: 16 Nov 2022 6:55:11am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <array>
#include <any>
#include <string>
#include "Framework/platform_definitions.h"
#include "Framework/vector_map.h"

namespace juce
{
	class File;
	class String;
	class Colour;
	class LookAndFeel;
}

namespace Interface
{
	class OpenGlContainer;
	class MainInterface;
	class BaseSection;

	class Skin
	{
	public:
		enum SectionOverride
		{
			kNone,
			kOverlay,
			kEffectsLane,
			kPopupBrowser,
			kFilterModule,
			kDynamicsModule,
			kPhaseModule,

			kSectionsCount,
			kUseParentOverride = kSectionsCount
		};

		enum ValueId
		{
			kBodyRoundingTop,
			kBodyRoundingBottom,

			kWidgetLineWidth,
			kWidgetLineBoost,
			kWidgetFillCenter,
			kWidgetFillFade,
			kWidgetFillBoost,
			kWidgetMargin,
			kWidgetRoundedCorner,

			kLabelHeight,
			kLabelBackgroundHeight,
			kLabelBackgroundRounding,
			kLabelOffset,
			kTextComponentLabelOffset,

			kRotaryOptionXOffset,
			kRotaryOptionYOffset,
			kRotaryOptionWidth,

			kPadding,
			kLargePadding,
			kSliderWidth,

			kTextComponentHeight,
			kTextComponentOffset,
			kTextComponentFontSize,
			kTextButtonHeight,
			kButtonFontSize,

			kKnobArcSize,
			kKnobArcThickness,
			kKnobBodySize,
			kKnobHandleLength,

			kKnobModAmountArcSize,
			kKnobModAmountArcThickness,
			kKnobModMeterArcSize,
			kKnobModMeterArcThickness,

			kKnobOffset,
			kKnobSectionHeight,
			kKnobShadowWidth,
			kKnobShadowOffset,

			kModulationButtonWidth,
			kModulationFontSize,

			kValueIdCount
		};

		enum ColorId
		{
			kInitialColor = 0x42345678,
			kBackground = kInitialColor,
			kBody,
			kBackgroundElement,
			kHeadingText,
			kNormalText,
			kBorder,

			kWidgetPrimary1,
			kWidgetPrimary2,
			kWidgetPrimaryDisabled,
			kWidgetSecondary1,
			kWidgetSecondary2,
			kWidgetSecondaryDisabled,
			kWidgetAccent1,
			kWidgetAccent2,
			kWidgetBackground1,
			kWidgetBackground2,
			kWidgetCenterLine,

			kOverlayScreen,
			kLightenScreen,
			kShadow,
			kPopupSelectorBackground,
			kPopupBackground,
			kPopupBorder,

			kTextComponentBackground,
			kTextComponentText,

			kRotaryArc,
			kRotaryArcDisabled,
			kRotaryArcUnselected,
			kRotaryArcUnselectedDisabled,
			kRotaryHand,
			kRotaryBody,
			kRotaryBodyBorder,

			kLinearSlider,
			kLinearSliderDisabled,
			kLinearSliderUnselected,
			kLinearSliderThumb,
			kLinearSliderThumbDisabled,

			kModulationMeter,
			kModulationMeterLeft,
			kModulationMeterRight,
			kModulationMeterControl,
			kModulationButtonSelected,
			kModulationButtonDragging,
			kModulationButtonUnselected,

			kIconButtonOff,
			kIconButtonOffHover,
			kIconButtonOffPressed,
			kIconButtonOn,
			kIconButtonOnHover,
			kIconButtonOnPressed,

			kActionButtonPrimary,
			kActionButtonPrimaryHover,
			kActionButtonPrimaryPressed,
			kActionButtonSecondary,
			kActionButtonSecondaryHover,
			kActionButtonSecondaryPressed,
			kActionButtonText,

			kPowerButtonOn,
			kPowerButtonOff,

			kTextEditorBackground,
			kTextEditorBorder,
			kTextEditorCaret,
			kTextEditorSelection,

			kFinalColor
		};

		static constexpr auto kColorIdCount = kFinalColor - kInitialColor;

		Skin();

		void clearSkin();

		void setColor(ColorId colorId, const juce::Colour &color) noexcept;
		u32 getColor(ColorId colorId) const;
		u32 getColor(SectionOverride section, ColorId colorId) const;
		u32 getColor(const OpenGlContainer *section, ColorId colorId) const;

		void setValue(ValueId valueId, float value) { values_[valueId] = value; }
		float getValue(ValueId valueId) const { return values_[valueId]; }
		float getValue(SectionOverride section, ValueId valueId) const;
		float getValue(const OpenGlContainer *section, ValueId valueId) const;

		void addOverrideColor(int section, ColorId colorId, const juce::Colour &color);
		void removeOverrideColor(int section, ColorId colorId);
		bool overridesColor(int section, ColorId colorId) const;

		void addOverrideValue(int section, ValueId valueId, float value);
		void removeOverrideValue(int section, ValueId valueId);
		bool overridesValue(int section, ValueId valueId) const;

		void copyValuesToLookAndFeel(juce::LookAndFeel *lookAndFeel) const;

		std::string stateToString();
		void saveToFile(const juce::File &destination);

		void jsonToState(std::any jsonData);
		bool stringToState(const juce::String &skin_string);
		bool loadFromFile(const juce::File &source);
		void loadDefaultSkin();

		static bool shouldScaleValue(ValueId valueId) noexcept
		{
			return valueId != kWidgetFillFade && valueId != kWidgetFillBoost &&
				valueId != kWidgetLineBoost && valueId != kKnobHandleLength &&
				valueId != kWidgetFillCenter;
		}

	protected:
		std::array<u32, kColorIdCount> colors_{};
		std::array<float, kValueIdCount> values_{};
		std::array<Framework::VectorMap<ColorId, u32>, kSectionsCount> colorOverrides_{};
		std::array<Framework::VectorMap<ValueId, float>, kSectionsCount> valueOverrides_{};
	};
}