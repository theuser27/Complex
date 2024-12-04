/*
  ==============================================================================

    BaseControl.hpp
    Created: 31 Jul 2023 7:37:15pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include "Framework/vector_map.hpp"
#include "Framework/parameters.hpp"
#include "OpenGlContainer.hpp"

namespace Framework
{
  class ParameterValue;
  struct ParameterLink;
  class ParameterBridge;
}

namespace Interface
{
  class ControlListener;
  class PlainTextComponent;
  class BaseSection;

  class BaseControl : public OpenGlContainer
  {
  public:
    enum MenuId
    {
      kCancel = 0,
      kMidiLearn,
      kClearMidiLearn,
      kDefaultValue,
      kManualEntry,
      kCopyNormalisedValue,
      kCopyValue,
      kPasteValue,
      kClearModulations,
      kMapFirstSlot,
      kClearMapping,
      kControlMenuIdsSize,
      kMappingList = 64,
    };
    static constexpr bool isUnmappingParameter(int id) { return id % 2 != kMappingList % 2; }

    BaseControl();
    ~BaseControl() override;

    // ====================================================== Parameter related
    Framework::ParameterDetails getParameterDetails() const noexcept { return details_; }
    virtual void setParameterDetails(const Framework::ParameterDetails &details) { details_ = details; }

    Framework::ParameterLink *getParameterLink() const noexcept { return parameterLink_; }
    // returns the replaced link
    Framework::ParameterLink *setParameterLink(Framework::ParameterLink *parameterLink) noexcept;

    virtual Framework::ParameterValue *changeLinkedParameter(Framework::ParameterValue &parameter,
      bool getValueFromParameter = true);

    bool setValueFromHost(double value, Framework::ParameterBridge *notifyingBridge) noexcept;
    void setValueFromParameter() noexcept;
    void setValueToHost() const noexcept;
    void setValueToParameter() const noexcept;

    double getValueRaw() const noexcept { return value_.load(std::memory_order_acquire); }
    void setValueRaw(double newValue) noexcept { value_.store(newValue, std::memory_order_release); }
    virtual void setValue(double newValue, 
      juce::NotificationType notification = juce::sendNotificationSync) = 0;
    virtual void valueChanged();

    virtual juce::String getScaledValueString(double value, bool addPrefix = true) const = 0;
    static juce::String getNormalisedValueString(double value)
    {
      static constexpr auto kMaxDecimalCount = 5;
      return juce::String{ value, kMaxDecimalCount };
    }
    double getValueFromText(const juce::String &text) const;

    virtual void showTextEntry() = 0;
    void setResetValue(double resetValue) noexcept { resetValue_ = resetValue; }
    bool hasParameter() const noexcept { return hasParameter_; }
    void handlePopupResult(int result);

    void beginChange(double oldValue) noexcept;
    void endChange();

    void addListener(auto *listener) { controlListeners_.emplace_back(listener); }
    void removeListener(auto *listener) { std::erase(controlListeners_, listener); }

    // ========================================================= Layout related
    void resized() override;
    void moved() override;
    void parentHierarchyChanged() override;

    auto getDrawBounds() const noexcept { return drawBounds_; }
    auto getAddedHitbox() const noexcept { return addedHitbox_; }
    
    // returns tight bounds around all contained elements (drawn components, label, etc.)
    // by the end of this method drawBounds need to have been set to encompass the drawn components
    virtual juce::Rectangle<int> setSizes(int height, int width = 0) = 0;
    // sets the bounds based on drawBounds + position + added hitbox
    // call after initialising drawBounds with setSizes
    void setPosition(juce::Point<int> position);
    void setBounds(int x, int y, int width, int height) final;
    using OpenGlContainer::setBounds;
    // sets extra outer size
    void setAddedHitbox(juce::BorderSize<int> addedHitBox) noexcept { addedHitbox_ = addedHitBox; }
    // positions extra external elements (label, combined control, etc.) relative to drawBounds
    virtual void setExtraElementsPositions(juce::Rectangle<int> anchorBounds) = 0;
    void repositionExtraElements();

    // ====================================================== Rendering related
    void renderOpenGlComponents(OpenGlWrapper &openGl) final;
    void paint(juce::Graphics &) override { }
    // redraws components when an action occurs
    virtual void redoImage() = 0;
    // sets positions of all drawable components relative to drawBounds
    // drawBounds is guaranteed to be a valid area
    virtual void setComponentsBounds(bool redoImage = true) = 0;
    // refreshes colours from Skin
    virtual void setColours();

    // ========================================================== Label related
    void addLabel();
    void removeLabel();
    
    // ========================================================== Miscellaneous
    using OpenGlContainer::getValue;
    bool isActive() const noexcept { return isActive_; }

    void setLabelPlacement(Placement placement) { labelPlacement_ = placement; }
    void setShouldRepaintOnHover(bool shouldRepaintOnHover) noexcept { shouldRepaintOnHover_ = shouldRepaintOnHover; }

  protected:
    void resetValue() noexcept;
    PopupItems createPopupMenu() const noexcept;
    juce::Rectangle<int> getUnionOfAllElements() const noexcept;

    // ============================================================== Variables
    std::atomic<double> value_ = 0.0;
    double valueBeforeChange_ = 0.0;
    double resetValue_ = 0.0;
    bool hasBegunChange_ = false;

    bool hasParameter_ = false;
    bool isActive_ = true;
    bool canInputValue_ = false;

    Framework::ParameterLink *parameterLink_ = nullptr;
    Framework::ParameterDetails details_{};

    // drawBounds is a space inside getLocalBounds() where components are being drawn
    juce::Rectangle<int> drawBounds_{};
    // this determines how much to extend the bounds relative to drawBounds so that the hitbox is larger
    juce::BorderSize<int> addedHitbox_{};

    // extra stuff like label or modifying control (i.e. textSelector changing behaviour of a knob)
    // and their bounds relative to the drawBounds
    Framework::VectorMap<BaseComponent *, juce::Rectangle<int>> extraElements_{};
    utils::up<PlainTextComponent> label_;
    Placement labelPlacement_ = Placement::right;

    bool shouldRepaintOnHover_ = false;

    BaseSection *parent_ = nullptr;
    std::vector<ControlListener *> controlListeners_{};
  };

  // responsible for positioning of multiple controls
  // e.g. arranged in a horizontal strip
  class ControlContainer : public juce::ComponentListener
  {
  public:
    ~ControlContainer() override
    {
      for (auto &[control, _] : controls_)
        control->removeComponentListener(this);
    }

    void componentBeingDeleted(juce::Component &component) override 
    {
      std::erase_if(controls_, [&](auto element) { return element.first == &component; });
      component.removeComponentListener(this);
      repositionControls();
    }
    void componentMovedOrResized(juce::Component &, bool, bool) override { repositionControls(); }
    void componentVisibilityChanged(juce::Component &) override { repositionControls(); }

    void addControl(BaseControl *control)
    {
      COMPLEX_ASSERT(std::ranges::find_if(controls_, [&](auto element) 
        { return element.first == control; }) == controls_.end());
      controls_.emplace_back(control, std::pair<int, int>{});
      control->addComponentListener(this);
    }
    void deleteControl(BaseControl *control) 
    {
      std::erase_if(controls_, [&](auto element) { return element.first == control; });
      control->removeComponentListener(this);
    }
    
    void setAnchor(Placement anchor) noexcept { anchor_ = anchor; }
    void setParentAndBounds(BaseComponent *parent, juce::Rectangle<int> bounds) noexcept
    { parent_ = parent; bounds_ = bounds; }

    void setControlSizes(BaseControl *control, int height, int width = 0) noexcept
    {
      auto iter = std::ranges::find_if(controls_, 
        [&](auto element) { return element.first == control; });
      COMPLEX_ASSERT(iter != controls_.end());
      iter->second = { width, height };
    }
    void setControlSpacing(int spacing) noexcept { controlSpacing_ = spacing; }

    void repositionControls();

  private:
    BaseComponent *parent_ = nullptr;
    juce::Rectangle<int> bounds_{};
    std::vector<std::pair<BaseControl *, std::pair<int, int>>> controls_{};
    int controlSpacing_ = 0;
    Placement anchor_ = Placement::centerVertical | Placement::left;
    bool isArranging_ = false;
  };

}
