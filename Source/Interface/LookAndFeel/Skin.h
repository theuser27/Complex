/*
	==============================================================================

		Skin.h
		Created: 16 Nov 2022 6:55:11am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include <Third Party/json/json.hpp>
#include "Framework/common.h"

using json = nlohmann::json;

namespace Interface
{
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

			kSectionsCount
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

		void setColor(ColorId colorId, const Colour &color) { colors_[colorId - kInitialColor] = color; }
		Colour getColor(ColorId colorId) const { return colors_[colorId - kInitialColor]; }
		Colour getColor(int section, ColorId colorId) const;
		Colour getColor(const BaseSection *section, ColorId colorId) const;

		void setValue(ValueId valueId, float value) { values_[valueId] = value; }
		float getValue(ValueId valueId) const { return values_[valueId]; }
		float getValue(int section, ValueId valueId) const;
		float getValue(const BaseSection *section, ValueId valueId) const;

		void addOverrideColor(int section, ColorId colorId, Colour color);
		void removeOverrideColor(int section, ColorId colorId);
		bool overridesColor(int section, ColorId colorId) const;

		void addOverrideValue(int section, ValueId valueId, float value);
		void removeOverrideValue(int section, ValueId valueId);
		bool overridesValue(int section, ValueId valueId) const;

		void copyValuesToLookAndFeel(LookAndFeel *lookAndFeel) const;

		json stateToJson();
		String stateToString();
		void saveToFile(File destination);

		json updateJson(json data);
		void jsonToState(json skin_var);
		bool stringToState(String skin_string);
		bool loadFromFile(const File &source);
		void loadDefaultSkin();

		static bool shouldScaleValue(ValueId valueId) noexcept
		{
			return valueId != kWidgetFillFade && valueId != kWidgetFillBoost &&
				valueId != kWidgetLineBoost && valueId != kKnobHandleLength &&
				valueId != kWidgetFillCenter;
		}

	protected:
		std::array<Colour, kColorIdCount> colors_{};
		std::array<float, kValueIdCount> values_{};
		std::array<std::map<ColorId, Colour>, kSectionsCount> colorOverrides_{};
		std::array<std::map<ValueId, float>, kSectionsCount> valueOverrides_{};
	};
}