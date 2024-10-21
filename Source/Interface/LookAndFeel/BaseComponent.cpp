/*
  ==============================================================================

    BaseComponent.cpp
    Created: 11 Dec 2023 4:49:17pm
    Author:  theuser27

  ==============================================================================
*/

#include "BaseComponent.hpp"

namespace Interface
{
  void BaseComponent::parentHierarchyChanged()
  {
    if (auto *parent = getParentComponent())
      setParentSafe(utils::as<BaseComponent>(parent));
  }

  void BaseComponent::setBounds(int x, int y, int w, int h)
  {
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    if (getBounds() != juce::Rectangle{ x, y, w, h })
      boundsSafe_ = juce::Rectangle{ x, y, w, h };

    Component::setBounds(x, y, w, h);
  }

  bool BaseComponent::redirectMouse(RedirectMouse type, const juce::MouseEvent &e, 
    const juce::MouseWheelDetails *wheel, bool findFromParent) const
  {
    auto *destination = redirectMouse_;
    // if there isn't a specified destination try to find one from parents
    if (!destination)
    {
      if (!findFromParent)
        return false;

      auto *parent = getParentSafe();
      while (parent)
      {
        if (parent->redirectMouse_)
        {
          destination = parent->redirectMouse_;
          break;
        }
        parent = parent->getParentSafe();
      }

      if (!destination)
        return false;
    }

    switch (type)
    {
    case RedirectMouseDown:
      destination->mouseDown(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseDrag:
      destination->mouseDrag(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseUp:
      destination->mouseUp(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseMove:
      destination->mouseMove(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseEnter:
      destination->mouseEnter(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseExit:
      destination->mouseExit(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseDoubleClick:
      destination->mouseDoubleClick(e.getEventRelativeTo(destination));
      break;
    case RedirectMouseWheel:
      COMPLEX_ASSERT(wheel != nullptr && "Mouse wheel details were not provided when redirecting mouse event");
      destination->mouseWheelMove(e.getEventRelativeTo(destination), *wheel);
      break;
    default:
      COMPLEX_ASSERT(false && "Uncaught case for redirecting mouse");
      break;
    }

    return true;
  }

  bool BaseComponent::needsToRedirectMouse(const juce::MouseEvent &e) const noexcept
  {
    if (e.mods == juce::ModifierKeys::noModifiers)
      return false;

    // handling kb modifiers (99% of the time masking for mouse wheel events)
    if (auto mods = e.mods.withoutMouseButtons(); mods != juce::ModifierKeys::noModifiers && 
      mods == redirectMods_.withoutMouseButtons())
      return true;

    // handling mouse clicks (99% of the time masking only for middle mouse click)
    auto mods = e.mods.withOnlyMouseButtons();
    return mods != juce::ModifierKeys::noModifiers && mods == redirectMods_.withOnlyMouseButtons();
  }
}
