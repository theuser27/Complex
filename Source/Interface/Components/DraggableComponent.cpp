/*
	==============================================================================

		DraggableComponent.cpp
		Created: 10 Feb 2023 5:50:16am
		Author:  theuser27

	==============================================================================
*/

#include "DraggableComponent.h"
#include "../Sections/EffectModuleSection.h"

namespace Interface
{
	void DraggableComponent::paint(Graphics &g)
	{
		static constexpr float kDotDiameter = 2.0f;
		static constexpr float kDotsOffset = 6.0f;

		auto dotsDiameter = getDraggedComponent()->scaleValueRoundInt(kDotDiameter);
		auto dotsOffset = getDraggedComponent()->scaleValueRoundInt(kDotsOffset);

		auto centeredX = BaseSection::centerHorizontally(0, dotsDiameter + dotsOffset, getWidth());
		auto centeredY = BaseSection::centerVertically(0, dotsDiameter + dotsOffset, getHeight());

		auto centreRectangle = Rectangle{ centeredX, centeredY, dotsOffset, dotsOffset }.toFloat();

		g.setColour(getDraggedComponent()->getColour(Skin::kWidgetSecondary1));
		g.fillEllipse(centreRectangle.getX(), centreRectangle.getY(), kDotDiameter, kDotDiameter);
		g.fillEllipse(centreRectangle.getX(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
		g.fillEllipse(centreRectangle.getRight(), centreRectangle.getY(), kDotDiameter, kDotDiameter);
		g.fillEllipse(centreRectangle.getRight(), centreRectangle.getBottom(), kDotDiameter, kDotDiameter);
	}

	void DraggableComponent::mouseMove(const MouseEvent &e)
	{
		setMouseCursor(MouseCursor(MouseCursor::StandardCursorType::DraggingHandCursor));
		BaseComponent::mouseMove(e);
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
		BaseComponent::mouseExit(e);
	}

}
