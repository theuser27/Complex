/*
	==============================================================================

		PluginSlider.cpp
		Created: 14 Dec 2022 6:59:59am
		Author:  theuser27

	==============================================================================
*/

#include "Framework/update_types.h"
#include "BaseSlider.h"
#include "../LookAndFeel/Paths.h"
#include "../Sections/InterfaceEngineLink.h"
#include "../Sections/BaseSection.h"

namespace Interface
{
	void SliderLabel::setParent(BaseSlider *parent) noexcept
	{
		parent_ = parent;
		labelText_ = parent->getParameterDetails().displayName.data();
	}

	void SliderLabel::resized()
	{
		auto height = getHeight();
		if (labelModifier_)
			labelModifier_->setBounds(0, height / 2, labelModifier_->getWidth(), height / 2);
		else if (valueEntry_)
			valueEntry_->setBounds(0, height / 2, valueEntry_->getWidth(), height / 2);
	}
	void SliderLabel::paint(Graphics& g)
	{
		// painting only the label text, the other components will be handled by the owning section
		if (!parent_)
			return;

		auto height = (float)getHeight();
		auto textRectangle = Rectangle{ 0.0f, 0.0f, (float)getWidth(), height / 2.0f };
		g.setColour(parent_->findColour(Skin::kBodyText, true));
		g.drawText(parent_->getParameterDetails().displayName.data(), textRectangle, Justification::centredLeft);
	}

	int SliderLabel::getTotalWidth() const
	{
		auto totalWidth = usedFont_.getStringWidthFloat(labelText_);

		if (labelModifier_)
			totalWidth = std::max(totalWidth, (float)labelModifier_->getTotalDrawWidth());
		else if (valueEntry_)
			totalWidth = std::max(totalWidth, parent_->getNumericTextMaxWidth(valueEntry_->getUsedFont()));

		return (int)std::round(totalWidth);
	}

	BaseSlider::BaseSlider(Framework::ParameterValue *parameter, String name) : Slider(LinearVertical, NoTextBox)
	{
		sliderQuad_.setTargetComponent(this);

		imageComponent_.paintEntireComponent(false);
		imageComponent_.setComponent(this);
		imageComponent_.setScissor(true);

		sliderQuad_.setActive(false);
		imageComponent_.setActive(false);

		textEntry_ = std::make_unique<OpenGlTextEditor>("textEntry");
		textEntry_->setMultiLine(false);
		textEntry_->setScrollToShowCursor(false);
		textEntry_->addListener(this);
		textEntry_->setSelectAllWhenFocused(true);
		textEntry_->setKeyboardType(TextEditor::numericKeyboard);
		textEntry_->setJustification(Justification::centred);
		textEntry_->setIndents(0, 0);
		textEntry_->setBorder({ 0, 0, 0, 0 });
		textEntry_->setAlwaysOnTop(true);
		textEntry_->getImageComponent()->setAlwaysOnTop(true);
		addChildComponent(textEntry_.get());

		setWantsKeyboardFocus(true);
		setDrawBox(getLocalBounds());

		if (!parameter)
		{
			setName(name);
			hasParameterAssignment_ = false;
			return;
		}
		
		hasParameterAssignment_ = true;
		setName(parameter->getParameterDetails().name.data());

		setParameterLink(parameter->getParameterLink());
		setParameterDetails(parameter->getParameterDetails());
		label_.setParent(this);

		setValue(details_.defaultNormalisedValue, NotificationType::dontSendNotification);
		setValueSafe(details_.defaultNormalisedValue);

		setDoubleClickReturnValue(true, details_.defaultNormalisedValue);
		setVelocityBasedMode(false);
		setVelocityModeParameters(1.0, 0, 0.0, false, ModifierKeys::ctrlAltCommandModifiers);
		setSliderSnapsToMousePosition(false);
		setDefaultRange();
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
			beginChange(getValue());
			// calls mouseDrag internally
			Slider::mouseDown(e);

			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->beginChangeGesture();

			for (SliderListener *listener : sliderListeners_)
				listener->mouseDown(this);

			showPopup(true);
		}
	}

	void BaseSlider::mouseDrag(const MouseEvent &e)
	{
		Slider::mouseDrag(e);
		setValueSafe(getValue());
		setValueToHost();

		if (!e.mods.isPopupMenu())
			showPopup(true);
	}

	void BaseSlider::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu() || e.mods.isAltDown())
			return;

		setDefaultRange();
		Slider::mouseUp(e);
		setValueSafe(getValue());
		setValueToHost();
		endChange();

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

		if (showPopupOnHover_)
			showPopup(true);

		hovering_ = true;
		if (shouldRepaintOnHover_)
			redoImage();
	}

	void BaseSlider::mouseExit(const MouseEvent &e)
	{
		Slider::mouseExit(e);
		for (SliderListener *listener : sliderListeners_)
			listener->hoverEnded(this);

		hidePopup(true);
		hovering_ = false;
		if (shouldRepaintOnHover_)
			redoImage();
	}

	void BaseSlider::mouseDoubleClick(const MouseEvent &e)
	{
		Slider::mouseDoubleClick(e);
		setValueSafe(getValue());
		setValueToHost();
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

	void BaseSlider::parentHierarchyChanged()
	{
		parent_ = findParentComponentOfClass<BaseSection>();
		Slider::parentHierarchyChanged();
	}

	void BaseSlider::endChange()
	{
		auto *mainInterface = findParentComponentOfClass<MainInterface>();
		mainInterface->getPlugin().pushUndo(new Framework::ParameterUpdate(this, valueBeforeChange_, getValue()));
	}

	String BaseSlider::getRawTextFromValue(double value)
	{
		if (!hasParameterAssignment_)
			return Slider::getTextFromValue(value);

		return String(Framework::scaleValue(value, details_, getSampleRate(), true));
	}

	String BaseSlider::getSliderTextFromValue(double value, bool retrieveSampleRate)
	{
		if (!hasParameterAssignment_)
			return Slider::getTextFromValue(value);

		float sampleRate = (retrieveSampleRate) ? getSampleRate() : (float)kDefaultSampleRate;
		double scaledValue = Framework::scaleValue(value, details_, sampleRate, true);
		if (!details_.stringLookup.empty())
		{
			size_t lookup = (size_t)(std::clamp(scaledValue, (double)details_.minValue, 
				(double)details_.maxValue) - details_.minValue);
			return popupPrefix_ + details_.stringLookup[lookup].data();
		}
		
		return popupPrefix_ + formatValue(scaledValue);
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
		return Framework::unscaleValue(Slider::getValueFromText(text), details_, true);
	}

	double BaseSlider::snapValue(double attemptedValue, DragMode dragMode)
	{
		constexpr double percent = 0.05;
		if (!snapToValue_ || sensitiveMode_ || dragMode != DragMode::absoluteDrag)
			return attemptedValue;

		double range = getMaximum() - getMinimum();
		double radius = percent * range;
		if (attemptedValue - snapValue_ <= radius && attemptedValue - snapValue_ >= -radius)
			return snapValue_;
		return attemptedValue;
	}

	void BaseSlider::showTextEntry()
	{
		textEntry_->setVisible(true);
		textEntry_->redoImage();
		textEntry_->setText(getRawTextFromValue(getValue()));
		textEntry_->selectAll();
		if (textEntry_->isShowing())
			textEntry_->grabKeyboardFocus();
	}

	void BaseSlider::setDefaultRange()
	{
		if (!hasParameterAssignment_)
			return;

		if (details_.scale == Framework::ParameterScale::Indexed)
			setRange(0.0, 1.0, 1.0 / (details_.maxValue - details_.minValue));
		else
			setRange(0.0, 1.0);
	}

	float BaseSlider::getNumericTextMaxWidth(const Font &usedFont) const
	{
		int integerPlaces = std::max(maxTotalCharacters_ - maxDecimalCharacters_, 0);
		// for the separating '.' between integer and decimal parts
		if (maxDecimalCharacters_)
			integerPlaces--;

		String maxStringLength;
		if (shouldUsePlusMinusPrefix_)
			maxStringLength += '+';

		// figured out that 8s take up the most space in DDin
		for (int i = 0; i < integerPlaces; i++)
			maxStringLength += '8';

		if (maxDecimalCharacters_)
		{
			maxStringLength += '.';

			for (int i = 0; i < maxDecimalCharacters_; i++)
				maxStringLength += '8';
		}

		maxStringLength += details_.displayUnits.data();

		return usedFont.getStringWidthFloat(maxStringLength);
	}

	void BaseSlider::showPopup(bool primary)
	{
		if (shouldShowPopup_)
			parent_->showPopupDisplay(this, getTextFromValue(getValue()).toStdString(), popupPlacement_, primary);
	}

	void BaseSlider::hidePopup(bool primary) { parent_->hidePopupDisplay(primary); }

	String BaseSlider::formatValue(double value) const noexcept
	{
		if (details_.scale == Framework::ParameterScale::Indexed)
			return String(value) + details_.displayUnits.data();

		String format;

		int integerCharacters = maxTotalCharacters_;
		if (maxDecimalCharacters_ == 0)
			format = String(std::round(value));
		else
		{
			format = String(value, maxDecimalCharacters_);
			// +1 because of the dot
			integerCharacters -= maxDecimalCharacters_ + 1;
		}

		int numberOfIntegers = format.indexOfChar('.');
		int insertIndex = 0;
		int displayCharacters = maxTotalCharacters_;
		if (format[0] == '-')
		{
			insertIndex++;
			numberOfIntegers--;
			displayCharacters += 1;
		}
		else if (shouldUsePlusMinusPrefix_)
		{
			insertIndex++;
			displayCharacters += 1;
			format = String("+") + format;
		}

		// insert leading zeroes
		int numZeroesToInsert = integerCharacters - numberOfIntegers;
		for (int i = 0; i < numZeroesToInsert; i++)
			format = format.replaceSection(insertIndex, 0, StringRef("0"));

		// truncating string to fit
		format = format.substring(0, displayCharacters);
		if (format.getLastCharacter() == '.')
			format = format.removeCharacters(".");

		// adding suffix
		return format + details_.displayUnits.data();
	}

	void BaseSlider::handlePopupResult(int result)
	{
		auto mainInterface = findParentComponentOfClass<MainInterface>();
		COMPLEX_ASSERT(mainInterface && "What component owns this slider that isn't a child of MainInterface??");

		auto &plugin = mainInterface->getPlugin();

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
				plugin.getParameterBridges()[connection_index]->resetParameterLink(parameterLink_);
				notifyModulationsChanged();
			}
		}
	}

	std::vector<Framework::ParameterBridge *> BaseSlider::getMappedParameters() const noexcept
	{
		auto mainInterface = findParentComponentOfClass<MainInterface>();
		COMPLEX_ASSERT(mainInterface && "What component owns this slider that isn't a child of MainInterface??");

		auto &synth = mainInterface->getPlugin();
		return synth.getParameterBridges();
	}

	float BaseSlider::findValue(Skin::ValueId valueId) const
	{
		if (valueLookup_.contains(valueId))
			return valueLookup_.at(valueId);
		if (parent_)
			return parent_->findValue(valueId);
		return 0.0f;
	}

	PopupItems BaseSlider::createPopupMenu() const noexcept
	{
		PopupItems options{ getName().toStdString() };

		if (isDoubleClickReturnEnabled())
			options.addItem(kDefaultValue, "Set to Default Value");

		//if (hasParameterAssignment_)
		//  options.addItem(kArmMidiLearn, "Learn MIDI Assignment");

		//if (hasParameterAssignment_ && interfaceEngineLink_->getPlugin()->isMidiMapped(getName().toStdString()))
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

	float BaseSlider::getSampleRate() const noexcept
	{
		auto mainInterface = findParentComponentOfClass<MainInterface>();
		COMPLEX_ASSERT(mainInterface && "What component owns this slider that isn't a child of MainInterface??");
		return mainInterface->getPlugin().getSampleRate();
	}

	void RotarySlider::mouseDrag(const MouseEvent &e)
	{
		float multiply = 1.0f;
		setDefaultRange();

		sensitiveMode_ = e.mods.isCommandDown();
		if (sensitiveMode_)
			multiply *= kSlowDragMultiplier;

		setMouseDragSensitivity((int)(kDefaultRotaryDragLength / (sensitivity_ * multiply)));

		BaseSlider::mouseDrag(e);
	}

	void RotarySlider::redoImage()
	{
		static constexpr float kRotaryHoverBoost = 1.2f;

		if (drawBox_.getWidth() <= 0 || drawBox_.getHeight() <= 0)
			return;

		float arc = sliderQuad_.getMaxArc();
		float t = (float)Framework::scaleValue(getValueForDrawing(this), details_, getSampleRate(), false, true);
		sliderQuad_.setShaderValue(0, std::lerp(-arc, arc, t));
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setAltColor(unselectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);
		sliderQuad_.setStartPos(bipolar_ ? 0.0f : -kPi);

		float thickness = findValue(Skin::kKnobArcThickness);
		if (isMouseOverOrDragging())
			thickness *= kRotaryHoverBoost;
		sliderQuad_.setThickness(thickness);
	}

	void RotarySlider::setSliderBounds()
	{
		drawBox_ = getBounds();
		auto width = (float)drawBox_.getWidth();
		auto height = (float)drawBox_.getHeight();

		float thickness = findValue(Skin::kKnobArcThickness);
		float size = findValue(Skin::kKnobArcSize) * getKnobSizeScale() + thickness;
		float offset = findValue(Skin::kKnobOffset);
		float radius_x = (size + 0.5f) / width;
		float center_y = 2.0f * offset / height;
		float radius_y = (size + 0.5f) / height;
		sliderQuad_.setQuad(0, -radius_x, -center_y - radius_y, 2.0f * radius_x, 2.0f * radius_y);
		sliderQuad_.setThumbAmount(findValue(Skin::kKnobHandleLength));
	}

	void RotarySlider::drawShadow(Graphics &g)
	{
		Colour shadow_color = findColour(Skin::kShadow, true);

		auto width = (float)drawBox_.getWidth();
		auto height = (float)drawBox_.getHeight();

		float center_x = width / 2.0f;
		float center_y = height / 2.0f;
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
		if (body_radius >= 0.0f && body_radius < width)
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

			ColourGradient borderGradient(findColour(Skin::kRotaryBodyBorder, true), center_x, 0.0f,
				body, center_x, 0.75f * height, false);

			g.setGradientFill(borderGradient);
			g.drawEllipse(ellipse.reduced(0.5f), 1.0f);
		}

		Path shadow_outline;
		Path shadow_path;

		shadow_outline.addCentredArc(center_x, center_y, radius, radius,
			0.0f, -kRotaryAngle, kRotaryAngle, true);
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
		auto width = (float)drawBox_.getWidth();
		auto height = (float)drawBox_.getHeight();

		int text_width = width * textEntryWidthPercent_;
		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = font_size / kTextEntryHeightPercent;

		textEntry_->setBounds((getWidth() - text_width) / 2, (height - text_height + 1) / 2,
			text_width, text_height);

		BaseSlider::showTextEntry();
	}

	void LinearSlider::redoImage()
	{
		if (drawBox_.getWidth() <= 0 || drawBox_.getHeight() <= 0)
			return;

		sliderQuad_.setActive(true);
		auto t = (float)Framework::scaleValue(getValueForDrawing(this), details_, getSampleRate(), false, true);
		sliderQuad_.setShaderValue(0, t);
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setAltColor(unselectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);
		sliderQuad_.setStartPos(bipolar_ ? 0.0f : -1.0f);

		int total_width = isHorizontal() ? drawBox_.getHeight() : drawBox_.getWidth();
		int extra = total_width % 2;
		auto slider_width = (float)(total_width + extra);

		sliderQuad_.setThickness(slider_width);
		sliderQuad_.setRounding(slider_width / 2.0f);
	}

	void LinearSlider::setSliderBounds()
	{
		if (isHorizontal())
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBox_.getWidth();
			sliderQuad_.setQuad(0, -1.0f + margin, -1.0f, 2.0f - 2.0f * margin, 2.0f);
		}
		else
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBox_.getHeight();
			sliderQuad_.setQuad(0, -1.0f, -1.0f + margin, 2.0f, 2.0f - 2.0f * margin);
		}
	}

	void LinearSlider::showTextEntry()
	{
		static constexpr float kTextEntryWidthRatio = 3.0f;

		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = (int)(font_size / kTextEntryHeightPercent);
		int text_width = (int)(text_height * kTextEntryWidthRatio);

		textEntry_->setBounds((drawBox_.getWidth() - text_width) / 2, 
			(drawBox_.getHeight() - text_height) / 2, text_width, text_height);

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

		static Path pinPentagon = []()
		{
			Path shape{};

			// top
			shape.startNewSubPath(kWidth * 0.5f, 0.0f);
			shape.lineTo(kWidth - kRounding, 0.0f);
			shape.quadraticTo(kWidth, 0.0f, kWidth, kRounding);

			// right vertical
			shape.lineTo(kWidth, kVerticalSideYLength - controlPoint1YOffset);
			shape.quadraticTo(kWidth, kVerticalSideYLength,
				kWidth - controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);

			// right sideways
			shape.lineTo(kWidth * 0.5f + controlPoint3XOffset, kHeight - controlPoint3YOffset);
			shape.quadraticTo(kWidth * 0.5f, kHeight,
				kWidth * 0.5f - controlPoint3XOffset, kHeight - controlPoint3YOffset);

			// left sideways
			shape.lineTo(controlPoint2XOffset, kVerticalSideYLength + controlPoint2YOffset);
			shape.quadraticTo(0.0f, kVerticalSideYLength,
				0.0f, kVerticalSideYLength - controlPoint2YOffset);

			// left vertical
			shape.lineTo(0.0f, kRounding);
			shape.quadraticTo(0.0f, 0.0f, kRounding, 0.0f);

			shape.closeSubPath();
			return shape;
		}();

		g.setColour(getThumbColor());
		g.fillPath(pinPentagon, pinPentagon.getTransformToScaleToFit(drawBox_.toFloat(), true));
	}

	void PinSlider::redoImage()
	{
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);

		imageComponent_.redrawImage();
	}

	void PinSlider::setSliderBounds()
	{
		float leftOffset = 2.0f * (float)drawBox_.getTopLeft().x / (float)getWidth();
		float rightOffset = 2.0f * (float)(getWidth() - drawBox_.getRight()) / (float)getWidth();
		float topOffset = 2.0f * (float)drawBox_.getTopLeft().y / (float)getHeight();
		float bottomOffset = 2.0f * (float)(getHeight() - drawBox_.getBottom()) / (float)getHeight();
		sliderQuad_.setQuad(0, -1.0f + leftOffset, -1.0f + bottomOffset, 
			2.0f - leftOffset - rightOffset, 2.0f - bottomOffset - topOffset);
	}

	TextSelector::TextSelector(Framework::ParameterValue *parameter, 
		Font usedFont, String name) : BaseSlider(parameter, std::move(name))
	{
		setUseQuad(true);
		sliderQuad_.setFragmentShader(Shaders::kRoundedRectangleFragment);
		setUseImage(true);
		setIsImageOnTop(true);

		// checking if the font has not been set, sans serif is not used here
		if (usedFont.getTypefaceName() == Font::getDefaultSansSerifFontName())
			usedFont_ = Fonts::instance()->getInterVFont();
		else
			usedFont_ = std::move(usedFont);
	}

	void TextSelector::mouseDown(const juce::MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
		{
			BaseSlider::mouseDown(e);
			return;
		}

		std::span<const std::string_view> lookup = details_.stringLookup;
		// idk when this would happen but just to be sure
		if (lookup.empty())
			return;

		PopupItems options;
		for (int i = 0; i <= getMaximum(); ++i)
			options.addItem(i, lookup[i].data());

		parent_->showPopupSelector(this, Point<int>(0, getHeight()), options, [=](int value) { setValue(value); });
	}

	void TextSelector::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			BaseSlider::mouseUp(e);
	}

	void TextSelector::paint(Graphics &g)
	{
		static Path arrowPath = []()
		{
			Path path;
			path.startNewSubPath(0.0f, 0.0f);
			path.lineTo(0.5f, 0.5f);
			path.lineTo(1.0f, 0.0f);
			return path;
		}();

		auto height = drawBox_.getHeight();
		auto leftOffset = height / 4;

		String text = getSliderTextFromValue(getValue());
		g.setColour(selectedColor_);
		g.setFont(usedFont_);
		g.drawText(text, leftOffset, 0, textWidth_, height, Justification::centred, false);

		if (!drawArrow_)
			return;

		Rectangle<int> arrowBounds;
		arrowBounds.setX(leftOffset + textWidth_ + kArrowOffset);
		arrowBounds.setY(height / 2 - 1);

		auto fontSize = usedFont_.getHeight();
		auto arrowRatio = fontSize / Fonts::instance()->getDefaultFontHeight(usedFont_);
		arrowBounds.setWidth((int)std::round(arrowRatio * (float)kArrowWidth));
		arrowBounds.setHeight((int)std::round(kArrowWidthHeightRatio * arrowRatio * (float)kArrowWidth));

		g.setColour(selectedColor_);
		g.strokePath(arrowPath, PathStrokeType{ 1.0f, PathStrokeType::mitered, PathStrokeType::square }, 
			arrowPath.getTransformToScaleToFit(arrowBounds.toFloat(), true));
	}

	void TextSelector::valueChanged()
	{
		resizeForText();
		BaseSlider::valueChanged();
	}

	void TextSelector::redoImage()
	{
		sliderQuad_.setRounding(findValue(Skin::kWidgetRoundedCorner));
		if (isMouseOverOrDragging())
			sliderQuad_.setColor(getBackgroundColor());
		else
			sliderQuad_.setColor(Colours::transparentBlack);
		imageComponent_.redrawImage();
	}

	void TextSelector::showTextEntry()
	{
		int text_width = (int)((float)drawBox_.getWidth() * textEntryWidthPercent_);
		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = (int)(font_size / kTextEntryHeightPercent);
		float y_offset = findValue(Skin::kTextComponentOffset);

		textEntry_->setBounds((drawBox_.getWidth() - text_width) / 2, 
			(drawBox_.getHeight() - text_height + 1) / 2 + y_offset, text_width, text_height);

		BaseSlider::showTextEntry();
	}

	void TextSelector::setSliderBounds()
	{
		drawBox_ = getLocalBounds();
		redoImage();
	}

	[[nodiscard]] int TextSelector::setHeight(float height) noexcept
	{
		if (getHeight() == (int)height)
			return totalDrawWidth_;

		auto fontHeight = Fonts::instance()->getHeightFromAscent(usedFont_, height * 0.5f);
		usedFont_.setHeight(fontHeight);
		usedFont_.setExtraKerningFactor((fontHeight + 0.5f) / fontHeight - 1.0f);

		String text = getSliderTextFromValue(getValue(), false);
		textWidth_ = usedFont_.getStringWidth(text);
		float totalDrawWidth = (float)textWidth_;

		if (drawArrow_)
		{
			totalDrawWidth += kArrowOffset;
			auto arrowRatio = fontHeight / Fonts::instance()->getDefaultFontHeight(usedFont_);
			totalDrawWidth += arrowRatio * (float)kArrowWidth;
		}

		// there's always some padding at the beginning and end regardless whether the arrow is drawn
		totalDrawWidth += height * 0.5f;

		totalDrawWidth_ = (int)std::round(totalDrawWidth);

		return totalDrawWidth_;
	}

	void TextSelector::resizeForText() noexcept
	{
		String text = getSliderTextFromValue(getValue(), false);
		auto newTextWidth = usedFont_.getStringWidth(text);
		auto sizeChange = newTextWidth - textWidth_;

		if (textSelectorListener_)
			textSelectorListener_->resizeForText(this, sizeChange);

		textWidth_ = newTextWidth;
		totalDrawWidth_ += sizeChange;
	}

	NumberBox::NumberBox(Framework::ParameterValue *parameter, String name) :
		BaseSlider(parameter, std::move(name))
	{
		textEntry_->setInterceptsMouseClicks(false, false);

		setUseImage(true);

		setUseQuad(false);
		setShouldRepaintOnHover(false);
		sliderQuad_.setFragmentShader(Shaders::kRoundedRectangleFragment);
	}

	void NumberBox::mouseDown(const MouseEvent &e)
	{
		// TODO: alt + click edit the textEntry's value itself
		//textEntry_->selectAll();
		//if (textEntry_->isShowing())
		//	textEntry_->grabKeyboardFocus();

		BaseSlider::mouseDown(e);
	}

	void NumberBox::mouseDrag(const MouseEvent& e)
	{
		static constexpr float kNormalDragMultiplier = 0.5f;
		static constexpr float kSlowDragMultiplier = 0.1f;

		sensitiveMode_ = e.mods.isShiftDown();
		float multiply = kNormalDragMultiplier;
		if (sensitiveMode_)
			multiply *= kSlowDragMultiplier;

		auto sensitivity = (double)std::max(getWidth(), getHeight()) / (sensitivity_ * multiply);

		setMouseDragSensitivity((int)sensitivity);

		BaseSlider::mouseDrag(e);
	}

	void NumberBox::paint(Graphics& g)
	{
		//static constexpr int kTextLeftOffset = 2;

		static constexpr float kEdgeRounding = 2.0f;
		static constexpr float kCornerRounding = 3.0f;
		static constexpr float kRotatedSideAngle = kPi * 0.25f;

		static constexpr float controlPoint1XOffset = gcem::tan(kRotatedSideAngle * 0.5f) * kCornerRounding;
		static constexpr float controlPoint2XOffset = controlPoint1XOffset * gcem::cos(kRotatedSideAngle);
		static constexpr float controlPoint2YOffset = controlPoint1XOffset * gcem::sin(kRotatedSideAngle);

		static constexpr float edgeControlPointAbsoluteOffset = gcem::tan(kRotatedSideAngle * 0.5f) * kEdgeRounding;
		static constexpr float edgeControlPointXOffset = edgeControlPointAbsoluteOffset * gcem::cos(kRotatedSideAngle);
		static constexpr float edgeControlPointYOffset = edgeControlPointAbsoluteOffset * gcem::sin(kRotatedSideAngle);

		if (!drawBackground_)
			return;

		auto width = (float)drawBox_.getWidth();
		auto height = (float)drawBox_.getHeight();
		auto triangleXLength = height * 0.5f;

		Path box;

		// right
		box.startNewSubPath(width - kCornerRounding, 0.0f);
		box.quadraticTo(width, 0.0f, width, kCornerRounding);
		box.lineTo(width, height - kCornerRounding);

		// bottom
		box.quadraticTo(width, height, width - kCornerRounding, height);
		box.lineTo(triangleXLength + controlPoint1XOffset, height);

		// triangle bottom side
		box.quadraticTo(triangleXLength, height, triangleXLength - controlPoint2XOffset, height - controlPoint2YOffset);
		box.lineTo(edgeControlPointXOffset, height / 2.0f + edgeControlPointYOffset);

		// triangle top side
		box.quadraticTo(0.0f, height / 2.0f, edgeControlPointXOffset, height / 2.0f - edgeControlPointYOffset);
		box.lineTo(triangleXLength - controlPoint2XOffset, controlPoint2YOffset);

		// top
		box.quadraticTo(triangleXLength, 0.0f, triangleXLength + controlPoint2XOffset, 0.0f);
		box.closeSubPath();

		g.setColour(backgroundColor_);
		g.fillPath(box);
	}

	void NumberBox::redoImage()
	{
		textEntry_->setColour(TextEditor::textColourId, selectedColor_);
		textEntry_->setText(getSliderTextFromValue(getValue()));

		imageComponent_.redrawImage();

		sliderQuad_.setRounding(findValue(Skin::kWidgetRoundedCorner));
		if (isMouseOverOrDragging())
			sliderQuad_.setColor(backgroundColor_);
		else
			sliderQuad_.setColor(Colours::transparentBlack);
	}

	void NumberBox::setSliderBounds()
	{
		drawBox_ = getLocalBounds();
		auto bounds = drawBox_.toFloat();
		auto height = (float)drawBox_.getHeight();
		auto xOffset = height * kTriangleWidthRatio + height * kTriangleToValueMarginRatio;

		// extra offsets are pretty much magic values, don't change
		if (drawBackground_)
			bounds.removeFromLeft(xOffset - 1.0f);
		else
			bounds.removeFromLeft(2.0f);
 
		textEntry_->setBounds(bounds.toNearestInt());
		textEntry_->setVisible(true);

		redoImage();
	}

	void NumberBox::showTextEntry()
	{
		textEntry_->setColour(CaretComponent::caretColourId, findColour(Skin::kTextEditorCaret, true));
		textEntry_->setColour(TextEditor::textColourId, findColour(Skin::kBodyText, true));
		textEntry_->setColour(TextEditor::highlightedTextColourId, findColour(Skin::kBodyText, true));
		textEntry_->setColour(TextEditor::highlightColourId, findColour(Skin::kTextEditorSelection, true));

		BaseSlider::showTextEntry();
	}

	[[nodiscard]] int NumberBox::setHeight(float height) noexcept
	{
		if (getHeight() == (int)height)
			return totalDrawWidth_;

		auto fontHeight = Fonts::instance()->getHeightFromAscent(usedFont_, height * 0.5f);
		usedFont_.setHeight(fontHeight);
		usedFont_.setExtraKerningFactor((fontHeight + 0.5f) / fontHeight - 1.0f);
		textEntry_->setUsedFont(usedFont_);

		float totalDrawWidth = getNumericTextMaxWidth(usedFont_);
		if (drawBackground_)
		{
			totalDrawWidth += height * kTriangleWidthRatio;
			totalDrawWidth += kTriangleToValueMarginRatio * height;
			totalDrawWidth += kValueToEndMarginRatio * height;
		}
		else
		{
			// extra space around the value
			totalDrawWidth += height * 0.5f;
		}
		totalDrawWidth_ = (int)std::ceil(totalDrawWidth);		

		return totalDrawWidth_;
	}

	void ModulationSlider::redoImage()
	{
		if (getWidth() <= 0 || getHeight() <= 0)
			return;

		float t = 2.0f * (float)getValue() - 1.0f;
		sliderQuad_.setThumbColor(thumbColor_);

		if (t > 0.0f)
		{
			sliderQuad_.setShaderValue(0, std::lerp(kPi, -kPi, t));
			sliderQuad_.setColor(unselectedColor_);
			sliderQuad_.setAltColor(selectedColor_);
		}
		else
		{
			sliderQuad_.setShaderValue(0, std::lerp(-kPi, kPi, -t));
			sliderQuad_.setColor(selectedColor_);
			sliderQuad_.setAltColor(unselectedColor_);
		}

		if (isMouseOverOrDragging())
			sliderQuad_.setThickness(1.8f);
		else
			sliderQuad_.setThickness(1.0f);
	}

}
