#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "dsp/CabConvolutionEngine.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <random>

namespace
{
    void setParam (NaveAudioProcessor& processor, const char* id, float realValue)
    {
        auto* param = processor.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 (realValue));
    }
}

TEST_CASE ("Silence produces silence (and no NaN/Inf)", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::loCut, 300.0f);
    setParam (processor, ParamIDs::hiCut, 3000.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Full-scale input at extreme parameter values produces no NaN/Inf", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::loCut, CabConvolutionEngine::loCutMaxHz);
    setParam (processor, ParamIDs::hiCut, CabConvolutionEngine::hiCutMinHz);
    setParam (processor, ParamIDs::level, 24.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    // Refill with fresh input every iteration - Level's +24 dB is a genuine,
    // uncompressed linear gain (unlike a saturating clipper stage), so
    // repeatedly reprocessing the same buffer's own growing output in place
    // would compound that gain exponentially, which is not how a host ever
    // actually drives processBlock() (each call receives fresh audio).
    for (int i = 0; i < 8; ++i)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 1.0f);
        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }

    // Sane bound for a single pass: LoCut/HiCut (Butterworth, no resonant
    // peaking) leave a 1 kHz tone close to unity magnitude, and +24 dB is
    // ~15.85x - comfortably under 100.
    CHECK (TestHelpers::peakAbsolute (buffer) < 100.0f);
}

TEST_CASE ("Denormal-range input produces no NaN/Inf output", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::loCut, 300.0f);
    setParam (processor, ParamIDs::hiCut, 3000.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    constexpr int numSamples = 512;
    juce::AudioBuffer<float> buffer (2, numSamples);

    const auto denormalValue = std::numeric_limits<float>::denorm_min() * 4.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* data = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            data[sample] = (sample % 2 == 0) ? denormalValue : -denormalValue;
    }

    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
        CHECK_NOTHROW (processor.processBlock (buffer, midi));

    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("Zero-sample buffer does not crash processBlock", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 0);
    juce::MidiBuffer midi;

    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (buffer.getNumSamples() == 0);
}

TEST_CASE ("Extreme parameter values at both range edges produce no NaN/Inf", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (44100.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    juce::MidiBuffer midi;

    for (bool useMinimum : { true, false })
    {
        setParam (processor, ParamIDs::loCut, useMinimum ? CabConvolutionEngine::loCutMinHz : CabConvolutionEngine::loCutMaxHz);
        setParam (processor, ParamIDs::hiCut, useMinimum ? CabConvolutionEngine::hiCutMinHz : CabConvolutionEngine::hiCutMaxHz);
        setParam (processor, ParamIDs::level, useMinimum ? -24.0f : 24.0f);
        setParam (processor, ParamIDs::mix, useMinimum ? 0.0f : 100.0f);

        TestHelpers::fillWithSine (buffer, 44100.0, 440.0, 0.8f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("Rapid parameter automation across many blocks produces no NaN/Inf", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 256);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> unit (0.0f, 1.0f);

    juce::MidiBuffer midi;

    for (int block = 0; block < 100; ++block)
    {
        setParam (processor, ParamIDs::loCut,
                  CabConvolutionEngine::loCutMinHz + unit (rng) * (CabConvolutionEngine::loCutMaxHz - CabConvolutionEngine::loCutMinHz));
        setParam (processor, ParamIDs::hiCut,
                  CabConvolutionEngine::hiCutMinHz + unit (rng) * (CabConvolutionEngine::hiCutMaxHz - CabConvolutionEngine::hiCutMinHz));
        setParam (processor, ParamIDs::level, -24.0f + unit (rng) * 48.0f);
        setParam (processor, ParamIDs::mix, unit (rng) * 100.0f);

        juce::AudioBuffer<float> buffer (2, 256);
        TestHelpers::fillWithSine (buffer, 48000.0, 200.0 + unit (rng) * 4000.0, 0.7f);

        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }
}

TEST_CASE ("reset() followed by processBlock does not crash", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    setParam (processor, ParamIDs::loCut, 300.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    juce::MidiBuffer midi;

    processor.processBlock (buffer, midi);

    CHECK_NOTHROW (processor.reset());

    TestHelpers::fillWithSine (buffer, 48000.0, 1000.0, 0.6f);
    CHECK_NOTHROW (processor.processBlock (buffer, midi));
    CHECK (TestHelpers::allSamplesFinite (buffer));
}

TEST_CASE ("A loaded custom IR produces no NaN/Inf across many blocks", "[robustness]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    const auto irFile = juce::File::createTempFile (".wav");

    juce::AudioBuffer<float> ir (1, 256);
    std::mt19937 rng (99);
    std::uniform_real_distribution<float> unit (-1.0f, 1.0f);

    for (int i = 0; i < ir.getNumSamples(); ++i)
        ir.setSample (0, i, unit (rng) * std::exp (-0.02f * static_cast<float> (i)));

    REQUIRE (TestHelpers::writeWavFile (irFile, ir, 48000.0));
    REQUIRE (processor.loadImpulseResponseFromFile (irFile));

    setParam (processor, ParamIDs::loCut, 300.0f);
    setParam (processor, ParamIDs::hiCut, 3000.0f);
    setParam (processor, ParamIDs::mix, 100.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;

    for (int block = 0; block < 16; ++block)
    {
        TestHelpers::fillWithSine (buffer, 48000.0, 300.0 + static_cast<double> (block) * 111.0, 0.8f);
        CHECK_NOTHROW (processor.processBlock (buffer, midi));
        CHECK (TestHelpers::allSamplesFinite (buffer));
    }

    irFile.deleteFile();
}
