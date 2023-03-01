/*
  ==============================================================================

    OpenGLComponent.cpp
    Created: 5 Dec 2022 11:55:29pm
    Author:  theuser27

  ==============================================================================
*/

#include "OpenGlComponent.h"

#include "OpenGlMultiQuad.h"
#include "../Sections/MainInterface.h"

namespace
{
  Rectangle<int> getGlobalBounds(const Component *component, Rectangle<int> bounds)
  {
    Component *parent = component->getParentComponent();
    while (parent && dynamic_cast<const Interface::MainInterface *>(component) == nullptr)
    {
      bounds = bounds + component->getPosition();
      component = parent;
      parent = component->getParentComponent();
    }

    return bounds;
  }

  Rectangle<int> getGlobalVisibleBounds(const Component *component, Rectangle<int> visibleBounds)
  {
    Component *parent = component->getParentComponent();
    while (parent && dynamic_cast<Interface::MainInterface *>(parent) == nullptr)
    {
      visibleBounds = visibleBounds + component->getPosition();
      parent->getLocalBounds().intersectRectangle(visibleBounds);
      component = parent;
      parent = component->getParentComponent();
    }

    return visibleBounds + component->getPosition();
  }
}

namespace Interface
{
  using namespace juce::gl;

  bool OpenGlComponent::setViewPort(Component *component, Rectangle<int> bounds, OpenGlWrapper &openGl)
  {
    MainInterface *topLevel = component->findParentComponentOfClass<MainInterface>();
    float scale = openGl.display_scale;
    float resizeScale = topLevel->getResizingScale();
    float renderScale = 1.0f;
    if (scale == 1.0f)
      renderScale *= openGl.context.getRenderingScale();

    float glScale = renderScale * resizeScale;

    Rectangle<int> top_level_bounds = topLevel->getBounds();
    Rectangle<int> global_bounds = getGlobalBounds(component, bounds);
    Rectangle<int> visible_bounds = getGlobalVisibleBounds(component, bounds);

    glViewport(glScale * global_bounds.getX(),
      std::ceil(scale * renderScale * top_level_bounds.getHeight()) - glScale * global_bounds.getBottom(),
      glScale * global_bounds.getWidth(), glScale * global_bounds.getHeight());

    if (visible_bounds.getWidth() <= 0 || visible_bounds.getHeight() <= 0)
      return false;

    glScissor(glScale * visible_bounds.getX(),
      std::ceil(scale * renderScale * top_level_bounds.getHeight()) - glScale * visible_bounds.getBottom(),
      glScale * visible_bounds.getWidth(), glScale * visible_bounds.getHeight());

    return true;
  }

  bool OpenGlComponent::setViewPort(Component *component, OpenGlWrapper &openGl)
  { return setViewPort(component, component->getLocalBounds(), openGl); }

  bool OpenGlComponent::setViewPort(OpenGlWrapper &openGl)
  { return setViewPort(this, openGl); }

  void OpenGlComponent::setScissor(Component *component, OpenGlWrapper &openGl)
  { setScissorBounds(component, component->getLocalBounds(), openGl); }

  void OpenGlComponent::setScissorBounds(Component *component, Rectangle<int> bounds, OpenGlWrapper &openGl)
  {
    if (component == nullptr)
      return;

    MainInterface *top_level = component->findParentComponentOfClass<MainInterface>();
    float scale = openGl.display_scale;
    float resizeScale = top_level->getResizingScale();
    float renderScale = 1.0f;
    if (scale == 1.0f)
      renderScale *= openGl.context.getRenderingScale();

    float glScale = renderScale * resizeScale;

    Rectangle<int> top_level_bounds = top_level->getBounds();
    Rectangle<int> visible_bounds = getGlobalVisibleBounds(component, bounds);

    if (visible_bounds.getHeight() > 0 && visible_bounds.getWidth() > 0)
    {
      glScissor(glScale * visible_bounds.getX(),
        std::ceil(scale * renderScale * top_level_bounds.getHeight()) - glScale * visible_bounds.getBottom(),
        glScale * visible_bounds.getWidth(), glScale * visible_bounds.getHeight());
    }
  }

  void OpenGlComponent::paint(Graphics &g)
  {
    if (!isVisible())
      return;

    g.fillAll(findColour(Skin::kWidgetBackground, true));
  }

  void OpenGlComponent::repaintBackground()
  {
    if (!isShowing())
      return;

    if (MainInterface *parent = findParentComponentOfClass<MainInterface>())
      parent->repaintOpenGlBackground(this);
  }

  void OpenGlComponent::resized()
  {
    if (corners_)
      corners_->setBounds(getLocalBounds());

    bodyColor_ = findColour(Skin::kBody, true);
  }

  void OpenGlComponent::addRoundedCorners()
  {
    corners_ = std::make_unique<OpenGlCorners>();
    addAndMakeVisible(corners_.get());
  }

  void OpenGlComponent::addBottomRoundedCorners()
  {
    onlyBottomCorners_ = true;
    addRoundedCorners();
  }

  void OpenGlComponent::init(OpenGlWrapper &openGl)
  {
    if (corners_)
      corners_->init(openGl);
  }

  void OpenGlComponent::renderCorners(OpenGlWrapper &openGl, bool animate, Colour color, float rounding)
  {
    if (corners_)
    {
      if (onlyBottomCorners_)
        corners_->setBottomCorners(getLocalBounds(), rounding);
      else
        corners_->setCorners(getLocalBounds(), rounding);
      corners_->setColor(color);
      corners_->render(openGl, animate);
    }
  }

  void OpenGlComponent::renderCorners(OpenGlWrapper &openGl, bool animate)
  {
    renderCorners(openGl, animate, bodyColor_, findValue(Skin::kWidgetRoundedCorner));
  }

  void OpenGlComponent::destroy(OpenGlWrapper &openGl)
  {
    if (corners_)
      corners_->destroy(openGl);
  }

  float OpenGlComponent::findValue(Skin::ValueId valueId) const 
  {
    if (parent_)
      return parent_->findValue(valueId);

    return 0.0f;
  }
}
