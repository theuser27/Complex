/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2022 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 7 End-User License
   Agreement and JUCE Privacy Policy.

   End User License Agreement: www.juce.com/juce-7-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#include <juce_core/system/juce_TargetPlatform.h>
#include "../utility/juce_CheckSettingMacros.h"

#include "../utility/juce_IncludeSystemHeaders.h"
#include "../utility/juce_IncludeModuleHeaders.h"
#include "../utility/juce_WindowsHooks.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>

// You can set this flag in your build if you need to specify a different
// standalone JUCEApplication class for your app to use. If you don't
// set it then by default we'll just create a simple one as below.
#if ! JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP

#include "juce_StandaloneFilterWindow.h"

namespace juce
{

template <typename Value>
struct ChannelInfo
{
    ChannelInfo() = default;
    ChannelInfo (Value* const* dataIn, int numChannelsIn)
        : data (dataIn), numChannels (numChannelsIn)  {}

    Value* const* data = nullptr;
    int numChannels = 0;
};

/** Sets up `channels` so that it contains channel pointers suitable for passing to
    an AudioProcessor's processBlock.

    On return, `channels` will hold `max (processorIns, processorOuts)` entries.
    The first `processorIns` entries will point to buffers holding input data.
    Any entries after the first `processorIns` entries will point to zeroed buffers.

    In the case that the system only provides a single input channel, but the processor
    has been initialised with multiple input channels, the system input will be copied
    to all processor inputs.

    In the case that the system provides no input channels, but the processor has
    been initialise with multiple input channels, the processor's input channels will
    all be zeroed.

    @param ins            the system inputs.
    @param outs           the system outputs.
    @param numSamples     the number of samples in the system buffers.
    @param processorIns   the number of input channels requested by the processor.
    @param processorOuts  the number of output channels requested by the processor.
    @param tempBuffer     temporary storage for inputs that don't have a corresponding output.
    @param channels       holds pointers to each of the processor's audio channels.
*/
static void initialiseIoBuffers (ChannelInfo<const float> ins,
                                 ChannelInfo<float> outs,
                                 const int numSamples,
                                 int processorIns,
                                 int processorOuts,
                                 AudioBuffer<float>& tempBuffer,
                                 std::vector<float*>& channels)
{
    jassert ((int) channels.size() >= jmax (processorIns, processorOuts));

    size_t totalNumChans = 0;
    const auto numBytes = (size_t) numSamples * sizeof (float);

    const auto prepareInputChannel = [&] (int index)
    {
        if (ins.numChannels == 0)
            zeromem (channels[totalNumChans], numBytes);
        else
            memcpy (channels[totalNumChans], ins.data[index % ins.numChannels], numBytes);
    };

    if (processorIns > processorOuts)
    {
        // If there aren't enough output channels for the number of
        // inputs, we need to use some temporary extra ones (can't
        // use the input data in case it gets written to).
        jassert (tempBuffer.getNumChannels() >= processorIns - processorOuts);
        jassert (tempBuffer.getNumSamples() >= numSamples);

        for (int i = 0; i < processorOuts; ++i)
        {
            channels[totalNumChans] = outs.data[i];
            prepareInputChannel (i);
            ++totalNumChans;
        }

        for (auto i = processorOuts; i < processorIns; ++i)
        {
            channels[totalNumChans] = tempBuffer.getWritePointer (i - processorOuts);
            prepareInputChannel (i);
            ++totalNumChans;
        }
    }
    else
    {
        for (int i = 0; i < processorIns; ++i)
        {
            channels[totalNumChans] = outs.data[i];
            prepareInputChannel (i);
            ++totalNumChans;
        }

        for (auto i = processorIns; i < processorOuts; ++i)
        {
            channels[totalNumChans] = outs.data[i];
            zeromem (channels[totalNumChans], (size_t) numSamples * sizeof (float));
            ++totalNumChans;
        }
    }
}

static void drawTickBox(Graphics &g, Component &component,
  float x, float y, float w, float h,
  const bool ticked,
  [[maybe_unused]] const bool isEnabled,
  [[maybe_unused]] const bool shouldDrawButtonAsHighlighted,
  [[maybe_unused]] const bool shouldDrawButtonAsDown)
{
  auto getTickShape = [](float height)
  {
    static const unsigned char pathData[] = { 110,109,32,210,202,64,126,183,148,64,108,39,244,247,64,245,76,124,64,108,178,131,27,65,246,76,252,64,108,175,242,4,65,246,76,252,
    64,108,236,5,68,65,0,0,160,180,108,240,150,90,65,21,136,52,63,108,48,59,16,65,0,0,32,65,108,32,210,202,64,126,183,148,64, 99,101,0,0 };

    Path path;
    path.loadPathFromData(pathData, sizeof(pathData));
    path.scaleToFit(0, 0, height * 2.0f, height, true);

    return path;
  };
  Rectangle<float> tickBounds(x, y, w, h);

  g.setColour(component.findColour(ToggleButton::tickDisabledColourId));
  g.drawRoundedRectangle(tickBounds, 4.0f, 1.0f);

  if (ticked)
  {
    g.setColour(component.findColour(ToggleButton::tickColourId));
    auto tick = getTickShape(0.75f);
    g.fillPath(tick, tick.getTransformToScaleToFit(tickBounds.reduced(4, 5).toFloat(), false));
  }
}

//==============================================================================
AudioProcessorPlayer::AudioProcessorPlayer (bool doDoublePrecisionProcessing)
    : isDoublePrecision (doDoublePrecisionProcessing)
{
}

AudioProcessorPlayer::~AudioProcessorPlayer()
{
    setProcessor (nullptr);
}

//==============================================================================
AudioProcessorPlayer::NumChannels AudioProcessorPlayer::findMostSuitableLayout (const AudioProcessor& proc) const
{
    if (proc.isMidiEffect())
        return {};

    std::vector<NumChannels> layouts { deviceChannels };

    if (deviceChannels.ins == 0 || deviceChannels.ins == 1)
    {
        layouts.emplace_back (defaultProcessorChannels.ins, deviceChannels.outs);
        layouts.emplace_back (deviceChannels.outs, deviceChannels.outs);
    }

    const auto it = std::find_if (layouts.begin(), layouts.end(), [&] (const NumChannels& chans)
    {
        return proc.checkBusesLayoutSupported (chans.toLayout());
    });

    return it != std::end (layouts) ? *it : layouts[0];
}

void AudioProcessorPlayer::resizeChannels()
{
    const auto maxChannels = jmax (deviceChannels.ins,
                                   deviceChannels.outs,
                                   actualProcessorChannels.ins,
                                   actualProcessorChannels.outs);
    channels.resize ((size_t) maxChannels);
    tempBuffer.setSize (maxChannels, blockSize);
}

void AudioProcessorPlayer::setProcessor (AudioProcessor* const processorToPlay)
{
    const ScopedLock sl (lock);

    if (processor == processorToPlay)
        return;

    sampleCount = 0;

    if (processorToPlay != nullptr && sampleRate > 0 && blockSize > 0)
    {
        defaultProcessorChannels = NumChannels { processorToPlay->getBusesLayout() };
        actualProcessorChannels  = findMostSuitableLayout (*processorToPlay);

        if (processorToPlay->isMidiEffect())
            processorToPlay->setRateAndBufferSizeDetails (sampleRate, blockSize);
        else
            processorToPlay->setPlayConfigDetails (actualProcessorChannels.ins,
                                                   actualProcessorChannels.outs,
                                                   sampleRate,
                                                   blockSize);

        auto supportsDouble = processorToPlay->supportsDoublePrecisionProcessing() && isDoublePrecision;

        processorToPlay->setProcessingPrecision (supportsDouble ? AudioProcessor::doublePrecision
                                                                : AudioProcessor::singlePrecision);
        processorToPlay->prepareToPlay (sampleRate, blockSize);
    }

    AudioProcessor* oldOne = nullptr;

    oldOne = isPrepared ? processor : nullptr;
    processor = processorToPlay;
    isPrepared = true;
    resizeChannels();

    if (oldOne != nullptr)
        oldOne->releaseResources();
}

void AudioProcessorPlayer::setDoublePrecisionProcessing (bool doublePrecision)
{
    if (doublePrecision != isDoublePrecision)
    {
        const ScopedLock sl (lock);

        if (processor != nullptr)
        {
            processor->releaseResources();

            auto supportsDouble = processor->supportsDoublePrecisionProcessing() && doublePrecision;

            processor->setProcessingPrecision (supportsDouble ? AudioProcessor::doublePrecision
                                                              : AudioProcessor::singlePrecision);
            processor->prepareToPlay (sampleRate, blockSize);
        }

        isDoublePrecision = doublePrecision;
    }
}

void AudioProcessorPlayer::setMidiOutput (MidiOutput* midiOutputToUse)
{
    if (midiOutput != midiOutputToUse)
    {
        const ScopedLock sl (lock);
        midiOutput = midiOutputToUse;
    }
}

//==============================================================================
void AudioProcessorPlayer::audioDeviceIOCallbackWithContext (const float* const* const inputChannelData,
                                                             const int numInputChannels,
                                                             float* const* const outputChannelData,
                                                             const int numOutputChannels,
                                                             const int numSamples,
                                                             const AudioIODeviceCallbackContext& context)
{
    const ScopedLock sl (lock);

    // These should have been prepared by audioDeviceAboutToStart()...
    jassert (sampleRate > 0 && blockSize > 0);

    incomingMidi.clear();
    messageCollector.removeNextBlockOfMessages (incomingMidi, numSamples);

    initialiseIoBuffers ({ inputChannelData,  numInputChannels },
                         { outputChannelData, numOutputChannels },
                         numSamples,
                         actualProcessorChannels.ins,
                         actualProcessorChannels.outs,
                         tempBuffer,
                         channels);

    const auto totalNumChannels = jmax (actualProcessorChannels.ins, actualProcessorChannels.outs);
    AudioBuffer<float> buffer (channels.data(), (int) totalNumChannels, numSamples);

    if (processor != nullptr)
    {
        // The processor should be prepared to deal with the same number of output channels
        // as our output device.
        jassert (processor->isMidiEffect() || numOutputChannels == actualProcessorChannels.outs);

        const ScopedLock sl2 (processor->getCallbackLock());

        class PlayHead : private AudioPlayHead
        {
        public:
            PlayHead (AudioProcessor& proc,
                      Optional<uint64_t> hostTimeIn,
                      uint64_t sampleCountIn,
                      double sampleRateIn)
                : processor (proc),
                  hostTimeNs (hostTimeIn),
                  sampleCount (sampleCountIn),
                  seconds ((double) sampleCountIn / sampleRateIn)
            {
                if (useThisPlayhead)
                    processor.setPlayHead (this);
            }

            ~PlayHead() override
            {
                if (useThisPlayhead)
                    processor.setPlayHead (nullptr);
            }

        private:
            Optional<PositionInfo> getPosition() const override
            {
                PositionInfo info;
                info.setHostTimeNs (hostTimeNs);
                info.setTimeInSamples ((int64_t) sampleCount);
                info.setTimeInSeconds (seconds);
                return info;
            }

            AudioProcessor& processor;
            Optional<uint64_t> hostTimeNs;
            uint64_t sampleCount;
            double seconds;
            bool useThisPlayhead = processor.getPlayHead() == nullptr;
        };

        PlayHead playHead { *processor,
                            context.hostTimeNs != nullptr ? makeOptional (*context.hostTimeNs) : nullopt,
                            sampleCount,
                            sampleRate };

        sampleCount += (uint64_t) numSamples;

        if (! processor->isSuspended())
        {
            if (processor->isUsingDoublePrecision())
            {
                conversionBuffer.makeCopyOf (buffer, true);
                processor->processBlock (conversionBuffer, incomingMidi);
                buffer.makeCopyOf (conversionBuffer, true);
            }
            else
            {
                processor->processBlock (buffer, incomingMidi);
            }

            if (midiOutput != nullptr)
            {
                if (midiOutput->isBackgroundThreadRunning())
                {
                    midiOutput->sendBlockOfMessages (incomingMidi,
                                                     Time::getMillisecondCounterHiRes(),
                                                     sampleRate);
                }
                else
                {
                    midiOutput->sendBlockOfMessagesNow (incomingMidi);
                }
            }

            return;
        }
    }

    for (int i = 0; i < numOutputChannels; ++i)
        FloatVectorOperations::clear (outputChannelData[i], numSamples);
}

void AudioProcessorPlayer::audioDeviceAboutToStart (AudioIODevice* const device)
{
    auto newSampleRate = device->getCurrentSampleRate();
    auto newBlockSize  = device->getCurrentBufferSizeSamples();
    auto numChansIn    = device->getActiveInputChannels().countNumberOfSetBits();
    auto numChansOut   = device->getActiveOutputChannels().countNumberOfSetBits();

    const ScopedLock sl (lock);

    sampleRate = newSampleRate;
    blockSize  = newBlockSize;
    deviceChannels = { numChansIn, numChansOut };

    resizeChannels();

    messageCollector.reset (sampleRate);

    if (processor != nullptr)
    {
        if (isPrepared)
            processor->releaseResources();

        auto* oldProcessor = processor;
        setProcessor (nullptr);
        setProcessor (oldProcessor);
    }
}

void AudioProcessorPlayer::audioDeviceStopped()
{
    const ScopedLock sl (lock);

    if (processor != nullptr && isPrepared)
        processor->releaseResources();

    sampleRate = 0.0;
    blockSize = 0;
    isPrepared = false;
    tempBuffer.setSize (1, 1);
}

void AudioProcessorPlayer::handleIncomingMidiMessage (MidiInput*, const MidiMessage& message)
{
    messageCollector.addMessageToQueue (message);
}

struct SimpleDeviceManagerInputLevelMeter  : public Component,
                                             public Timer
{
    SimpleDeviceManagerInputLevelMeter (AudioDeviceManager& m)  : manager (m)
    {
        startTimerHz (20);
        inputLevelGetter = manager.getInputLevelGetter();
    }

    void timerCallback() override
    {
        if (isShowing())
        {
            auto newLevel = (float) inputLevelGetter->getCurrentLevel();

            if (std::abs (level - newLevel) > 0.005f)
            {
                level = newLevel;
                repaint();
            }
        }
        else
        {
            level = 0;
        }
    }

    void paint (Graphics& g) override
    {
      [&](int width, int height, float level)
      {
        auto outerCornerSize = 3.0f;
        auto outerBorderWidth = 2.0f;
        auto totalBlocks = 7;
        auto spacingFraction = 0.03f;

        g.setColour(findColour(ResizableWindow::backgroundColourId));
        g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height), outerCornerSize);

        auto doubleOuterBorderWidth = 2.0f * outerBorderWidth;
        auto numBlocks = roundToInt((float)totalBlocks * level);

        auto blockWidth = ((float)width - doubleOuterBorderWidth) / static_cast<float> (totalBlocks);
        auto blockHeight = (float)height - doubleOuterBorderWidth;

        auto blockRectWidth = (1.0f - 2.0f * spacingFraction) * blockWidth;
        auto blockRectSpacing = spacingFraction * blockWidth;

        auto blockCornerSize = 0.1f * blockWidth;

        auto c = Colour{ 0xffddddff };

        for (auto i = 0; i < totalBlocks; ++i)
        {
          if (i >= numBlocks)
            g.setColour(c.withAlpha(0.5f));
          else
            g.setColour(i < totalBlocks - 1 ? c : Colours::red);

          g.fillRoundedRectangle(outerBorderWidth + ((float)i * blockWidth) + blockRectSpacing,
            outerBorderWidth,
            blockRectWidth,
            blockHeight,
            blockCornerSize);
        }
      }(getWidth(), getHeight(), (float)std::exp(std::log(level) / 3.0));
    }

    AudioDeviceManager& manager;
    AudioDeviceManager::LevelMeter::Ptr inputLevelGetter;
    float level = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleDeviceManagerInputLevelMeter)
};

static void drawTextLayout (Graphics& g, Component& owner, StringRef text, const Rectangle<int>& textBounds, bool enabled)
{
    const auto textColour = owner.findColour (ListBox::textColourId, true).withMultipliedAlpha (enabled ? 1.0f : 0.6f);

    AttributedString attributedString { text };
    attributedString.setColour (textColour);
    attributedString.setFont ((float) textBounds.getHeight() * 0.6f);
    attributedString.setJustification (Justification::centredLeft);
    attributedString.setWordWrap (AttributedString::WordWrap::none);

    TextLayout textLayout;
    textLayout.createLayout (attributedString,
                             (float) textBounds.getWidth(),
                             (float) textBounds.getHeight());
    textLayout.draw (g, textBounds.toFloat());
}


//==============================================================================
class AudioDeviceSelectorComponent::MidiInputSelectorComponentListBox  : public ListBox,
                                                                         private ListBoxModel
{
public:
    MidiInputSelectorComponentListBox (AudioDeviceManager& dm, const String& noItems)
        : ListBox ({}, nullptr),
          deviceManager (dm),
          noItemsMessage (noItems)
    {
        updateDevices();
        setModel (this);
        setOutlineThickness (1);
    }

    void updateDevices()
    {
        items = MidiInput::getAvailableDevices();
    }

    int getNumRows() override
    {
        return items.size();
    }

    void paintListBoxItem (int row, Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (isPositiveAndBelow (row, items.size()))
        {
            if (rowIsSelected)
              g.fillAll(Colour{ 0xff42a2c8 }.withAlpha(0.4f).withMultipliedAlpha (0.3f));

            auto item = items[row];
            bool enabled = deviceManager.isMidiInputDeviceEnabled (item.identifier);

            auto x = getTickX();
            auto tickW = (float) height * 0.75f;

            drawTickBox (g, *this, (float) x - tickW, ((float) height - tickW) * 0.5f, tickW, tickW,
                                          enabled, true, true, false);

            drawTextLayout (g, *this, item.name, { x + 5, 0, width - x - 5, height }, enabled);
        }
    }

    void listBoxItemClicked (int row, const MouseEvent& e) override
    {
        selectRow (row);

        if (e.x < getTickX())
            flipEnablement (row);
    }

    void listBoxItemDoubleClicked (int row, const MouseEvent&) override
    {
        flipEnablement (row);
    }

    void returnKeyPressed (int row) override
    {
        flipEnablement (row);
    }

    void paint (Graphics& g) override
    {
        ListBox::paint (g);

        if (items.isEmpty())
        {
            g.setColour (Colours::grey);
            g.setFont (0.5f * (float) getRowHeight());
            g.drawText (noItemsMessage,
                        0, 0, getWidth(), getHeight() / 2,
                        Justification::centred, true);
        }
    }

    int getBestHeight (int preferredHeight)
    {
        auto extra = getOutlineThickness() * 2;

        return jmax (getRowHeight() * 2 + extra,
                     jmin (getRowHeight() * getNumRows() + extra,
                           preferredHeight));
    }

private:
    //==============================================================================
    AudioDeviceManager& deviceManager;
    const String noItemsMessage;
    Array<MidiDeviceInfo> items;

    void flipEnablement (const int row)
    {
        if (isPositiveAndBelow (row, items.size()))
        {
            auto identifier = items[row].identifier;
            deviceManager.setMidiInputDeviceEnabled (identifier, ! deviceManager.isMidiInputDeviceEnabled (identifier));
        }
    }

    int getTickX() const
    {
        return getRowHeight();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiInputSelectorComponentListBox)
};


//==============================================================================
struct AudioDeviceSetupDetails
{
    AudioDeviceManager* manager;
    int minNumInputChannels, maxNumInputChannels;
    int minNumOutputChannels, maxNumOutputChannels;
    bool useStereoPairs;
};

static String getNoDeviceString()   { return "<< " + TRANS("none") + " >>"; }

//==============================================================================
class AudioDeviceSettingsPanel : public Component,
                                 private ChangeListener
{
public:
    AudioDeviceSettingsPanel (AudioIODeviceType& t, AudioDeviceSetupDetails& setupDetails,
                              const bool hideAdvancedOptionsWithButton)
        : type (t), setup (setupDetails)
    {
        if (hideAdvancedOptionsWithButton)
        {
            showAdvancedSettingsButton.reset (new TextButton (TRANS("Show advanced settings...")));
            addAndMakeVisible (showAdvancedSettingsButton.get());
            showAdvancedSettingsButton->setClickingTogglesState (true);
            showAdvancedSettingsButton->onClick = [this] { toggleAdvancedSettings(); };
        }

        type.scanForDevices();

        setup.manager->addChangeListener (this);
    }

    ~AudioDeviceSettingsPanel() override
    {
        setup.manager->removeChangeListener (this);
    }

    void resized() override
    {
        if (auto* parent = findParentComponentOfClass<AudioDeviceSelectorComponent>())
        {
            Rectangle<int> r (proportionOfWidth (0.35f), 0, proportionOfWidth (0.6f), 3000);

            const int maxListBoxHeight = 100;
            const int h = parent->getItemHeight();
            const int space = h / 4;

            if (outputDeviceDropDown != nullptr)
            {
                auto row = r.removeFromTop (h);

                if (testButton != nullptr)
                {
                    testButton->changeWidthToFitText (h);
                    testButton->setBounds (row.removeFromRight (testButton->getWidth()));
                    row.removeFromRight (space);
                }

                outputDeviceDropDown->setBounds (row);
                r.removeFromTop (space);
            }

            if (inputDeviceDropDown != nullptr)
            {
                auto row = r.removeFromTop (h);

                inputLevelMeter->setBounds (row.removeFromRight (testButton != nullptr ? testButton->getWidth() : row.getWidth() / 6));
                row.removeFromRight (space);
                inputDeviceDropDown->setBounds (row);
                r.removeFromTop (space);
            }

            if (outputChanList != nullptr)
            {
                outputChanList->setRowHeight (jmin (22, h));
                outputChanList->setBounds (r.removeFromTop (outputChanList->getBestHeight (maxListBoxHeight)));
                outputChanLabel->setBounds (0, outputChanList->getBounds().getCentreY() - h / 2, r.getX(), h);
                r.removeFromTop (space);
            }

            if (inputChanList != nullptr)
            {
                inputChanList->setRowHeight (jmin (22, h));
                inputChanList->setBounds (r.removeFromTop (inputChanList->getBestHeight (maxListBoxHeight)));
                inputChanLabel->setBounds (0, inputChanList->getBounds().getCentreY() - h / 2, r.getX(), h);
                r.removeFromTop (space);
            }

            r.removeFromTop (space * 2);

            if (showAdvancedSettingsButton != nullptr
                && sampleRateDropDown != nullptr && bufferSizeDropDown != nullptr)
            {
                showAdvancedSettingsButton->setBounds (r.removeFromTop (h));
                r.removeFromTop (space);
                showAdvancedSettingsButton->changeWidthToFitText();
            }

            auto advancedSettingsVisible = showAdvancedSettingsButton == nullptr
                                              || showAdvancedSettingsButton->getToggleState();

            if (sampleRateDropDown != nullptr)
            {
                sampleRateDropDown->setVisible (advancedSettingsVisible);

                if (advancedSettingsVisible)
                {
                    sampleRateDropDown->setBounds (r.removeFromTop (h));
                    r.removeFromTop (space);
                }
            }

            if (bufferSizeDropDown != nullptr)
            {
                bufferSizeDropDown->setVisible (advancedSettingsVisible);

                if (advancedSettingsVisible)
                {
                    bufferSizeDropDown->setBounds (r.removeFromTop (h));
                    r.removeFromTop (space);
                }
            }

            r.removeFromTop (space);

            if (showUIButton != nullptr || resetDeviceButton != nullptr)
            {
                auto buttons = r.removeFromTop (h);

                if (showUIButton != nullptr)
                {
                    showUIButton->setVisible (advancedSettingsVisible);
                    showUIButton->changeWidthToFitText (h);
                    showUIButton->setBounds (buttons.removeFromLeft (showUIButton->getWidth()));
                    buttons.removeFromLeft (space);
                }

                if (resetDeviceButton != nullptr)
                {
                    resetDeviceButton->setVisible (advancedSettingsVisible);
                    resetDeviceButton->changeWidthToFitText (h);
                    resetDeviceButton->setBounds (buttons.removeFromLeft (resetDeviceButton->getWidth()));
                }

                r.removeFromTop (space);
            }

            setSize (getWidth(), r.getY());
        }
        else
        {
            jassertfalse;
        }
    }

    void updateConfig (bool updateOutputDevice, bool updateInputDevice, bool updateSampleRate, bool updateBufferSize)
    {
        auto config = setup.manager->getAudioDeviceSetup();
        String error;

        if (updateOutputDevice || updateInputDevice)
        {
            if (outputDeviceDropDown != nullptr)
                config.outputDeviceName = outputDeviceDropDown->getSelectedId() < 0 ? String()
                                                                                    : outputDeviceDropDown->getText();

            if (inputDeviceDropDown != nullptr)
                config.inputDeviceName = inputDeviceDropDown->getSelectedId() < 0 ? String()
                                                                                  : inputDeviceDropDown->getText();

            if (! type.hasSeparateInputsAndOutputs())
                config.inputDeviceName = config.outputDeviceName;

            if (updateInputDevice)
                config.useDefaultInputChannels = true;
            else
                config.useDefaultOutputChannels = true;

            error = setup.manager->setAudioDeviceSetup (config, true);

            showCorrectDeviceName (inputDeviceDropDown.get(), true);
            showCorrectDeviceName (outputDeviceDropDown.get(), false);

            updateControlPanelButton();
            resized();
        }
        else if (updateSampleRate)
        {
            if (sampleRateDropDown->getSelectedId() > 0)
            {
                config.sampleRate = sampleRateDropDown->getSelectedId();
                error = setup.manager->setAudioDeviceSetup (config, true);
            }
        }
        else if (updateBufferSize)
        {
            if (bufferSizeDropDown->getSelectedId() > 0)
            {
                config.bufferSize = bufferSizeDropDown->getSelectedId();
                error = setup.manager->setAudioDeviceSetup (config, true);
            }
        }

        if (error.isNotEmpty())
            NativeMessageBox::showMessageBoxAsync (MessageBoxIconType::WarningIcon,
                                              TRANS("Error when trying to open audio device!"),
                                              error);
    }

    bool showDeviceControlPanel()
    {
        if (auto* device = setup.manager->getCurrentAudioDevice())
        {
            Component modalWindow;
            modalWindow.setOpaque (true);
            modalWindow.addToDesktop (0);
            modalWindow.enterModalState();

            return device->showControlPanel();
        }

        return false;
    }

    void toggleAdvancedSettings()
    {
        showAdvancedSettingsButton->setButtonText ((showAdvancedSettingsButton->getToggleState() ? "Hide " : "Show ")
                                                   + String ("advanced settings..."));
        resized();
    }

    void showDeviceUIPanel()
    {
        if (showDeviceControlPanel())
        {
            setup.manager->closeAudioDevice();
            setup.manager->restartLastAudioDevice();
            getTopLevelComponent()->toFront (true);
        }
    }

    void playTestSound()
    {
        setup.manager->playTestSound();
    }

    void updateAllControls()
    {
        updateOutputsComboBox();
        updateInputsComboBox();

        updateControlPanelButton();
        updateResetButton();

        if (auto* currentDevice = setup.manager->getCurrentAudioDevice())
        {
            if (setup.maxNumOutputChannels > 0
                 && setup.minNumOutputChannels < setup.manager->getCurrentAudioDevice()->getOutputChannelNames().size())
            {
                if (outputChanList == nullptr)
                {
                    outputChanList.reset (new ChannelSelectorListBox (setup, ChannelSelectorListBox::audioOutputType,
                                                                      TRANS ("(no audio output channels found)")));
                    addAndMakeVisible (outputChanList.get());
                    outputChanLabel.reset (new Label ({}, TRANS("Active output channels:")));
                    outputChanLabel->setJustificationType (Justification::centredRight);
                    outputChanLabel->attachToComponent (outputChanList.get(), true);
                }

                outputChanList->refresh();
            }
            else
            {
                outputChanLabel.reset();
                outputChanList.reset();
            }

            if (setup.maxNumInputChannels > 0
                 && setup.minNumInputChannels < setup.manager->getCurrentAudioDevice()->getInputChannelNames().size())
            {
                if (inputChanList == nullptr)
                {
                    inputChanList.reset (new ChannelSelectorListBox (setup, ChannelSelectorListBox::audioInputType,
                                                                     TRANS("(no audio input channels found)")));
                    addAndMakeVisible (inputChanList.get());
                    inputChanLabel.reset (new Label ({}, TRANS("Active input channels:")));
                    inputChanLabel->setJustificationType (Justification::centredRight);
                    inputChanLabel->attachToComponent (inputChanList.get(), true);
                }

                inputChanList->refresh();
            }
            else
            {
                inputChanLabel.reset();
                inputChanList.reset();
            }

            updateSampleRateComboBox (currentDevice);
            updateBufferSizeComboBox (currentDevice);
        }
        else
        {
            jassert (setup.manager->getCurrentAudioDevice() == nullptr); // not the correct device type!

            inputChanLabel.reset();
            outputChanLabel.reset();
            sampleRateLabel.reset();
            bufferSizeLabel.reset();

            inputChanList.reset();
            outputChanList.reset();
            sampleRateDropDown.reset();
            bufferSizeDropDown.reset();

            if (outputDeviceDropDown != nullptr)
                outputDeviceDropDown->setSelectedId (-1, dontSendNotification);

            if (inputDeviceDropDown != nullptr)
                inputDeviceDropDown->setSelectedId (-1, dontSendNotification);
        }

        sendLookAndFeelChange();
        resized();
        setSize (getWidth(), getLowestY() + 4);
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updateAllControls();
    }

    void resetDevice()
    {
        setup.manager->closeAudioDevice();
        setup.manager->restartLastAudioDevice();
    }

private:
    AudioIODeviceType& type;
    const AudioDeviceSetupDetails setup;

    std::unique_ptr<ComboBox> outputDeviceDropDown, inputDeviceDropDown, sampleRateDropDown, bufferSizeDropDown;
    std::unique_ptr<Label> outputDeviceLabel, inputDeviceLabel, sampleRateLabel, bufferSizeLabel, inputChanLabel, outputChanLabel;
    std::unique_ptr<TextButton> testButton;
    std::unique_ptr<Component> inputLevelMeter;
    std::unique_ptr<TextButton> showUIButton, showAdvancedSettingsButton, resetDeviceButton;

    void showCorrectDeviceName (ComboBox* box, bool isInput)
    {
        if (box != nullptr)
        {
            auto* currentDevice = setup.manager->getCurrentAudioDevice();
            auto index = type.getIndexOfDevice (currentDevice, isInput);

            box->setSelectedId (index < 0 ? index : index + 1, dontSendNotification);

            if (testButton != nullptr && ! isInput)
                testButton->setEnabled (index >= 0);
        }
    }

    void addNamesToDeviceBox (ComboBox& combo, bool isInputs)
    {
        const StringArray devs (type.getDeviceNames (isInputs));

        combo.clear (dontSendNotification);

        for (int i = 0; i < devs.size(); ++i)
            combo.addItem (devs[i], i + 1);

        combo.addItem (getNoDeviceString(), -1);
        combo.setSelectedId (-1, dontSendNotification);
    }

    int getLowestY() const
    {
        int y = 0;

        for (auto* c : getChildren())
            y = jmax (y, c->getBottom());

        return y;
    }

    void updateControlPanelButton()
    {
        auto* currentDevice = setup.manager->getCurrentAudioDevice();
        showUIButton.reset();

        if (currentDevice != nullptr && currentDevice->hasControlPanel())
        {
            showUIButton.reset (new TextButton (TRANS ("Control Panel"),
                                                TRANS ("Opens the device's own control panel")));
            addAndMakeVisible (showUIButton.get());
            showUIButton->onClick = [this] { showDeviceUIPanel(); };
        }

        resized();
    }

    void updateResetButton()
    {
        if (auto* currentDevice = setup.manager->getCurrentAudioDevice())
        {
            if (currentDevice->hasControlPanel())
            {
                if (resetDeviceButton == nullptr)
                {
                    resetDeviceButton.reset (new TextButton (TRANS ("Reset Device"),
                                                             TRANS ("Resets the audio interface - sometimes needed after changing a device's properties in its custom control panel")));
                    addAndMakeVisible (resetDeviceButton.get());
                    resetDeviceButton->onClick = [this] { resetDevice(); };
                    resized();
                }

                return;
            }
        }

        resetDeviceButton.reset();
    }

    void updateOutputsComboBox()
    {
        if (setup.maxNumOutputChannels > 0 || ! type.hasSeparateInputsAndOutputs())
        {
            if (outputDeviceDropDown == nullptr)
            {
                outputDeviceDropDown.reset (new ComboBox());
                outputDeviceDropDown->onChange = [this] { updateConfig (true, false, false, false); };

                addAndMakeVisible (outputDeviceDropDown.get());

                outputDeviceLabel.reset (new Label ({}, type.hasSeparateInputsAndOutputs() ? TRANS("Output:")
                                                                                           : TRANS("Device:")));
                outputDeviceLabel->attachToComponent (outputDeviceDropDown.get(), true);

                if (setup.maxNumOutputChannels > 0)
                {
                    testButton.reset (new TextButton (TRANS("Test"), TRANS("Plays a test tone")));
                    addAndMakeVisible (testButton.get());
                    testButton->onClick = [this] { playTestSound(); };
                }
            }

            addNamesToDeviceBox (*outputDeviceDropDown, false);
        }

        showCorrectDeviceName (outputDeviceDropDown.get(), false);
    }

    void updateInputsComboBox()
    {
        if (setup.maxNumInputChannels > 0 && type.hasSeparateInputsAndOutputs())
        {
            if (inputDeviceDropDown == nullptr)
            {
                inputDeviceDropDown.reset (new ComboBox());
                inputDeviceDropDown->onChange = [this] { updateConfig (false, true, false, false); };
                addAndMakeVisible (inputDeviceDropDown.get());

                inputDeviceLabel.reset (new Label ({}, TRANS("Input:")));
                inputDeviceLabel->attachToComponent (inputDeviceDropDown.get(), true);

                inputLevelMeter.reset (new SimpleDeviceManagerInputLevelMeter (*setup.manager));
                addAndMakeVisible (inputLevelMeter.get());
            }

            addNamesToDeviceBox (*inputDeviceDropDown, true);
        }

        showCorrectDeviceName (inputDeviceDropDown.get(), true);
    }

    void updateSampleRateComboBox (AudioIODevice* currentDevice)
    {
        if (sampleRateDropDown == nullptr)
        {
            sampleRateDropDown.reset (new ComboBox());
            addAndMakeVisible (sampleRateDropDown.get());

            sampleRateLabel.reset (new Label ({}, TRANS("Sample rate:")));
            sampleRateLabel->attachToComponent (sampleRateDropDown.get(), true);
        }
        else
        {
            sampleRateDropDown->clear();
            sampleRateDropDown->onChange = nullptr;
        }

        const auto getFrequencyString = [] (int rate) { return String (rate) + " Hz"; };

        for (auto rate : currentDevice->getAvailableSampleRates())
        {
            const auto intRate = roundToInt (rate);
            sampleRateDropDown->addItem (getFrequencyString (intRate), intRate);
        }

        const auto intRate = roundToInt (currentDevice->getCurrentSampleRate());
        sampleRateDropDown->setText (getFrequencyString (intRate), dontSendNotification);

        sampleRateDropDown->onChange = [this] { updateConfig (false, false, true, false); };
    }

    void updateBufferSizeComboBox (AudioIODevice* currentDevice)
    {
        if (bufferSizeDropDown == nullptr)
        {
            bufferSizeDropDown.reset (new ComboBox());
            addAndMakeVisible (bufferSizeDropDown.get());

            bufferSizeLabel.reset (new Label ({}, TRANS("Audio buffer size:")));
            bufferSizeLabel->attachToComponent (bufferSizeDropDown.get(), true);
        }
        else
        {
            bufferSizeDropDown->clear();
            bufferSizeDropDown->onChange = nullptr;
        }

        auto currentRate = currentDevice->getCurrentSampleRate();

        if (currentRate == 0)
            currentRate = 48000.0;

        for (auto bs : currentDevice->getAvailableBufferSizes())
            bufferSizeDropDown->addItem (String (bs) + " samples (" + String (bs * 1000.0 / currentRate, 1) + " ms)", bs);

        bufferSizeDropDown->setSelectedId (currentDevice->getCurrentBufferSizeSamples(), dontSendNotification);
        bufferSizeDropDown->onChange = [this] { updateConfig (false, false, false, true); };
    }

public:
    //==============================================================================
    class ChannelSelectorListBox  : public ListBox,
                                    private ListBoxModel
    {
    public:
        enum BoxType
        {
            audioInputType,
            audioOutputType
        };

        //==============================================================================
        ChannelSelectorListBox (const AudioDeviceSetupDetails& setupDetails, BoxType boxType, const String& noItemsText)
           : ListBox ({}, nullptr), setup (setupDetails), type (boxType), noItemsMessage (noItemsText)
        {
            refresh();
            setModel (this);
            setOutlineThickness (1);
        }

        void refresh()
        {
            items.clear();

            if (auto* currentDevice = setup.manager->getCurrentAudioDevice())
            {
                if (type == audioInputType)
                    items = currentDevice->getInputChannelNames();
                else if (type == audioOutputType)
                    items = currentDevice->getOutputChannelNames();

                if (setup.useStereoPairs)
                {
                    StringArray pairs;

                    for (int i = 0; i < items.size(); i += 2)
                    {
                        auto& name = items[i];

                        if (i + 1 >= items.size())
                            pairs.add (name.trim());
                        else
                            pairs.add (getNameForChannelPair (name, items[i + 1]));
                    }

                    items = pairs;
                }
            }

            updateContent();
            repaint();
        }

        int getNumRows() override
        {
            return items.size();
        }

        void paintListBoxItem (int row, Graphics& g, int width, int height, bool) override
        {
            if (isPositiveAndBelow (row, items.size()))
            {
                g.fillAll (findColour (ListBox::backgroundColourId));

                auto item = items[row];
                bool enabled = false;
                auto config = setup.manager->getAudioDeviceSetup();

                if (setup.useStereoPairs)
                {
                    if (type == audioInputType)
                        enabled = config.inputChannels[row * 2] || config.inputChannels[row * 2 + 1];
                    else if (type == audioOutputType)
                        enabled = config.outputChannels[row * 2] || config.outputChannels[row * 2 + 1];
                }
                else
                {
                    if (type == audioInputType)
                        enabled = config.inputChannels[row];
                    else if (type == audioOutputType)
                        enabled = config.outputChannels[row];
                }

                auto x = getTickX();
                auto tickW = (float) height * 0.75f;

                drawTickBox (g, *this, (float) x - tickW, ((float) height - tickW) * 0.5f, tickW, tickW,
                                              enabled, true, true, false);

                drawTextLayout (g, *this, item, { x + 5, 0, width - x - 5, height }, enabled);
            }
        }

        void listBoxItemClicked (int row, const MouseEvent& e) override
        {
            selectRow (row);

            if (e.x < getTickX())
                flipEnablement (row);
        }

        void listBoxItemDoubleClicked (int row, const MouseEvent&) override
        {
            flipEnablement (row);
        }

        void returnKeyPressed (int row) override
        {
            flipEnablement (row);
        }

        void paint (Graphics& g) override
        {
            ListBox::paint (g);

            if (items.isEmpty())
            {
                g.setColour (Colours::grey);
                g.setFont (0.5f * (float) getRowHeight());
                g.drawText (noItemsMessage,
                            0, 0, getWidth(), getHeight() / 2,
                            Justification::centred, true);
            }
        }

        int getBestHeight (int maxHeight)
        {
            return getRowHeight() * jlimit (2, jmax (2, maxHeight / getRowHeight()),
                                            getNumRows())
                       + getOutlineThickness() * 2;
        }

    private:
        //==============================================================================
        const AudioDeviceSetupDetails setup;
        const BoxType type;
        const String noItemsMessage;
        StringArray items;

        static String getNameForChannelPair (const String& name1, const String& name2)
        {
            String commonBit;

            for (int j = 0; j < name1.length(); ++j)
                if (name1.substring (0, j).equalsIgnoreCase (name2.substring (0, j)))
                    commonBit = name1.substring (0, j);

            // Make sure we only split the name at a space, because otherwise, things
            // like "input 11" + "input 12" would become "input 11 + 2"
            while (commonBit.isNotEmpty() && ! CharacterFunctions::isWhitespace (commonBit.getLastCharacter()))
                commonBit = commonBit.dropLastCharacters (1);

            return name1.trim() + " + " + name2.substring (commonBit.length()).trim();
        }

        void flipEnablement (int row)
        {
            jassert (type == audioInputType || type == audioOutputType);

            if (isPositiveAndBelow (row, items.size()))
            {
                auto config = setup.manager->getAudioDeviceSetup();

                if (setup.useStereoPairs)
                {
                    BigInteger bits;
                    auto& original = (type == audioInputType ? config.inputChannels
                                                             : config.outputChannels);

                    for (int i = 0; i < 256; i += 2)
                        bits.setBit (i / 2, original[i] || original[i + 1]);

                    if (type == audioInputType)
                    {
                        config.useDefaultInputChannels = false;
                        flipBit (bits, row, setup.minNumInputChannels / 2, setup.maxNumInputChannels / 2);
                    }
                    else
                    {
                        config.useDefaultOutputChannels = false;
                        flipBit (bits, row, setup.minNumOutputChannels / 2, setup.maxNumOutputChannels / 2);
                    }

                    for (int i = 0; i < 256; ++i)
                        original.setBit (i, bits[i / 2]);
                }
                else
                {
                    if (type == audioInputType)
                    {
                        config.useDefaultInputChannels = false;
                        flipBit (config.inputChannels, row, setup.minNumInputChannels, setup.maxNumInputChannels);
                    }
                    else
                    {
                        config.useDefaultOutputChannels = false;
                        flipBit (config.outputChannels, row, setup.minNumOutputChannels, setup.maxNumOutputChannels);
                    }
                }

                setup.manager->setAudioDeviceSetup (config, true);
            }
        }

        static void flipBit (BigInteger& chans, int index, int minNumber, int maxNumber)
        {
            auto numActive = chans.countNumberOfSetBits();

            if (chans[index])
            {
                if (numActive > minNumber)
                    chans.setBit (index, false);
            }
            else
            {
                if (numActive >= maxNumber)
                {
                    auto firstActiveChan = chans.findNextSetBit (0);
                    chans.clearBit (index > firstActiveChan ? firstActiveChan : chans.getHighestBit());
                }

                chans.setBit (index, true);
            }
        }

        int getTickX() const
        {
            return getRowHeight();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelSelectorListBox)
    };

private:
    std::unique_ptr<ChannelSelectorListBox> inputChanList, outputChanList;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioDeviceSettingsPanel)
};


//==============================================================================
AudioDeviceSelectorComponent::AudioDeviceSelectorComponent (AudioDeviceManager& dm,
                                                            int minInputChannelsToUse,
                                                            int maxInputChannelsToUse,
                                                            int minOutputChannelsToUse,
                                                            int maxOutputChannelsToUse,
                                                            bool showMidiInputOptions,
                                                            bool showMidiOutputSelector,
                                                            bool showChannelsAsStereoPairsToUse,
                                                            bool hideAdvancedOptionsWithButtonToUse)
    : deviceManager (dm),
      itemHeight (24),
      minOutputChannels (minOutputChannelsToUse),
      maxOutputChannels (maxOutputChannelsToUse),
      minInputChannels (minInputChannelsToUse),
      maxInputChannels (maxInputChannelsToUse),
      showChannelsAsStereoPairs (showChannelsAsStereoPairsToUse),
      hideAdvancedOptionsWithButton (hideAdvancedOptionsWithButtonToUse)
{
    jassert (minOutputChannels >= 0 && minOutputChannels <= maxOutputChannels);
    jassert (minInputChannels >= 0 && minInputChannels <= maxInputChannels);

    const OwnedArray<AudioIODeviceType>& types = deviceManager.getAvailableDeviceTypes();

    if (types.size() > 1)
    {
        deviceTypeDropDown.reset (new ComboBox());

        for (int i = 0; i < types.size(); ++i)
            deviceTypeDropDown->addItem (types.getUnchecked(i)->getTypeName(), i + 1);

        addAndMakeVisible (deviceTypeDropDown.get());
        deviceTypeDropDown->onChange = [this] { updateDeviceType(); };

        deviceTypeDropDownLabel.reset (new Label ({}, TRANS("Audio device type:")));
        deviceTypeDropDownLabel->setJustificationType (Justification::centredRight);
        deviceTypeDropDownLabel->attachToComponent (deviceTypeDropDown.get(), true);
    }

    if (showMidiInputOptions)
    {
        midiInputsList.reset (new MidiInputSelectorComponentListBox (deviceManager,
                                                                     "(" + TRANS("No MIDI inputs available") + ")"));
        addAndMakeVisible (midiInputsList.get());

        midiInputsLabel.reset (new Label ({}, TRANS ("Active MIDI inputs:")));
        midiInputsLabel->setJustificationType (Justification::topRight);
        midiInputsLabel->attachToComponent (midiInputsList.get(), true);
    }
    else
    {
        midiInputsList.reset();
        midiInputsLabel.reset();
        bluetoothButton.reset();
    }

    if (showMidiOutputSelector)
    {
        midiOutputSelector.reset (new ComboBox());
        addAndMakeVisible (midiOutputSelector.get());
        midiOutputSelector->onChange = [this] { updateMidiOutput(); };

        midiOutputLabel.reset (new Label ("lm", TRANS("MIDI Output:")));
        midiOutputLabel->attachToComponent (midiOutputSelector.get(), true);
    }
    else
    {
        midiOutputSelector.reset();
        midiOutputLabel.reset();
    }

    deviceManager.addChangeListener (this);
    updateAllControls();
}

AudioDeviceSelectorComponent::~AudioDeviceSelectorComponent()
{
    deviceManager.removeChangeListener (this);
}

void AudioDeviceSelectorComponent::setItemHeight (int newItemHeight)
{
    itemHeight = newItemHeight;
    resized();
}

void AudioDeviceSelectorComponent::resized()
{
    Rectangle<int> r (proportionOfWidth (0.35f), 15, proportionOfWidth (0.6f), 3000);
    auto space = itemHeight / 4;

    if (deviceTypeDropDown != nullptr)
    {
        deviceTypeDropDown->setBounds (r.removeFromTop (itemHeight));
        r.removeFromTop (space * 3);
    }

    if (audioDeviceSettingsComp != nullptr)
    {
        audioDeviceSettingsComp->resized();
        audioDeviceSettingsComp->setBounds (r.removeFromTop (audioDeviceSettingsComp->getHeight())
                                                .withX (0).withWidth (getWidth()));
        r.removeFromTop (space);
    }

    if (midiInputsList != nullptr)
    {
        midiInputsList->setRowHeight (jmin (22, itemHeight));
        midiInputsList->setBounds (r.removeFromTop (midiInputsList->getBestHeight (jmin (itemHeight * 8,
                                                                                         getHeight() - r.getY() - space - itemHeight))));
        r.removeFromTop (space);
    }

    if (bluetoothButton != nullptr)
    {
        bluetoothButton->setBounds (r.removeFromTop (24));
        r.removeFromTop (space);
    }

    if (midiOutputSelector != nullptr)
        midiOutputSelector->setBounds (r.removeFromTop (itemHeight));

    r.removeFromTop (itemHeight);
    setSize (getWidth(), r.getY());
}

void AudioDeviceSelectorComponent::updateDeviceType()
{
    if (auto* type = deviceManager.getAvailableDeviceTypes() [deviceTypeDropDown->getSelectedId() - 1])
    {
        audioDeviceSettingsComp.reset();
        deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);
        updateAllControls(); // needed in case the type hasn't actually changed
    }
}

void AudioDeviceSelectorComponent::updateMidiOutput()
{
    auto selectedId = midiOutputSelector->getSelectedId();

    if (selectedId == -1)
        deviceManager.setDefaultMidiOutputDevice ({});
    else
        deviceManager.setDefaultMidiOutputDevice (currentMidiOutputs[selectedId - 1].identifier);
}

void AudioDeviceSelectorComponent::changeListenerCallback (ChangeBroadcaster*)
{
    updateAllControls();
}

void AudioDeviceSelectorComponent::updateAllControls()
{
    if (deviceTypeDropDown != nullptr)
        deviceTypeDropDown->setText (deviceManager.getCurrentAudioDeviceType(), dontSendNotification);

    if (audioDeviceSettingsComp == nullptr
         || audioDeviceSettingsCompType != deviceManager.getCurrentAudioDeviceType())
    {
        audioDeviceSettingsCompType = deviceManager.getCurrentAudioDeviceType();
        audioDeviceSettingsComp.reset();

        if (auto* type = deviceManager.getAvailableDeviceTypes() [deviceTypeDropDown == nullptr
                                                                   ? 0 : deviceTypeDropDown->getSelectedId() - 1])
        {
            AudioDeviceSetupDetails details;
            details.manager = &deviceManager;
            details.minNumInputChannels = minInputChannels;
            details.maxNumInputChannels = maxInputChannels;
            details.minNumOutputChannels = minOutputChannels;
            details.maxNumOutputChannels = maxOutputChannels;
            details.useStereoPairs = showChannelsAsStereoPairs;

            auto* sp = new AudioDeviceSettingsPanel (*type, details, hideAdvancedOptionsWithButton);
            audioDeviceSettingsComp.reset (sp);
            addAndMakeVisible (sp);
            sp->updateAllControls();
        }
    }

    if (midiInputsList != nullptr)
    {
        midiInputsList->updateDevices();
        midiInputsList->updateContent();
        midiInputsList->repaint();
    }

    if (midiOutputSelector != nullptr)
    {
        midiOutputSelector->clear();

        currentMidiOutputs = MidiOutput::getAvailableDevices();

        midiOutputSelector->addItem (getNoDeviceString(), -1);
        midiOutputSelector->addSeparator();

        auto defaultOutputIdentifier = deviceManager.getDefaultMidiOutputIdentifier();
        int i = 0;

        for (auto& out : currentMidiOutputs)
        {
            midiOutputSelector->addItem (out.name, i + 1);

            if (defaultOutputIdentifier.isNotEmpty() && out.identifier == defaultOutputIdentifier)
                midiOutputSelector->setSelectedId (i + 1);

            ++i;
        }
    }

    resized();
}

ListBox* AudioDeviceSelectorComponent::getMidiInputSelectorListBox() const noexcept
{
    return midiInputsList.get();
}

//==============================================================================
class StandaloneFilterApp  : public JUCEApplication
{
public:
    StandaloneFilterApp()
    {
        PropertiesFile::Options options;

        options.applicationName     = getApplicationName();
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const String getApplicationName() override              { return CharPointer_UTF8 (JucePlugin_Name); }
    const String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override              { return true; }
    void anotherInstanceStarted (const String&) override    {}

    virtual StandaloneFilterWindow* createWindow()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        StandalonePluginHolder::PluginInOuts channels[] = { JucePlugin_PreferredChannelConfigurations };
       #endif

        return new StandaloneFilterWindow (getApplicationName(),
                                           LookAndFeel::getDefaultLookAndFeel().findColour (ResizableWindow::backgroundColourId),
                                           appProperties.getUserSettings(),
                                           false, {}, nullptr
                                          #ifdef JucePlugin_PreferredChannelConfigurations
                                           , juce::Array<StandalonePluginHolder::PluginInOuts> (channels, juce::numElementsInArray (channels))
                                          #else
                                           , {}
                                          #endif
                                          #if JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
                                           , false
                                          #endif
                                           );
    }

    //==============================================================================
    void initialise (const String&) override
    {
        mainWindow.reset (createWindow());

       #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
        Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
       #endif

        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        if (mainWindow.get() != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            Timer::callAfterDelay (100, []()
            {
                if (auto app = JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

protected:
    ApplicationProperties appProperties;
    std::unique_ptr<StandaloneFilterWindow> mainWindow;
};

} // namespace juce

#if JucePlugin_Build_Standalone && JUCE_IOS

JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmissing-prototypes")

using namespace juce;

bool JUCE_CALLTYPE juce_isInterAppAudioConnected()
{
    if (auto holder = StandalonePluginHolder::getInstance())
        return holder->isInterAppAudioConnected();

    return false;
}

void JUCE_CALLTYPE juce_switchToHostApplication()
{
    if (auto holder = StandalonePluginHolder::getInstance())
        holder->switchToHostApplication();
}

Image JUCE_CALLTYPE juce_getIAAHostIcon (int size)
{
    if (auto holder = StandalonePluginHolder::getInstance())
        return holder->getIAAHostIcon (size);

    return Image();
}

JUCE_END_IGNORE_WARNINGS_GCC_LIKE

#endif

#endif
