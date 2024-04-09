/*
	==============================================================================

		DraggableComponent.h
		Created: 10 Feb 2023 5:50:16am
		Author:  theuser27

	==============================================================================
*/

#pragma once

#include "../LookAndFeel/BaseComponent.h"

namespace Interface
{
	class EffectModuleSection;

	class DraggableComponent final : public BaseComponent
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;
			virtual void prepareToMove(EffectModuleSection *component, const juce::MouseEvent &e, bool isCopying) = 0;
			virtual void draggingComponent(EffectModuleSection *component, const juce::MouseEvent &e) = 0;
			virtual void releaseComponent(EffectModuleSection *component, const juce::MouseEvent &e) = 0;
		};

		DraggableComponent() noexcept { setInterceptsMouseClicks(true, false); }

		void paint(juce::Graphics &g) override;

		void mouseMove(const juce::MouseEvent &e) override;
		void mouseDown(const juce::MouseEvent &e) override;
		void mouseDrag(const juce::MouseEvent &e) override;
		void mouseUp(const juce::MouseEvent &e) override;
		void mouseExit(const juce::MouseEvent &e) override;

		auto *getDraggedComponent() const noexcept { return draggedComponent_; }
		void setDraggedComponent(EffectModuleSection *draggedComponent) noexcept { draggedComponent_ = draggedComponent; }
		void setListener(Listener *listener) noexcept { listener_ = listener; }

	private:
		EffectModuleSection *draggedComponent_ = nullptr;
		Listener *listener_ = nullptr;
	};
}
