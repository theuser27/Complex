/*
	==============================================================================

		DraggableComponent.cpp
		Created: 10 Feb 2023 5:50:16am
		Author:  theuser27

	==============================================================================
*/

#include "DraggableComponent.h"

namespace Interface
{
	void DraggableComponent::paint(Graphics &g)
	{
		static constexpr float kDotDiameter = 2;
		auto height = (float)getHeight();
		auto centreRectangle = Rectangle<float>{ (float)getWidth() / 2 - height / 7 + kDotDiameter,
			height / 2 - height / 7 + kDotDiameter, height / 7 - kDotDiameter, height / 7 - kDotDiameter };

		g.setColour(findColour(Skin::kBody, true));
		g.fillRoundedRectangle(centreRectangle.getX(), centreRectangle.getY(), kDotDiameter, kDotDiameter, kDotDiameter / 2.0f);
		g.fillRoundedRectangle(centreRectangle.getX(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter, kDotDiameter / 2.0f);
		g.fillRoundedRectangle(centreRectangle.getRight(), centreRectangle.getY(), kDotDiameter, kDotDiameter, kDotDiameter / 2.0f);
		g.fillRoundedRectangle(centreRectangle.getRight(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter, kDotDiameter / 2.0f);
	}

	void DraggableComponent::mouseMove(const MouseEvent &e)
	{
		setMouseCursor(MouseCursor(MouseCursor::StandardCursorType::DraggingHandCursor));
		Component::mouseMove(e);
	}

	void DraggableComponent::mouseDown(const MouseEvent &e)
	{
		listener_->prepareToMove(draggedComponent_, e, e.mods.isCommandDown());
	}

	void DraggableComponent::mouseDrag(const MouseEvent &e)
	{
		listener_->draggingComponent(draggedComponent_, e);
	}

	void DraggableComponent::mouseUp(const MouseEvent &e)
	{
		listener_->releaseComponent(draggedComponent_, e);
	}

	void DraggableComponent::mouseExit(const MouseEvent &e)
	{
		setMouseCursor(MouseCursor(MouseCursor::StandardCursorType::NormalCursor));
		Component::mouseExit(e);
	}

}
