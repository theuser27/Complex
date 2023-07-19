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
		labelText_ = (!parent->hasParameter()) ? parent->getName() :
			parent->getParameterDetails().displayName.data();
	}

	void SliderLabel::resized()
	{
		auto height = getHeight();
		if (labelModifier_)
			labelModifier_->setBounds(0, height / 2, labelModifier_->getWidth(), height / 2);
		else if (valueEntry_)
		{
			valueEntry_->setVisible(true);
			valueEntry_->setBounds(0, height / 2, valueEntry_->getWidth(), height / 2);
		}
	}

	void SliderLabel::paint(Graphics& g)
	{
		// painting only the label text, the other components will be handled by the owning section
		if (!parent_)
			return;

		auto textRectangle = Rectangle{ 0.0f, 0.0f, (float)getWidth(), (float)getHeight() };
		g.setFont(usedFont_);
		g.setColour(parent_->getColour(Skin::kNormalText));
		g.drawText(parent_->getParameterDetails().displayName.data(), textRectangle, textJustification_);
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

	BaseSlider::BaseSlider(Framework::ParameterValue *parameter, String name, SliderStyle style) : Slider(style, NoTextBox)
	{
		sliderQuad_.setTargetComponent(this);

		imageComponent_.paintEntireComponent(false);
		imageComponent_.setTargetComponent(this);
		imageComponent_.setScissor(true);

		sliderQuad_.setActive(false);
		imageComponent_.setActive(false);

		textEntry_ = std::make_unique<OpenGlTextEditor>("textEntry");
		textEntry_->setMultiLine(false);
		textEntry_->setScrollToShowCursor(true);
		textEntry_->addListener(this);
		textEntry_->setSelectAllWhenFocused(true);
		textEntry_->setKeyboardType(TextEditor::numericKeyboard);
		textEntry_->setJustification(Justification::centred);
		textEntry_->setIndents(0, 0);
		textEntry_->setBorder({ 0, 0, 0, 0 });
		textEntry_->setAlwaysOnTop(true);
		addChildComponent(textEntry_.get());

		setWantsKeyboardFocus(true);

		if (!parameter)
		{
			setName(name);
			hasParameter_ = false;
			return;
		}
		
		hasParameter_ = true;
		changeLinkedParameter(parameter);
		label_.setParent(this);

		setDoubleClickReturnValue(true, details_.defaultNormalisedValue);
		setVelocityBasedMode(false);
		setVelocityModeParameters(1.0, 0, 0.0, false, ModifierKeys::ctrlAltCommandModifiers);
		setSliderSnapsToMousePosition(false);
		setScrollWheelEnabled(false);
		setDefaultRange();
	}

	Framework::ParameterValue *BaseSlider::changeLinkedParameter(Framework::ParameterValue *parameter, 
		bool getValueFromParameter)
	{
		setName(parameter->getParameterDetails().name.data());
		auto replacedLink = setParameterLink(parameter->getParameterLink());

		Framework::ParameterValue *replacedParameter = nullptr;
		if (replacedLink)
			replacedParameter = replacedLink->parameter;

		if (getValueFromParameter)
		{
			setValue(parameter->getNormalisedValue(), NotificationType::dontSendNotification);
			setValueSafe(parameter->getNormalisedValue());
		}
		else
		{
			parameter->updateValues(getSampleRate(), (float)getValue());
			setValueSafe(getValue());
		}

		setParameterDetails(parameter->getParameterDetails());

		return replacedParameter;
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
			parent_->showPopupSelector(this, e.getPosition(), options, 
				[this](int selection) { handlePopupResult(selection); });
			return;
		}

		if (parameterLink_ && parameterLink_->hostControl)
			parameterLink_->hostControl->beginChangeGesture();

		beginChange(getValue());
		// mouseDown calls mouseDrag internally
		Slider::mouseDown(e);

		sliderQuad_.getAnimator().setIsClicked(true);
		imageComponent_.getAnimator().setIsClicked(true);

		for (SliderListener *listener : sliderListeners_)
			listener->mouseDown(this);

		showPopup(true);
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

		Slider::mouseUp(e);

		sliderQuad_.getAnimator().setIsClicked(false);
		imageComponent_.getAnimator().setIsClicked(false);

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

		sliderQuad_.getAnimator().setIsHovered(true);
		imageComponent_.getAnimator().setIsHovered(true);

		for (SliderListener *listener : sliderListeners_)
			listener->hoverStarted(this);

		if (showPopupOnHover_)
			showPopup(true);

		if (shouldRepaintOnHover_)
			redoImage();
	}

	void BaseSlider::mouseExit(const MouseEvent &e)
	{
		Slider::mouseExit(e);

		sliderQuad_.getAnimator().setIsHovered(false);
		imageComponent_.getAnimator().setIsHovered(false);

		for (SliderListener *listener : sliderListeners_)
			listener->hoverEnded(this);

		hidePopup(true);
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

	void BaseSlider::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
	{
		if (!isScrollWheelEnabled())
			return;

		bool isMapped = parameterLink_ && parameterLink_->hostControl;
		if (isMapped)
			parameterLink_->hostControl->beginChangeGesture();

		if (!hasBegunChange_)
			beginChange(getValue());

		Slider::mouseWheelMove(e, wheel);
		setValueSafe(getValue());
		setValueToHost();

		if (isMapped)
			parameterLink_->hostControl->endChangeGesture();
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

	void BaseSlider::textEditorReturnKeyPressed(TextEditor &editor)
	{
		updateValueFromTextEntry();
		textEntry_->setVisible(false);

		for (SliderListener *listener : sliderListeners_)
			listener->menuFinished(this);
	}

	bool BaseSlider::updateValue()
	{
		if (!hasParameter())
			return false;

		auto oldValue = getValue();
		auto newValue = getValueSafe();

		bool needsUpdate = oldValue != newValue;
		if (needsUpdate)
			setValue(newValue, NotificationType::sendNotificationSync);

		return needsUpdate;
	}

	void BaseSlider::endChange()
	{
		hasBegunChange_ = false;
		auto *mainInterface = findParentComponentOfClass<MainInterface>();
		mainInterface->getPlugin().pushUndo(new Framework::ParameterUpdate(this, valueBeforeChange_, getValue()));
	}

	String BaseSlider::getRawTextFromValue(double value)
	{
		if (!hasParameter())
			return Slider::getTextFromValue(value);

		return String(Framework::scaleValue(value, details_, getSampleRate(), true));
	}

	String BaseSlider::getSliderTextFromValue(double value, bool retrieveSampleRate)
	{
		if (!hasParameter())
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
		if (text.endsWithChar('%') && details_.displayUnits != "%")
			return 0.01 * cleaned.removeCharacters("%").getDoubleValue();
		if (!details_.stringLookup.empty())
		{
			for (int i = 0; i <= (int)(details_.maxValue - details_.minValue); ++i)
				if (cleaned == String(details_.stringLookup[i].data()).toLowerCase())
					return Framework::unscaleValue(details_.minValue + (float)i, details_, true);
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
		if (!hasParameter())
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
		{
			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->beginChangeGesture();

			beginChange(getValue());

			setValueSafe(getDoubleClickReturnValue());
			setValue(getDoubleClickReturnValue());
			setValueToHost();
			endChange();

			if (parameterLink_ && parameterLink_->hostControl)
				parameterLink_->hostControl->endChangeGesture();
		}
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
			auto &parameters = getMappedParameters();

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

	std::vector<Framework::ParameterBridge *> &BaseSlider::getMappedParameters() const noexcept
	{
		auto mainInterface = findParentComponentOfClass<MainInterface>();
		COMPLEX_ASSERT(mainInterface && "What component owns this slider that isn't a child of MainInterface??");

		auto &synth = mainInterface->getPlugin();
		return synth.getParameterBridges();
	}

	float BaseSlider::findValue(Skin::ValueId valueId) const
	{
		if (parent_)
			return parent_->getValue(valueId);
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

		if (canInputValue_)
			options.addItem(kManualEntry, "Enter Value");

		if (parameterLink_ && parameterLink_->hostControl)
			options.addItem(kClearMapping, "Clear Parameter Mapping");
		else
		{
			options.addItem(kMapFirstSlot, "Quick Link");

			PopupItems automationSlots{ kMappingList, "Assign to Automation Slot" };
			std::vector<Framework::ParameterBridge *> connections = getMappedParameters();
			for (size_t i = 0; i < connections.size(); i++)
				automationSlots.addItem((int)i, connections[i]->getName(20).toStdString());

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

	Colour BaseSlider::getColour(Skin::ColorId colourId) const noexcept
	{ return parent_->getColour(colourId); }

	RotarySlider::RotarySlider(Framework::ParameterValue *parameter, String name) : BaseSlider(parameter, std::move(name))
	{
		setUseQuad(true);
		sliderQuad_.setMaxArc(kRotaryAngle);
		sliderQuad_.setFragmentShader(Shaders::kRotarySliderFragment);
		sliderQuad_.getAnimator().setHoverIncrement(0.2f);
		sliderQuad_.getAnimator().addTickCallback(Animator::Hover,
			[this](float value)
			{
				float thickness = findValue(Skin::kKnobArcThickness);
				sliderQuad_.setThickness(thickness + thickness * 0.15f * value);
			});

		label_.setValueEntry(textEntry_.get());
		label_.setTextJustification(Justification::topLeft);

		setRotaryParameters(2.0f * kPi - kRotaryAngle, 2.0f * kPi + kRotaryAngle, true);
		// TODO: think of a proper way of handling bipolar controls
		setBipolar(details_.minValue == -details_.maxValue);
		setShouldShowPopup(true);
	}

	void RotarySlider::mouseDrag(const MouseEvent &e)
	{
		float multiply = 1.0f;

		sensitiveMode_ = e.mods.isShiftDown();
		if (sensitiveMode_)
			multiply *= kSlowDragMultiplier;

		setMouseDragSensitivity((int)(kDefaultRotaryDragLength / (sensitivity_ * multiply)));

		BaseSlider::mouseDrag(e);
	}

	void RotarySlider::redoImage()
	{
		if (drawBounds_.getWidth() <= 0 || drawBounds_.getHeight() <= 0)
			return;

		float arc = sliderQuad_.getMaxArc();
		sliderQuad_.setShaderValue(0, std::lerp(-arc, arc, (float)getValue()));
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setAltColor(unselectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);
		sliderQuad_.setStartPos(isBipolar() ? 0.0f : -kPi);
	}

	void RotarySlider::setSliderBounds()
	{
		drawBounds_ = getBounds();
		auto width = (float)drawBounds_.getWidth();
		auto height = (float)drawBounds_.getHeight();

		float thickness = findValue(Skin::kKnobArcThickness);
		float size = findValue(Skin::kKnobArcSize) * getKnobSizeScale() + thickness;
		float radius_x = (size + 0.5f) / width;
		float radius_y = (size + 0.5f) / height;
		sliderQuad_.setQuad(0, -radius_x, -radius_y, 2.0f * radius_x, 2.0f * radius_y);
		sliderQuad_.setThumbAmount(findValue(Skin::kKnobHandleLength));
		redoImage();
	}

	void RotarySlider::drawShadow(Graphics &g)
	{
		Colour shadow_color = getColour(Skin::kShadow);

		auto width = (float)drawBounds_.getWidth();
		auto height = (float)drawBounds_.getHeight();

		float center_x = width / 2.0f;
		float center_y = height / 2.0f;
		float stroke_width = findValue(Skin::kKnobArcThickness);
		float radius = knobSizeScale_ * findValue(Skin::kKnobArcSize) / 2.0f;
		float shadow_width = findValue(Skin::kKnobShadowWidth);
		float shadow_offset = findValue(Skin::kKnobShadowOffset);

		PathStrokeType outer_stroke(stroke_width, PathStrokeType::beveled, PathStrokeType::rounded);
		PathStrokeType shadow_stroke(stroke_width + 1, PathStrokeType::beveled, PathStrokeType::rounded);

		g.saveState();
		g.setOrigin(getX(), getY());

		Colour body = getColour(Skin::kRotaryBody);
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

			ColourGradient borderGradient(getColour(Skin::kRotaryBodyBorder), center_x, 0.0f,
				body, center_x, 0.75f * height, false);

			g.setGradientFill(borderGradient);
			g.drawEllipse(ellipse.reduced(0.5f), 1.0f);
		}

		Path shadow_outline;
		Path shadow_path;

		shadow_outline.addCentredArc(center_x, center_y, radius, radius,
			0.0f, -kRotaryAngle, kRotaryAngle, true);
		shadow_stroke.createStrokedPath(shadow_path, shadow_outline);
		if ((!getColour(Skin::kRotaryArcUnselected).isTransparent() && isActive()) ||
			(!getColour(Skin::kRotaryArcUnselectedDisabled).isTransparent() && !isActive()))
		{
			g.setColour(shadow_color);
			g.fillPath(shadow_path);
		}

		g.restoreState();
	}

	void RotarySlider::showTextEntry()
	{
		/*auto width = (float)drawBounds_.getWidth();
		auto height = (float)drawBounds_.getHeight();

		int text_width = width;
		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = font_size;

		textEntry_->setBounds((getWidth() - text_width) / 2, (height - text_height + 1) / 2,
			text_width, text_height);*/

		BaseSlider::showTextEntry();
	}

	void LinearSlider::redoImage()
	{
		if (drawBounds_.getWidth() <= 0 || drawBounds_.getHeight() <= 0)
			return;

		sliderQuad_.setActive(true);
		auto t = (float)Framework::scaleValue(getValue(), details_, getSampleRate(), false, true);
		sliderQuad_.setShaderValue(0, t);
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setAltColor(unselectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);
		sliderQuad_.setStartPos(isBipolar_ ? 0.0f : -1.0f);

		int total_width = isHorizontal() ? drawBounds_.getHeight() : drawBounds_.getWidth();
		int extra = total_width % 2;
		auto slider_width = (float)(total_width + extra);

		sliderQuad_.setThickness(slider_width);
		sliderQuad_.setRounding(slider_width / 2.0f);
	}

	void LinearSlider::setSliderBounds()
	{
		if (isHorizontal())
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBounds_.getWidth();
			sliderQuad_.setQuad(0, -1.0f + margin, -1.0f, 2.0f - 2.0f * margin, 2.0f);
		}
		else
		{
			float margin = 2.0f * (findValue(Skin::kWidgetMargin) - 0.5f) / (float)drawBounds_.getHeight();
			sliderQuad_.setQuad(0, -1.0f, -1.0f + margin, 2.0f, 2.0f - 2.0f * margin);
		}
	}

	void LinearSlider::showTextEntry()
	{
		/*static constexpr float kTextEntryWidthRatio = 3.0f;

		float font_size = findValue(Skin::kTextComponentFontSize);
		int text_height = (int)(font_size);
		int text_width = (int)(text_height * kTextEntryWidthRatio);

		textEntry_->setBounds((drawBounds_.getWidth() - text_width) / 2, 
			(drawBounds_.getHeight() - text_height) / 2, text_width, text_height);*/

		BaseSlider::showTextEntry();
	}

	PinSlider::PinSlider(Framework::ParameterValue *parameter, String name) :
		BaseSlider(parameter, std::move(name), SliderStyle::LinearHorizontal)
	{
		setUseQuad(true);
		sliderQuad_.setFragmentShader(Shaders::kPinSliderFragment);
		setUseImage(true);
		setShouldShowPopup(true);
	}

	void PinSlider::mouseDown(const MouseEvent &e)
	{
		auto mouseEvent = e.getEventRelativeTo(parent_);
		lastDragPosition_ = mouseEvent.position.x;
		runningTotal_ = getValue();

		BaseSlider::mouseDown(mouseEvent);
	}

	void PinSlider::mouseDrag(const MouseEvent &e)
	{
		float multiply = 1.0f;

		sensitiveMode_ = e.mods.isShiftDown();
		if (sensitiveMode_)
			multiply *= kSlowDragMultiplier;

		auto mouseEvent = e.getEventRelativeTo(parent_);

		auto normalisedDiff = ((double)mouseEvent.position.x - lastDragPosition_) / totalRange_;
		runningTotal_ += multiply * normalisedDiff;
		setValue(std::clamp(runningTotal_, 0.0, 1.0));
		lastDragPosition_ = mouseEvent.position.x;

		setValueSafe(getValue());
		setValueToHost();

		if (!e.mods.isPopupMenu())
			showPopup(true);
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
		g.fillPath(pinPentagon, pinPentagon.getTransformToScaleToFit(drawBounds_.toFloat(), true, Justification::top));
	}

	void PinSlider::redoImage()
	{
		sliderQuad_.setColor(selectedColor_);
		sliderQuad_.setThumbColor(thumbColor_);

		imageComponent_.redrawImage();
	}

	void PinSlider::setSliderBounds()
	{
		if (areDrawBoundsSet_)
			sliderQuad_.setCustomDrawBounds(drawBounds_);
		else
			drawBounds_ = getLocalBounds();

		redoImage();
	}

	TextSelector::TextSelector(Framework::ParameterValue *parameter, 
		std::optional<Font> usedFont, String name) : BaseSlider(parameter, std::move(name))
	{
		setUseQuad(true);
		sliderQuad_.setFragmentShader(Shaders::kRoundedRectangleFragment);
		sliderQuad_.getAnimator().setHoverIncrement(0.2f);
		sliderQuad_.getAnimator().addTickCallback(Animator::Hover,
			[this](float value)
			{
				sliderQuad_.setColor(backgroundColor_.withMultipliedAlpha(value));
			});
		setUseImage(true);
		setIsImageOnTop(true);
		setCanInputValue(false);

		if (usedFont)
			usedFont_ = std::move(usedFont.value());
		else
			usedFont_ = Fonts::instance()->getInterVFont();
	}

	void TextSelector::mouseDown(const juce::MouseEvent &e)
	{
		updateValue();

		if (e.mods.isAltDown())
			return;

		if (e.mods.isPopupMenu())
		{
			PopupItems options = createPopupMenu();
			parent_->showPopupSelector(this, e.getPosition(), options,
				[this](int selection) { handlePopupResult(selection); });
			return;
		}

		// idk when this would happen but just to be sure
		if (details_.stringLookup.empty())
			return;

		PopupItems options;
		for (int i = 0; i <= (int)details_.maxValue; ++i)
			options.addItem(i, details_.stringLookup[i].data());

		parent_->showPopupSelector(this, Point{ 0, 0 }, options,
			[this](int value)
			{
				if (parameterLink_ && parameterLink_->hostControl)
					parameterLink_->hostControl->beginChangeGesture();

				beginChange(getValue());
				auto unscaledValue = unscaleValue(value, details_, getSampleRate());
				setValueSafe(unscaledValue);
				setValue(unscaledValue);
				setValueToHost();
				endChange();

				if (parameterLink_ && parameterLink_->hostControl)
					parameterLink_->hostControl->endChangeGesture();
			});
	}

	void TextSelector::mouseUp(const MouseEvent &e)
	{
		if (e.mods.isPopupMenu())
			BaseSlider::mouseUp(e);
	}

	void TextSelector::mouseWheelMove(const MouseEvent &e, const MouseWheelDetails &wheel)
	{
		auto newWheel = wheel;
		newWheel.isReversed = true;
		BaseSlider::mouseWheelMove(e, newWheel);
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

		auto height = drawBounds_.getHeight();
		auto leftOffset = height / 4 + drawBounds_.getX();

		String text = getSliderTextFromValue(getValue());
		g.setColour(selectedColor_);
		g.setFont(usedFont_);
		g.drawText(text, leftOffset, 0, textWidth_, height, Justification::centred, false);

		if (!drawArrow_)
			return;

		int arrowOffsetX = (int)std::round(kArrowOffsetRatio * (float)height);
		int arrowOffsetY = height / 2 - 1;
		float arrowWidth = (float)height * kHeightToArrowWidthRatio;

		Rectangle<int> arrowBounds;
		arrowBounds.setX(leftOffset + textWidth_ + arrowOffsetX);
		arrowBounds.setY(arrowOffsetY);
		arrowBounds.setWidth((int)std::round(arrowWidth));
		arrowBounds.setHeight((int)std::round(kArrowWidthHeightRatio * arrowWidth));

		g.setColour(selectedColor_);
		g.strokePath(arrowPath, PathStrokeType{ 1.0f, PathStrokeType::mitered, PathStrokeType::square }, 
			arrowPath.getTransformToScaleToFit(arrowBounds.toFloat(), true));
	}

	void TextSelector::redoImage()
	{
		sliderQuad_.setRounding(findValue(Skin::kWidgetRoundedCorner));
		imageComponent_.redrawImage();
	}

	void TextSelector::setSliderBounds()
	{
		if (!areDrawBoundsSet_)
			drawBounds_ = getLocalBounds();
		redoImage();
	}

	[[nodiscard]] int TextSelector::setHeight(int height) noexcept
	{
		if (getHeight() == height && !isDirty_)
			return totalDrawWidth_;

		float floatHeight = (float)height;
		Fonts::instance()->setFontForAscent(usedFont_, floatHeight * 0.5f);

		String text = getSliderTextFromValue(getValue(), false);
		textWidth_ = usedFont_.getStringWidth(text);
		float totalDrawWidth = (float)textWidth_;

		if (drawArrow_)
		{
			totalDrawWidth += floatHeight * kArrowOffsetRatio;
			totalDrawWidth += floatHeight * kHeightToArrowWidthRatio;
		}

		// there's always some padding at the beginning and end regardless whether the arrow is drawn
		totalDrawWidth += floatHeight * 0.5f;

		totalDrawWidth_ = (int)std::round(totalDrawWidth);
		isDirty_ = false;

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
		sliderQuad_.getAnimator().setHoverIncrement(0.2f);
		sliderQuad_.getAnimator().addTickCallback(Animator::Hover,
			[this](float value)
			{
				sliderQuad_.setColor(backgroundColor_.withMultipliedAlpha(value));
			});
	}

	void NumberBox::mouseDrag(const MouseEvent& e)
	{
		static constexpr float kNormalDragMultiplier = 0.5f;

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

		auto width = (float)drawBounds_.getWidth();
		auto height = (float)drawBounds_.getHeight();
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

	void NumberBox::setVisible(bool shouldBeVisible)
	{
		if (!shouldBeVisible)
		{
			sliderQuad_.setActive(false);
			imageComponent_.setActive(false);
			textEntry_->setVisible(false);
		}
		else
		{
			sliderQuad_.setActive(isUsingQuad());
			imageComponent_.setActive(isUsingImage());
			textEntry_->setVisible(true);
		}

		BaseSlider::setVisible(shouldBeVisible);
	}

	void NumberBox::textEditorReturnKeyPressed(TextEditor &editor)
	{
		updateValueFromTextEntry();

		for (SliderListener *listener : sliderListeners_)
			listener->menuFinished(this);

		textEditorEscapeKeyPressed(editor);
	}

	void NumberBox::textEditorEscapeKeyPressed(TextEditor &)
	{
		isEditing_ = false;
		textEntry_->giveAwayKeyboardFocus();
		textEntry_->setColour(TextEditor::textColourId, selectedColor_);
		textEntry_->setText(getSliderTextFromValue(getValue()));
	}

	void NumberBox::redoImage()
	{
		if (!isEditing_)
		{
			textEntry_->applyColourToAllText(selectedColor_);
			textEntry_->setText(getSliderTextFromValue(getValue()));
		}

		imageComponent_.redrawImage();
		textEntry_->redoImage();
		sliderQuad_.setRounding(findValue(Skin::kWidgetRoundedCorner));
		//sliderQuad_.setColor(backgroundColor_.withMultipliedAlpha(sliderQuad_.getAnimator().getValue(Animator::Click)));
		/*sliderQuad_.setColor(Colours::transparentBlack.interpolatedWith(backgroundColor_, 
			sliderQuad_.getAnimator().getValue(Animator::Hover)));*/
		/*if (isMouseOverOrDragging())
			sliderQuad_.setColor(backgroundColor_);
		else
			sliderQuad_.setColor(Colours::transparentBlack);*/
	}

	void NumberBox::setSliderBounds()
	{
		drawBounds_ = getLocalBounds();
		auto bounds = drawBounds_.toFloat();
		auto height = (float)drawBounds_.getHeight();
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
		textEntry_->setColour(CaretComponent::caretColourId, getColour(Skin::kTextEditorCaret));
		textEntry_->setColour(TextEditor::textColourId, getColour(Skin::kNormalText));
		textEntry_->setColour(TextEditor::highlightedTextColourId, getColour(Skin::kNormalText));
		textEntry_->setColour(TextEditor::highlightColourId, getColour(Skin::kTextEditorSelection));

		isEditing_ = true;

		BaseSlider::showTextEntry();
	}

	[[nodiscard]] int NumberBox::setHeight(int height) noexcept
	{
		if (getHeight() == height)
			return totalDrawWidth_;

		auto floatHeight = (float)height;
		Fonts::instance()->setFontForAscent(usedFont_, floatHeight * 0.5f);
		textEntry_->setUsedFont(usedFont_);

		float totalDrawWidth = getNumericTextMaxWidth(usedFont_);
		if (drawBackground_)
		{
			totalDrawWidth += floatHeight * kTriangleWidthRatio;
			totalDrawWidth += kTriangleToValueMarginRatio * floatHeight;
			totalDrawWidth += kValueToEndMarginRatio * floatHeight;
		}
		else
		{
			// extra space around the value
			totalDrawWidth += floatHeight * 0.5f;
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
