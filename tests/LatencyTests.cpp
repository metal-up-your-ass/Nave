#include "PluginProcessor.h"
#include "dsp/CabConvolutionEngine.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("getLatencySamples() is zero after prepareToPlay with the default delta IR", "[latency]")
{
    NaveAudioProcessor processor;

    // Before prepareToPlay, no engine has been prepared yet - JUCE's default
    // AudioProcessor latency is 0.
    CHECK (processor.getLatencySamples() == 0);

    processor.prepareToPlay (48000.0, 512);

    // Cross-check against a standalone engine prepared identically: the
    // processor must report exactly what the engine (i.e. the convolution)
    // computes, not an approximation of it.
    CabConvolutionEngine referenceEngine;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    referenceEngine.prepare (spec);

    CHECK (processor.getLatencySamples() == referenceEngine.getLatencySamples());

    // The default zero-latency Convolution configuration with a 1-sample
    // delta IR always reports zero latency.
    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Latency is stable across repeated prepareToPlay calls at the same sample rate", "[latency]")
{
    NaveAudioProcessor processor;

    processor.prepareToPlay (44100.0, 256);
    const auto firstLatency = processor.getLatencySamples();

    processor.prepareToPlay (44100.0, 256);
    const auto secondLatency = processor.getLatencySamples();

    CHECK (firstLatency == secondLatency);
    CHECK (firstLatency == 0);
}

TEST_CASE ("Latency remains zero when the sample rate changes", "[latency]")
{
    NaveAudioProcessor processor;

    processor.prepareToPlay (44100.0, 512);
    const auto latencyAt44k = processor.getLatencySamples();

    processor.prepareToPlay (96000.0, 512);
    const auto latencyAt96k = processor.getLatencySamples();

    CHECK (latencyAt44k == 0);
    CHECK (latencyAt96k == 0);
}

TEST_CASE ("Latency remains zero after loading a custom (short) IR", "[latency]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> ir (1, 16);
    for (int i = 0; i < ir.getNumSamples(); ++i)
        ir.setSample (0, i, i == 0 ? 1.0f : 0.0f);

    CabConvolutionEngine engine;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    engine.prepare (spec);
    engine.setImpulseResponse (std::move (ir), 48000.0);

    // Re-prepare after loading, mirroring how the processor's next
    // prepareToPlay() call would pick up a newly-loaded IR's latency.
    engine.prepare (spec);

    CHECK (engine.getLatencySamples() == 0);
}
