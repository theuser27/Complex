#pragma once

#define CPLUG_COMPANY_NAME         "theuser27"
#define CPLUG_COMPANY_EMAIL        ""
#define CPLUG_PLUGIN_NAME          "Complex"
#define CPLUG_PLUGIN_URI           ""
#define CPLUG_PLUGIN_VERSION       "0.2.0"
																   
#define CPLUG_IS_INSTRUMENT        0
																   
#define CPLUG_NUM_INPUT_BUSSES     1
#define CPLUG_NUM_OUTPUT_BUSSES    1
#define CPLUG_WANT_MIDI_INPUT      0
#define CPLUG_WANT_MIDI_OUTPUT     0
																   
#define CPLUG_WANT_GUI             1
#define CPLUG_GUI_RESIZABLE        1

// See list of categories here: https://steinbergmedia.github.io/vst3_doc/vstinterfaces/group__plugType.html
#define CPLUG_VST3_CATEGORIES      "Fx|Stereo"

#define CPLUG_VST3_TUID_COMPONENT  'cplg', 'comp', 'xmpl', 0
#define CPLUG_VST3_TUID_CONTROLLER 'cplg', 'edit', 'xmpl', 0

#define CPLUG_CLAP_ID              "com.theuser27.complex"
#define CPLUG_CLAP_DESCRIPTION     "Example plugin"
#define CPLUG_CLAP_FEATURES        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_STEREO
