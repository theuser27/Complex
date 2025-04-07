/*
  ==============================================================================

    BaseComponent.hpp
    Created: 11 Dec 2023 4:49:17pm
    Author:  theuser27

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Framework/utils.hpp"
#include "Framework/sync_primitives.hpp"

namespace Interface
{
  class BaseComponent;

  struct ViewportChange
  {
    BaseComponent *component = nullptr;
    juce::Rectangle<int> change{};
    bool isClipping = true;

    constexpr bool operator==(const ViewportChange &) const noexcept = default;
  };

  class BaseComponent : public juce::Component
  {
  public:
    enum RedirectMouse { RedirectMouseWheel, RedirectMouseDown, RedirectMouseDrag, RedirectMouseUp, RedirectMouseMove,
      RedirectMouseEnter, RedirectMouseExit, RedirectMouseDoubleClick };

    class ScopedBoundsEmplace
    {
    public:
      static constexpr auto doNotAddFlag = ViewportChange{ nullptr, {}, true };
      static constexpr auto doNotClipFlag = ViewportChange{ nullptr, {}, false };

      ScopedBoundsEmplace(std::vector<ViewportChange> &vector, BaseComponent *component) :
        ScopedBoundsEmplace{ vector, component, component->getBoundsSafe() } { }

      ScopedBoundsEmplace(std::vector<ViewportChange> &vector, BaseComponent *component, 
        juce::Rectangle<int> bounds) : vector_(vector)
      {
        shouldAdd_ = vector.back() != doNotAddFlag;
        if (!shouldAdd_)
          vector.erase(vector.end() - 1);
        else if (vector.back() == doNotClipFlag)
          vector.back() = { component, bounds, false };
        else
          vector.emplace_back(component, bounds, true);
      }

      ~ScopedBoundsEmplace() noexcept { if (shouldAdd_) vector_.pop_back(); }
    private:
      std::vector<ViewportChange> &vector_;
      bool shouldAdd_;
    };

    class ScopedIgnoreClip
    {
    public:
      ScopedIgnoreClip(std::vector<ViewportChange> &vector, const BaseComponent *ignoreClipIncluding) : vector_(vector)
      {
        COMPLEX_ASSERT(vector.back() != ScopedBoundsEmplace::doNotClipFlag);
        COMPLEX_ASSERT(vector.back() != ScopedBoundsEmplace::doNotAddFlag);
        
        if (ignoreClipIncluding == nullptr)
        {
          i_ = vector.size();
          return;
        }

        for (i_ = vector.size() - 1; i_ > 0; --i_)
        {
          vector[i_].isClipping = false;
          if (vector[i_].component == ignoreClipIncluding)
            break;
        }
      }

      ~ScopedIgnoreClip() noexcept
      {
        for (; i_ < vector_.size(); ++i_)
          vector_[i_].isClipping = true;
      }
    private:
      std::vector<ViewportChange> &vector_;
      size_t i_;
    };

    BaseComponent(juce::String name = juce::String()) : Component{ name } { }

    void parentHierarchyChanged() override;
    void paint(juce::Graphics &) override { }
    // if you get an error here declare setBounds(int, int, int, int) in juce::Component as virtual
    void setBounds(int x, int y, int w, int h) override;
    using Component::setBounds;
    void setBoundsSafe(juce::Rectangle<int> bounds) noexcept { boundsSafe_ = bounds; }
    void setBoundsSafe(int x, int y, int w, int h) noexcept { setBoundsSafe(juce::Rectangle{ x, y, w, h }); }

    void setVisible(bool shouldBeVisible) override
    {
      isVisibleSafe_ = shouldBeVisible;
      Component::setVisible(shouldBeVisible);
    }

    // if you get an error here declare setAlwaysOnTop(bool) in juce::Component as virtual
    void setAlwaysOnTop(bool shouldStayOnTop) final
    {
      isAlwaysOnTopSafe_ = shouldStayOnTop;
      Component::setAlwaysOnTop(shouldStayOnTop);
    }

    bool isVisibleSafe() const noexcept { return isVisibleSafe_.get(); }
    bool isAlwaysOnTopSafe() const noexcept { return isAlwaysOnTopSafe_.get(); }

    juce::Rectangle<int> getBoundsSafe() const noexcept { return boundsSafe_.get(); }
    juce::Rectangle<int> getLocalBoundsSafe() const noexcept { return boundsSafe_.get().withZeroOrigin(); }
    juce::Point<int> getPositionSafe() const noexcept { return boundsSafe_.get().getPosition(); }

    int getWidthSafe() const noexcept { return getLocalBoundsSafe().getWidth(); }
    int getHeightSafe() const noexcept { return getLocalBoundsSafe().getHeight(); }

    BaseComponent *getParentSafe() const noexcept { return parentSafe_.get(); }
    void setParentSafe(BaseComponent *parent) noexcept { parentSafe_ = parent; }
    
    bool redirectMouse(RedirectMouse type, const juce::MouseEvent &e,
      const juce::MouseWheelDetails *wheel = nullptr, bool findFromParent = true) const;
    bool needsToRedirectMouse(const juce::MouseEvent &e) const noexcept;
    void setRedirectMouseToComponent(BaseComponent *component) { redirectMouse_ = component; }
    void setRedirectMouseModifiers(juce::ModifierKeys redirectMods) { redirectMods_ = redirectMods; }

  protected:
    utils::shared_value<BaseComponent *> parentSafe_ = nullptr;
    utils::shared_value<juce::Rectangle<int>> boundsSafe_{};
    utils::shared_value<bool> isVisibleSafe_ = false;
    utils::shared_value<bool> isAlwaysOnTopSafe_ = false;

    juce::ModifierKeys redirectMods_{};
    BaseComponent *redirectMouse_ = nullptr;
  private:
    COMPLEX_MAKE_LIVENESS_CHECKED;
  };
}
