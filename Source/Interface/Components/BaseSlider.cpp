/*
	==============================================================================

		PluginSlider.cpp
		Created: 14 Dec 2022 6:59:59am
		Author:  theuser27

	==============================================================================
*/

#include "BaseSlider.h"
#include "../LookAndFeel/Paths.h"
#include "../Sections/InterfaceEngineLink.h"

namespace Interface
{
	BaseSlider::BaseSlider(String name) : Slider(std::move(name))
	{
		slider_quad_.setTargetComponent(this);
		slider_quad_.setMaxArc(kRotaryAngle);

		image_component_.paintEntireComponent(false);
		image_component_.setComponent(this);
		image_component_.setScissor(true);

		slider_quad_.setActive(false);
		image_component_.setActive(false);

		textEntry_ = std::make_unique<OpenGlTextEditor>("text_entry");
		textEntry_->setValuesFont();
		textEntry_->setMultiLine(false);
		textEntry_->setScrollToShowCursor(false);
		textEntry_->addListener(this);
		textEntry_->setSelectAllWhenFocused(true);
		textEntry_->setKeyboardType(TextEditor::numericKeyboard);
		textEntry_->setJustification(Justification::centred);
		textEntry_->setAlwaysOnTop(true);
		textEntry_->getImageComponent()->setAlwaysOnTop(true);
		addChildComponent(textEntry_.get());

		setWantsKeyboardFocus(true);
		setTextBoxStyle(Slider::NoTextBox, true, 0, 0);

		has_parameter_assignment_ = Framework::Parameters::isParameter(getName().toRawUTF8());
		if (!has_parameter_assignment_)
			return;

		setRotaryParameters(2.0f * kPi - kRotaryAngle, 2.0f * kPi + kRotaryAngle, true);

		details_ = Framework::Parameters::getDetails(getName().toRawUTF8());

		setDefaultRange();
		setDoubleClickReturnValue(true, details_.defaultValue);
		setVelocityBasedMode(false);
		setVelocityModeParameters(1.0, 0, 0.0, false, ModifierKeys::ctrlAltCommandModifiers);
	}

	void BaseSlider::mouseDown(const MouseEvent &e)
	{
		updateValue();

		if (e.mods.isAltDown())
		{
			showTextEntry();
			return;
		}

		if (e.mods.isPopupMenu())
		{
			PopupItems options = createPopupMenu();
			parent_->showPopupSelector(this, e.getPosition(), options, [=](int selection) { handlePopupResult(selection); });
		}
		else
		{
			Slider::mouseDown(e);

			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->beginChangeGesture();

			for (SliderListener *listener : sliderListeners_)
				listener->mouseDown(this);

			setValueFromUI(getValue());
			showPopup(true);
		}
	}

	void BaseSlider::mouseDrag(const MouseEvent &e)
	{
		Slider::mouseDrag(e);
		setValueFromUI(getValue());
		if (!e.mods.isPopupMenu())
			showPopup(true);
	}

	void BaseSlider::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu() || e.mods.isAltDown())
			return;

		setDefaultRange();
		Slider::mouseUp(e);
		setValueFromUI(getValue());

		for (SliderListener *listener : sliderListeners_)
			listener->mouseUp(this);

		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->endChangeGesture();
	}

	void BaseSlider::mouseEnter(const MouseEvent &e)
	{
		Slider::mouseEnter(e);
		for (SliderListener *listener : sliderListeners_)
			listener->hoverStarted(this);

		if (show_popup_on_hover_)
			showPopup(true);

		hovering_ = true;
		redoImage();
	}

	void BaseSlider::mouseExit(const MouseEvent &e)
	{
		Slider::mouseExit(e);
		for (SliderListener *listener : sliderListeners_)
			listener->hoverEnded(this);

		hidePopup(true);
		hovering_ = false;
		redoImage();
	}

	void BaseSlider::mouseDoubleClick(const MouseEvent &e)
	{
		Slider::mouseDoubleClick(e);
		setValueFromUI(getValue());
		if (!e.mods.isPopupMenu())
		{
			for (SliderListener *listener : sliderListeners_)
				listener->doubleClick(this);
		}
		showPopup(true);
	}

	void BaseSlider::focusLost(FocusChangeType cause)
	{
		for (SliderListener *listener : sliderListeners_)
			listener->focusLost(this);
	}

	String BaseSlider::getRawTextFromValue(double value)
	{
		if (!has_parameter_assignment_)
			return Slider::getTextFromValue(value);

		return String(utils::scaleValue(value, details_, true));
	}

	String BaseSlider::getSliderTextFromValue(double value)
	{
		double scaledValue = utils::scaleValue(value, details_, true);
		if (!details_.stringLookup.empty())
		{
			size_t lookup = std::clamp(scaledValue, 0.0, getMaximum());
			return details_.stringLookup[lookup].data();
		}

		if (!has_parameter_assignment_)
			return Slider::getTextFromValue(scaledValue);
		
		return popup_prefix_ + formatValue(scaledValue);
	}

	String BaseSlider::getTextFromValue(double value)
	{
		if (isText() && has_parameter_assignment_ && popup_prefix_.isEmpty())
			return details_.displayName.data();
		if (isText() && !popup_prefix_.isEmpty())
			return popup_prefix_;
		return getSliderTextFromValue(value);
	}

	double BaseSlider::getValueFromText(const String &text)
	{
		String cleaned = text.removeCharacters(" ").toLowerCase();
		if (!details_.stringLookup.empty())
		{
			for (int i = 0; i <= getMaximum(); ++i)
				if (cleaned == String(details_.stringLookup[i].data()).toLowerCase())
					return i;
		}
		if (text.endsWithChar('%') && details_.displayUnits != "%")
		{
			float t = 0.01f * cleaned.removeCharacters("%").getDoubleValue();
			return (getMaximum() - getMinimum()) * t + getMinimum();
		}
		return utils::unscaleValue(Slider::getValueFromText(text), details_, true);
	}

	double BaseSlider::snapValue(double attemptedValue, DragMode dragMode)
	{
		constexpr double percent = 0.05;
		if (!snap_to_value_ || sensitive_mode_ || dragMode != DragMode::absoluteDrag)
			return attemptedValue;

		double range = getMaximum() - getMinimum();
		double radius = percent * range;
		if (attemptedValue - snap_value_ <= radius && attemptedValue - snap_value_ >= -radius)
			return snap_value_;
		return attemptedValue;
	}

	void BaseSlider::showTextEntry()
	{
		textEntry_->setVisible(true);

		textEntry_->setColour(CaretComponent::caretColourId, findColour(Skin::kTextEditorCaret, true));
		textEntry_->setColour(TextEditor::textColourId, findColour(Skin::kBodyText, true));
		textEntry_->setColour(TextEditor::highlightedTextColourId, findColour(Skin::kBodyText, true));
		textEntry_->setColour(TextEditor::highlightColourId, findColour(Skin::kTextEditorSelection, true));

		textEntry_->redoImage();
		textEntry_->setText(getRawTextFromValue(getValue()));
		textEntry_->selectAll();
		if (textEntry_->isShowing())
			textEntry_->grabKeyboardFocus();
	}

	void BaseSlider::setDefaultRange()
	{
		if (!has_parameter_assignment_)
			return;

		if (details_.scale == Framework::ParameterScale::Indexed)
			setRange(details_.minValue, details_.maxValue, 1.0f);
		else
			setRange(details_.minValue, details_.maxValue);
	}

	void BaseSlider::showPopup(bool primary)
	{
		if (shouldShowPopup_)
			parent_->showPopupDisplay(this, getTextFromValue(getValue()).toStdString(), popup_placement_, primary);
	}

	void BaseSlider::hidePopup(bool primary) { parent_->hidePopupDisplay(primary); }

	String BaseSlider::formatValue(double value) const noexcept
	{
		String format;
		if (details_.scale == Framework::ParameterScale::Indexed)
			format = String(value);
		else
		{
			if (max_decimal_places_ == 0)
				format = String(std::round(value));
			else
				format = String(value, max_decimal_places_);

			int display_characters = max_display_characters_;
			if (format[0] == '-')
				display_characters += 1;

			format = format.substring(0, display_characters);
			if (format.getLastCharacter() == '.')
				format = format.removeCharacters(".");
		}

		if (use_suffix_)
			return format + details_.displayUnits.data();
		return format;
	}

	Rectangle<int> BaseSlider::getModulationMeterBounds() const
	{
		static constexpr int kTextBarSize = 2;

		Rectangle<int> mod_bounds = getModulationArea();
		if (isTextOrCurve())
		{
			if (modulation_bar_right_)
				return mod_bounds.removeFromRight(kTextBarSize);

			return mod_bounds.removeFromLeft(kTextBarSize);
		}
		else if (isRotary())
			return getLocalBounds();

		int buffer = findValue(Skin::kWidgetMargin);
		if (getSliderStyle() == LinearBar)
		{
			return Rectangle(mod_bounds.getX() + buffer, mod_bounds.getY(),
				mod_bounds.getWidth() - 2 * buffer, mod_bounds.getHeight());
		}
		return Rectangle(mod_bounds.getX(), mod_bounds.getY() + buffer,
			mod_bounds.getWidth(), mod_bounds.getHeight() - 2 * buffer);
	}

	void BaseSlider::handlePopupResult(int result)
	{
		auto *synth = interfaceEngineLink_->getPlugin();
		auto connections = getMappedParameters();

		//if (result == kArmMidiLearn)
		//  synth->armMidiLearn(getName().toStdString());
		//else if (result == kClearMidiLearn)
		//  synth->clearMidiLearn(getName().toStdString());
		if (result == kDefaultValue)
			setValue(getDoubleClickReturnValue());
		else if (result == kManualEntry)
			showTextEntry();
		//else if (result == kClearModulations)
		//{
		//  for (vital::ModulationConnection *connection : connections)
		//  {
		//    std::string source = connection->source_name;
		//    interfaceEngineLink_->disconnectModulation(connection);
		//  }
		//  notifyModulationsChanged();
		//}
		else if (result == kClearMapping)
		{
			if (parameterLink_ && parameterLink_->hostControl)
			{
				parameterLink_->hostControl->resetParameterLink(nullptr);
				parameterLink_->hostControl = nullptr;
			}
		}
		else if (result == kMapFirstSlot)
		{
			auto parameters = getMappedParameters();

			for (auto &parameter : parameters)
			{
				if (!parameter->isMappedToParameter())
				{
					parameter->resetParameterLink(getParameterLink());
					break;
				}
			}
		}
		else if (result == kMappingList)
		{
			if (parameterLink_)
			{
				int connection_index = result - kMappingList;
				synth->getParameterBridges()[connection_index]->resetParameterLink(parameterLink_);
				notifyModulationsChanged();
			}
		}
	}

	std::vector<Framework::ParameterBridge *> BaseSlider::getMappedParameters() const noexcept
	{
		if (interfaceEngineLink_ == nullptr)
			return {};

		Plugin::ComplexPlugin *synth = interfaceEngineLink_->getPlugin();
		return synth->getParameterBridges();
	}

	PopupItems BaseSlider::createPopupMenu() const noexcept
	{
		PopupItems options{ getName().toStdString() };

		if (isDoubleClickReturnEnabled())
			options.addItem(kDefaultValue, "Set to Default Value");

		//if (has_parameter_assignment_)
		//  options.addItem(kArmMidiLearn, "Learn MIDI Assignment");

		//if (has_parameter_assignment_ && interfaceEngineLink_->getPlugin()->isMidiMapped(getName().toStdString()))
		//  options.addItem(kClearMidiLearn, "Clear MIDI Assignment");

		options.addItem(kManualEntry, "Enter Value");

		if (parameterLink_ && parameterLink_->hostControl)
			options.addItem(kClearMapping, "Clear Parameter Mapping");
		else
		{
			options.addItem(kMapFirstSlot, "Quick Link");

			PopupItems automationSlots{ kMappingList, "Assign to Automation Slot" };
			std::vector<Framework::ParameterBridge *> connections = getMappedParameters();
			for (size_t i = 0; i < connections.size(); i++)
				automationSlots.addItem(i, connections[i]->getName(20).toStdString());

			options.addItem(std::move(automationSlots));
		}

		//std::string disconnect = "Disconnect from ";
		//for (int i = 0; i < connections.size(); ++i)
		//{
		//  std::string name = ModulationMatrix::getMenuSourceDisplayName(connections[i]->source_name).toStdString();
		//  options.addItem(kModulationList + i, disconnect + name);
		//}

		//if (connections.size() > 1)
		//  options.addItem(kClearModulations, "Disconnect all modulations");

		return options;
	}

	void RotarySlider::redoImage()
	{
		static constexpr float kRotaryHoverBoost = 1.2f;

		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		float arc = slider_quad_.getMaxArc();
		float t = (float)utils::scaleValue(getValue(), details_, false, true);
		slider_quad_.setShaderValue(0, std::lerp(-arc, arc, t));
		slider_quad_.setColor(selectedColor_);
		slider_quad_.setAltColor(unselectedColor_);
		slider_quad_.setThumbColor(thumbColor_);
		slider_quad_.setStartPos(bipolar_ ? 0.0f : -kPi);

		float thickness = findValue(Skin::kKnobArcThickness);
		if (isMouseOverOrDragging())
			thickness *= kRotaryHoverBoost;
		slider_quad_.setThickness(thickness);
	}

	void RotarySlider::setSliderBounds()
	{
		float thickness = findValue(Skin::kKnobArcThickness);
		float size = findValue(Skin::kKnobArcSize) * getKnobSizeScale() + thickness;
		float offset = findValue(Skin::kKnobOffset);
		float radius_x = (size + 0.5f) / getWidth();
		float center_y = 2.0f * offset / getHeight();
		float radius_y = (size + 0.5f) / getHeight();
		slider_quad_.setQuad(0, -radius_x, -center_y - radius_y, 2.0f * radius_x, 2.0f * radius_y);
		slider_quad_.setThumbAmount(findValue(Skin::kKnobHandleLength));
	}

	void RotarySlider::drawShadow(Graphics &g)
	{
		Colour shadow_color = findColour(Skin::kShadow, true);

		float center_x = getWidth() / 2.0f;
		float center_y = getHeight() / 2.0f;
		float stroke_width = findValue(Skin::kKnobArcThickness);
		float radius = knobSizeScale_ * findValue(Skin::kKnobArcSize) / 2.0f;
		center_y += findValue(Skin::kKnobOffset);
		float shadow_width = findValue(Skin::kKnobShadowWidth);
		float shadow_offset = findValue(Skin::kKnobShadowOffset);

		PathStrokeType outer_stroke(stroke_width, PathStrokeType::beveled, PathStrokeType::rounded);
		PathStrokeType shadow_stroke(stroke_width + 1, PathStrokeType::beveled, PathStrokeType::rounded);

		g.saveState();
		g.setOrigin(getX(), getY());

		Colour body = findColour(Skin::kRotaryBody, true);
		float body_radius = knobSizeScale_ * findValue(Skin::kKnobBodySize) / 2.0f;
		if (body_radius >= 0.0f && body_radius < getWidth())
		{

			if (shadow_width > 0.0f)
			{
				Colour transparent_shadow = shadow_color.withAlpha(0.0f);
				float shadow_radius = body_radius + shadow_width;
				ColourGradient shadow_gradient(shadow_color, center_x, center_y + shadow_offset,
					transparent_shadow, center_x - shadow_radius, center_y + shadow_offset, true);
				float shadow_start = std::max(0.0f, body_radius - std::abs(shadow_offset)) / shadow_radius;
				shadow_gradient.addColour(shadow_start, shadow_color);
				shadow_gradient.addColour(1.0f - (1.0f - shadow_start) * 0.75f, shadow_color.withMultipliedAlpha(0.5625f));
				shadow_gradient.addColour(1.0f - (1.0f - shadow_start) * 0.5f, shadow_color.withMultipliedAlpha(0.25f));
				shadow_gradient.addColour(1.0f - (1.0f - shadow_start) * 0.25f, shadow_color.withMultipliedAlpha(0.0625f));
				g.setGradientFill(shadow_gradient);
				g.fillRect(getLocalBounds());
			}

			g.setColour(body);
			Rectangle<float> ellipse(center_x - body_radius, center_y - body_radius, 2.0f * body_radius, 2.0f * body_radius);
			g.fillEllipse(ellipse);

			ColourGradient borderGradient(findColour(Skin::kRotaryBodyBorder, true), center_x, 0,
				body, center_x, 0.75f * (float)getHeight(), false);

			g.setGradientFill(borderGradient);
			g.drawEllipse(ellipse.reduced(0.5f), 1.0f);
		}

		Path shadow_outline;
		Path shadow_path;

		shadow_outline.addCentredArc(center_x, center_y, radius, radius,
			0, -kRotaryAngle, kRotaryAngle, true);
		shadow_stroke.createStrokedPath(shadow_path, shadow_outline);
		if ((!findColour(Skin::kRotaryArcUnselected, true).isTransparent() && isActive()) ||
			(!findColour(Skin::kRotaryArcUnselectedDisabled, true).isTransparent() && !isActive()))
		{
			g.setColour(shadow_color);
			g.fillPath(shadow_path);
		}

		g.restoreState();
	}

	void RotarySlider::showTextEntry()
	{
		int text_width = getWidth() * textEntryWidthPercent_;
		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = font_size / kTextEntryHeightPercent;
		float y_offset = 0.0f;
		if (isText())
			y_offset = findValue(Skin::kTextComponentOffset);

		textEntry_->setBounds((getWidth() - text_width) / 2, (getHeight() - text_height + 1) / 2 + y_offset,
			text_width, text_height);

		BaseSlider::showTextEntry();
	}

	void LinearSlider::redoImage()
	{
		static constexpr float kRoundingMult = 0.4f;

		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		slider_quad_.setActive(true);
		float t = valueToProportionOfLength(getValue());
		slider_quad_.setShaderValue(0, t);
		slider_quad_.setColor(selectedColor_);
		slider_quad_.setAltColor(unselectedColor_);
		slider_quad_.setThumbColor(thumbColor_);
		slider_quad_.setStartPos(bipolar_ ? 0.0f : -1.0f);

		int total_width = isHorizontal() ? getHeight() : getWidth();
		int extra = total_width % 2;
		float slider_width = std::floor(kLinearWidthPercent * total_width * 0.5f) * 2.0f + extra;

		float handle_width = kLinearHandlePercent * total_width;
		if (isMouseOverOrDragging())
		{
			int boost = std::round(slider_width / 8.0f) + 1.0f;
			slider_quad_.setThickness(slider_width + 2 * boost);
		}
		else
			slider_quad_.setThickness(slider_width);
		slider_quad_.setRounding(slider_width * kRoundingMult);
		slider_quad_.setThumbAmount(handle_width);
	}

	void LinearSlider::setSliderBounds()
	{
		if (isHorizontal())
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / getWidth();
			slider_quad_.setQuad(0, -1.0f + margin, -1.0f, 2.0f - 2.0f * margin, 2.0f);
		}
		else
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / getHeight();
			slider_quad_.setQuad(0, -1.0f, -1.0f + margin, 2.0f, 2.0f - 2.0f * margin);
		}
	}

	void LinearSlider::showTextEntry()
	{
		static constexpr float kTextEntryWidthRatio = 3.0f;

		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = (int)(font_size / kTextEntryHeightPercent);
		int text_width = (int)(text_height * kTextEntryWidthRatio);

		textEntry_->setBounds((getWidth() - text_width) / 2, (getHeight() - text_height) / 2, text_width, text_height);

		BaseSlider::showTextEntry();
	}

	void PinSlider::paint(Graphics &g)
	{
		static constexpr float kWidth = 10.0f;
		static constexpr float kHeight = kWidth * 0.9f;
		static constexpr float kRounding = 1.0f;
		static constexpr float kVerticalSideYLength = 4.0f;
		static constexpr float kRotatedSideAngle = kPi * 0.25f;

		static constexpr float controlPoint1YOffset = gcem::tan(kRotatedSideAngle / 2.0f) * kRounding;
		static constexpr float controlPoint2XOffset = controlPoint1YOffset * gcem::cos(kRotatedSideAngle);
		static constexpr float controlPoint2YOffset = controlPoint1YOffset * gcem::sin(kRotatedSideAngle);

		static constexpr float controlPoint3XOffset = controlPoint2XOffset;
		static constexpr float controlPoint3YOffset = controlPoint2YOffset;

		// top
		Path pinPentagon{};
		pinPentagon.startNewSubPath(kWidth * 0.5f, 0.0f);
		pinPentagon.lineTo(kWidth - kRounding, 0.0f);
		pinPentagon.quadraticTo(kWidth, 0.0f, kWidth, kRounding);

		// right vertical
		pinPentagon.lineTo(kWidth, kVerticalSideYLength - kRounding - controlPoint1YOffset);
		pinPentagon.quadraticTo(kWidth, kVerticalSideYLength, 
			kWidth - controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);

		// right sideways
		pinPentagon.lineTo(kWidth * 0.5f + controlPoint3XOffset, kHeight - controlPoint3YOffset);
		pinPentagon.quadraticTo(kWidth * 0.5f, kHeight,
			kWidth * 0.5f - controlPoint3XOffset, kHeight - controlPoint3YOffset);

		// left sideways
		pinPentagon.lineTo(controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);
		pinPentagon.quadraticTo(0.0f, kVerticalSideYLength,
			0.0f, kVerticalSideYLength - controlPoint2YOffset);

		// left vertical
		pinPentagon.lineTo(0.0f, kRounding);
		pinPentagon.quadraticTo(0.0f, 0.0f, kRounding, 0.0f);

		pinPentagon.closeSubPath();


		g.setColour(findColour(Skin::kPowerButtonOn, true));
		g.fillPath(pinPentagon, pinPentagon.getTransformToScaleToFit(getBounds().toFloat(), true));
	}

	void PinSlider::redoImage()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		slider_quad_.setColor(selectedColor_);
		slider_quad_.setThumbColor(thumbColor_);

		image_component_.redrawImage();
	}

	void PinSlider::setSliderBounds()
	{
		// TODO: size changes for the pin components
	}

	void ModulationSlider::redoImage()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		float t = 2.0f * (float)getValue() - 1.0f;
		slider_quad_.setThumbColor(thumbColor_);

		if (t > 0.0f)
		{
			slider_quad_.setShaderValue(0, std::lerp(kPi, -kPi, t));
			slider_quad_.setColor(unselectedColor_);
			slider_quad_.setAltColor(selectedColor_);
		}
		else
		{
			slider_quad_.setShaderValue(0, std::lerp(-kPi, kPi, -t));
			slider_quad_.setColor(selectedColor_);
			slider_quad_.setAltColor(unselectedColor_);
		}

		if (isMouseOverOrDragging())
			slider_quad_.setThickness(1.8f);
		else
			slider_quad_.setThickness(1.0f);
	}

}
