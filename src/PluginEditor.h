#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class NaveAudioProcessor;

// A simple, functional v0.1 editor: one rotary slider per parameter, bound
// to the APVTS via SliderAttachment, plus a "Load IR..."/"Default" button
// pair for choosing/clearing the cabinet impulse response. A custom
// vector-drawn GUI is a later milestone; this is deliberately plain but
// fully wired and usable.
class NaveAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NaveAudioProcessorEditor (NaveAudioProcessor& processorToEdit);
    ~NaveAudioProcessorEditor() override;

    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    // One knob + label per parameter, in signal-flow order.
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void configureKnob (Knob& knob, const juce::String& parameterId, const juce::String& labelText);
    void updateIrLabel();
    void chooseImpulseResponseFile();

    NaveAudioProcessor& audioProcessor;

    Knob loCutKnob;
    Knob hiCutKnob;
    Knob mixKnob;
    Knob levelKnob;

    juce::Label irNameLabel;
    juce::TextButton loadIrButton { "Load IR..." };
    juce::TextButton defaultIrButton { "Default" };

    std::unique_ptr<juce::FileChooser> activeFileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NaveAudioProcessorEditor)
};
