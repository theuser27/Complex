cmake_minimum_required(VERSION 3.24)
project("Complex" VERSION 0.1.1 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set_property(GLOBAL PROPERTY USE_FOLDERS true)

set(TargetName ${PROJECT_NAME})

set(CMAKE_BUILD_PARALLEL_LEVEL 4)

add_subdirectory(JUCE)


set(COMPLEX_COPY_PLUGIN_PATH "" CACHE STRING "Where the vst3 will be copied to after building")

juce_add_plugin(${TargetName}
	PRODUCT_NAME ${TargetName}
	COMPANY_NAME "theuser27"
	DESCRIPTION "Multi-lane Spectral Effects Suite"
	IS_SYNTH FALSE
	NEEDS_MIDI_INPUT FALSE
	NEEDS_MIDI_OUTPUT FALSE
	IS_MIDI_EFFECT FALSE
	EDITOR_WANTS_KEYBOARD_FOCUS FALSE
	COPY_PLUGIN_AFTER_BUILD TRUE
	VST3_COPY_DIR "${COMPLEX_COPY_PLUGIN_PATH}"
	PLUGIN_MANUFACTURER_CODE Tu27
	PLUGIN_CODE Cplx
	FORMATS VST3 Standalone
)

file(GLOB_RECURSE fileSources CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c
)
target_sources(${TargetName} PRIVATE ${fileSources})

target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "Source" FILES ${fileSources})

option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Enable Module Source Groups" ON)

target_compile_definitions(${TargetName}
	PUBLIC
		JUCE_STRICT_REFCOUNTEDPOINTER=1
		JUCE_WEB_BROWSER=0
		JUCE_USE_CURL=0
		JUCE_VST3_CAN_REPLACE_VST2=0
)

file(GLOB resourceFiles
	${CMAKE_CURRENT_SOURCE_DIR}/Fonts/*.ttf
	${CMAKE_CURRENT_SOURCE_DIR}/Icons/*.svg
	${CMAKE_CURRENT_SOURCE_DIR}/Complex.skin
)
juce_add_binary_data(BinaryData SOURCES ${resourceFiles})

option(COMPLEX_USE_INTEL_IPP "Avaliable on Windows and Linux if you have oneAPI installed, Intel IPP should come with cmake support" OFF)
if(COMPLEX_USE_INTEL_IPP)
	find_package(IPP)
	if(IPP_FOUND)
  		target_link_libraries(${TargetName} PRIVATE IPP::ipps)
		target_compile_definitions(${TargetName} PRIVATE INTEL_IPP=1)
	endif()
endif()

target_link_libraries(${TargetName}
	PRIVATE
		BinaryData
		juce::juce_audio_basics
		juce::juce_audio_devices
		juce::juce_audio_plugin_client
		juce::juce_audio_processors
		juce::juce_core
		juce::juce_data_structures
		juce::juce_events
		juce::juce_graphics
		juce::juce_gui_basics
		juce::juce_opengl
	PUBLIC
		juce::juce_recommended_config_flags
		juce::juce_recommended_lto_flags
		juce::juce_recommended_warning_flags
)


if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")

endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)|(x86_64)")
  option(COMPLEX_ENABLE_FMA "Choose to enable FMA (fused multiply-add)" ON)
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(COMPLEX_SIMD_OPTIMISATION_OPTIONS -msse4.1 -mavx -mavx2)
    set(COMPLEX_SIMD_OPTIMISATION -msse4.1 CACHE STRING "Choose SIMD optimisation" ${})
  elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(COMPLEX_SIMD_OPTIMISATION_OPTIONS /arch:SSE2 /arch:AVX2)
    set(COMPLEX_SIMD_OPTIMISATION /arch:SSE2 CACHE STRING "Choose SIMD optimisation")
  endif()

  set_property(CACHE COMPLEX_SIMD_OPTIMISATION PROPERTY STRINGS ${COMPLEX_SIMD_OPTIMISATION_OPTIONS})

else (CMAKE_SYSTEM_PROCESSOR MATCHES "(arm)|(arm64)")

endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_link_options(${TargetName} PUBLIC "-lc++")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(${TargetName} PRIVATE /fp:fast /Zc:preprocessor /permissive- /wd5105)
endif()

target_compile_options(${TargetName} PRIVATE ${COMPLEX_SIMD_OPTIMISATION})