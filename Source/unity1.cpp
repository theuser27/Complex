
#include "Framework/fourier_transform.cpp"
#include "Framework/load_save.cpp"
#include "Framework/parameters.cpp"

#include "Generation/Processor.cpp"
#include "Generation/Algorithms.cpp"
#include "Generation/Effects.cpp"
#include "Generation/SoundEngine.cpp"

#include "Interface/Control.cpp"
#include "Interface/Controls.cpp"
#include "Interface/DraggableComponent.cpp"
#include "Interface/Spectrogram.cpp"
#include "Interface/TextEditor.cpp"

#include "Interface/Component.cpp"
#include "Interface/Graphics.cpp"
#include "Interface/gui_utils.cpp"
#include "Interface/Skin.cpp"

#include "Interface/EffectModuleSection.cpp"
#include "Interface/EffectsLaneSection.cpp"
#include "Interface/MainInterface.cpp"
#include "Interface/Popups.cpp"

#include "Plugin/Complex.cpp"
#include "Plugin/Renderer.cpp"

#if COMPLEX_MSVC
	#include "crt/crt.cpp"
#endif
