#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>

// Small shared helpers used across the Tests target. Kept dependency-light
// (juce_audio_basics + juce_audio_formats) so it can be included from any
// test file.
namespace TestHelpers
{
    // Fills every channel of the buffer with a sine wave of the given
    // frequency. `startSampleIndex` offsets the phase calculation, so
    // calling this for consecutive blocks with startSampleIndex incremented
    // by each block's length produces a phase-continuous sine across block
    // boundaries. Defaults to 0 (phase continuity across separate, unrelated
    // calls is not needed for the RMS/peak-based checks that most callers
    // use this for).
    inline void fillWithSine (juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              double frequencyHz,
                              float amplitude = 0.5f,
                              juce::int64 startSampleIndex = 0)
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto phase = juce::MathConstants<double>::twoPi * frequencyHz
                                    * static_cast<double> (startSampleIndex + sample) / sampleRate;
                data[sample] = amplitude * static_cast<float> (std::sin (phase));
            }
        }
    }

    // Root-mean-square level across all channels/samples in the buffer.
    inline double rms (const juce::AudioBuffer<float>& buffer)
    {
        double sumOfSquares = 0.0;
        juce::int64 numValues = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = static_cast<double> (data[sample]);
                sumOfSquares += value * value;
                ++numValues;
            }
        }

        return numValues > 0 ? std::sqrt (sumOfSquares / static_cast<double> (numValues)) : 0.0;
    }

    // Largest absolute sample value across all channels/samples.
    inline float peakAbsolute (const juce::AudioBuffer<float>& buffer)
    {
        float peak = 0.0f;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                peak = std::max (peak, std::abs (data[sample]));
        }

        return peak;
    }

    // Returns true if every sample in the buffer is finite (no NaN/Inf).
    inline bool allSamplesFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite (data[sample]))
                    return false;
        }

        return true;
    }

    // Writes `buffer` to `destinationFile` as a 32-bit float WAV, for tests
    // that need a real IR file on disk (state round-trip, file-loading
    // tests). Returns true on success.
    inline bool writeWavFile (const juce::File& destinationFile,
                              const juce::AudioBuffer<float>& buffer,
                              double sampleRate)
    {
        destinationFile.deleteFile();

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::OutputStream> outputStream (destinationFile.createOutputStream());

        if (outputStream == nullptr)
            return false;

        const auto options = juce::AudioFormatWriterOptions()
                                  .withSampleRate (sampleRate)
                                  .withNumChannels (buffer.getNumChannels())
                                  .withBitsPerSample (32)
                                  .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        std::unique_ptr<juce::AudioFormatWriter> writer (wavFormat.createWriterFor (outputStream, options));

        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
}
