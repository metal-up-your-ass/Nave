#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE ("State round-trip preserves non-default values of every parameter", "[state]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto* loCutParam = processor.apvts.getParameter (ParamIDs::loCut);
    auto* hiCutParam = processor.apvts.getParameter (ParamIDs::hiCut);
    auto* mixParam = processor.apvts.getParameter (ParamIDs::mix);
    auto* levelParam = processor.apvts.getParameter (ParamIDs::level);

    REQUIRE (loCutParam != nullptr);
    REQUIRE (hiCutParam != nullptr);
    REQUIRE (mixParam != nullptr);
    REQUIRE (levelParam != nullptr);

    loCutParam->setValueNotifyingHost (loCutParam->convertTo0to1 (250.0f));
    hiCutParam->setValueNotifyingHost (hiCutParam->convertTo0to1 (3500.0f));
    mixParam->setValueNotifyingHost (mixParam->convertTo0to1 (42.0f));
    levelParam->setValueNotifyingHost (levelParam->convertTo0to1 (-6.5f));

    const auto savedLoCut = loCutParam->getValue();
    const auto savedHiCut = hiCutParam->getValue();
    const auto savedMix = mixParam->getValue();
    const auto savedLevel = levelParam->getValue();

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Reset every parameter back to its default before restoring, so the
    // round-trip assertion below can't pass by accident.
    loCutParam->setValueNotifyingHost (loCutParam->getDefaultValue());
    hiCutParam->setValueNotifyingHost (hiCutParam->getDefaultValue());
    mixParam->setValueNotifyingHost (mixParam->getDefaultValue());
    levelParam->setValueNotifyingHost (levelParam->getDefaultValue());

    REQUIRE (loCutParam->getValue() != Catch::Approx (savedLoCut));
    REQUIRE (hiCutParam->getValue() != Catch::Approx (savedHiCut));
    REQUIRE (mixParam->getValue() != Catch::Approx (savedMix));
    REQUIRE (levelParam->getValue() != Catch::Approx (savedLevel));

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (loCutParam->getValue() == Catch::Approx (savedLoCut).margin (1e-6));
    CHECK (hiCutParam->getValue() == Catch::Approx (savedHiCut).margin (1e-6));
    CHECK (mixParam->getValue() == Catch::Approx (savedMix).margin (1e-6));
    CHECK (levelParam->getValue() == Catch::Approx (savedLevel).margin (1e-6));
}

TEST_CASE ("State round-trip preserves the loaded IR file path", "[state][ir]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getCurrentIrFilePath().isEmpty());

    const auto irFile = juce::File::createTempFile (".wav");

    juce::AudioBuffer<float> ir (1, 8);
    for (int i = 0; i < ir.getNumSamples(); ++i)
        ir.setSample (0, i, 1.0f / static_cast<float> (i + 1));

    REQUIRE (TestHelpers::writeWavFile (irFile, ir, 48000.0));
    REQUIRE (processor.loadImpulseResponseFromFile (irFile));

    CHECK (processor.getCurrentIrFilePath() == irFile.getFullPathName());

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    // Revert to the default IR before restoring, so the round-trip
    // assertion below can't pass by accident.
    processor.loadDefaultImpulseResponse();
    CHECK (processor.getCurrentIrFilePath().isEmpty());

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (processor.getCurrentIrFilePath() == irFile.getFullPathName());

    irFile.deleteFile();
}

TEST_CASE ("State round-trip with no IR loaded keeps the IR path empty", "[state][ir]")
{
    NaveAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getCurrentIrFilePath().isEmpty());

    juce::MemoryBlock savedState;
    processor.getStateInformation (savedState);
    REQUIRE (savedState.getSize() > 0);

    processor.setStateInformation (savedState.getData(), static_cast<int> (savedState.getSize()));

    CHECK (processor.getCurrentIrFilePath().isEmpty());
}
