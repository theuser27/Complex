/*
	==============================================================================

		DraggableComponent.h
		Created: 10 Feb 2023 5:50:16am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../Sections/BaseSection.h"
#include "OpenGlImageComponent.h"

namespace Interface
{
	class EffectModuleSection;

	class DraggableComponent : public Component
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void prepareToMove(EffectModuleSection *component, const MouseEvent &e, bool isCopying) = 0;
			virtual void draggingComponent(EffectModuleSection *component, const MouseEvent &e) = 0;
			virtual void releaseComponent(EffectModuleSection *component, const MouseEvent &e) = 0;
		};

		DraggableComponent() noexcept { setInterceptsMouseClicks(true, false); }

		void paint(Graphics &g) override;

		void mouseMove(const MouseEvent &e) override;
		void mouseDown(const MouseEvent &e) override;
		void mouseDrag(const MouseEvent &e) override;
		void mouseUp(const MouseEvent &e) override;
		void mouseExit(const MouseEvent &e) override;

		auto *getDraggedComponent() const noexcept { return draggedComponent_; }
		void setDraggedComponent(EffectModuleSection *draggedComponent) noexcept { draggedComponent_ = draggedComponent; }
		void setListener(Listener *listener) noexcept { listener_ = listener; }

	private:
		EffectModuleSection *draggedComponent_ = nullptr;
		Listener *listener_ = nullptr;
	};
}
