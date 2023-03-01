/*
	==============================================================================

		DraggableEffect.cpp
		Created: 10 Feb 2023 5:50:16am
		Author:  theuser27

	==============================================================================
*/

#include "DraggableEffect.h"

namespace Interface
{
	void DraggableComponent::mouseMove(const MouseEvent &e)
	{
		if (dragHitbox_.contains(e.x, e.y))
		{
			hover_ = true;
			setMouseCursor(MouseCursor(MouseCursor::StandardCursorType::DraggingHandCursor));
		}
		else
		{
			hover_ = false;
			setMouseCursor(MouseCursor(MouseCursor::StandardCursorType::NormalCursor));
		}

		BaseSection::mouseMove(e);
	}

	void DraggableComponent::mouseDown(const MouseEvent &e)
	{
		if (dragHitbox_.contains(e.x, e.y))
		{
			
		}
	}

	void DraggableComponent::mouseDrag(const MouseEvent &e)
	{

	}

	void DraggableComponent::mouseUp(const MouseEvent &e)
	{
		
	}

}
