#include "CabConvolutionEngine.h"

namespace
{
    // Keeps a requested filter frequency safely below Nyquist regardless of
    // host sample rate, so juce::dsp::IIR::Coefficients::makeHighPass/
    // makeLowPass never receives an out-of-range value (which would produce
    // invalid/NaN coefficients).
    float clampBelowNyquist (float frequencyHz, double sampleRate) noexcept
    {
        const auto nyquist = static_cast<float> (sampleRate) * 0.5f;
        return juce::jlimit (10.0f, nyquist * 0.9f, frequencyHz);
    }

    // A single-sample, unit-amplitude impulse response: the mathematical
    // identity for convolution (y = x * delta = x). Declaring it mono is
    // deliberate - juce::dsp::Convolution applies a mono IR identically to
    // every processed channel, so a 1-channel delta is sufficient regardless
    // of whether the host session is mono or stereo.
    juce::AudioBuffer<float> makeDeltaImpulseResponse()
    {
        juce::AudioBuffer<float> buffer (1, 1);
        buffer.setSample (0, 0, 1.0f);
        return buffer;
    }
}

CabConvolutionEngine::CabConvolutionEngine() = default;

void CabConvolutionEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannelsPrepared = static_cast<int> (spec.numChannels);

    // Establish a valid IR before the first process() call. On the very
    // first prepare() (no IR ever loaded/requested yet), that means the
    // default delta/identity IR. On subsequent prepares (sample-rate
    // change, etc.) juce::dsp::Convolution retains and automatically
    // re-resamples whatever IR was most recently loaded, so nothing further
    // needs to be done here - see the class-level docs on
    // juce::dsp::Convolution::prepare() for this contract.
    if (! anyImpulseResponseLoaded)
        loadDefaultImpulseResponse();

    // Per juce::dsp::Convolution's documented contract: loadImpulseResponse()
    // must be called *before* prepare() for that IR to be guaranteed active
    // during the very first process() call.
    convolution.prepare (spec);

    loCutFilter.prepare (spec);
    hiCutFilter.prepare (spec);

    // Prime the target gain from lastLevelDb *before* prepare() (which
    // internally calls reset(), snapping current == target) - otherwise a
    // freshly constructed engine's Level would default to silence (see
    // lastLevelDb's declaration in the header) rather than unity gain.
    outputLevel.setGainDecibels (lastLevelDb);
    outputLevel.setRampDurationSeconds (smoothingTimeSeconds);
    outputLevel.prepare (spec);

    dryWetMixer.prepare (spec);

    latencySamples = convolution.getLatency();
    dryWetMixer.setWetLatency (static_cast<float> (latencySamples));

    // juce::dsp::DryWetMixer defaults its internal mix to fully wet (1.0)
    // until told otherwise, and its own reset() (called from our reset()
    // below) snaps its internal dry/wet gain smoothers' *current* value to
    // whatever their *target* happens to be at that moment - it does not
    // know about lastMixProportion. Priming the real target here, before
    // reset() runs, means the mixer is already sitting at the correct dry/
    // wet balance for the very first process() call instead of ramping up
    // from "fully wet" over its internal 50ms default ramp.
    dryWetMixer.setWetMixProportion (lastMixProportion);

    // Re-seed the smoothers at the new sample rate, but pin current ==
    // target to whatever was last requested (defaulting to the
    // ParameterLayout defaults on first prepare) - otherwise the ramp would
    // sweep up from a default-constructed 0 Hz/0.0 on the very first block.
    loCutFrequencySmoothed.reset (sampleRate, smoothingTimeSeconds);
    loCutFrequencySmoothed.setCurrentAndTargetValue (lastLoCutHz);
    hiCutFrequencySmoothed.reset (sampleRate, smoothingTimeSeconds);
    hiCutFrequencySmoothed.setCurrentAndTargetValue (lastHiCutHz);
    mixSmoothed.reset (sampleRate, smoothingTimeSeconds);
    mixSmoothed.setCurrentAndTargetValue (lastMixProportion);

    reset();

    // Prime the filter coefficients immediately (only meaningful if not
    // bypassed - see process()) so a subsequent engage of the filter starts
    // from correct, non-default coefficients rather than an identity/
    // uninitialised state.
    *loCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, clampBelowNyquist (lastLoCutHz, sampleRate), filterQ);
    *hiCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (
        sampleRate, clampBelowNyquist (lastHiCutHz, sampleRate), filterQ);

    loCutEngagedPreviously = lastLoCutHz > loCutMinHz + bypassEpsilonHz;
    hiCutEngagedPreviously = lastHiCutHz < hiCutMaxHz - bypassEpsilonHz;
}

void CabConvolutionEngine::reset()
{
    convolution.reset();
    loCutFilter.reset();
    hiCutFilter.reset();
    outputLevel.reset();
    dryWetMixer.reset();
}

void CabConvolutionEngine::setLoCutHz (float newFrequencyHz)
{
    lastLoCutHz = newFrequencyHz;
    loCutFrequencySmoothed.setTargetValue (newFrequencyHz);
}

void CabConvolutionEngine::setHiCutHz (float newFrequencyHz)
{
    lastHiCutHz = newFrequencyHz;
    hiCutFrequencySmoothed.setTargetValue (newFrequencyHz);
}

void CabConvolutionEngine::setMixProportion (float newProportion01)
{
    lastMixProportion = newProportion01;
    mixSmoothed.setTargetValue (newProportion01);
}

void CabConvolutionEngine::setLevelDb (float newLevelDb)
{
    lastLevelDb = newLevelDb;
    outputLevel.setGainDecibels (newLevelDb);
}

void CabConvolutionEngine::setImpulseResponse (juce::AudioBuffer<float> irBuffer, double irSampleRate)
{
    const auto isStereo = (irBuffer.getNumChannels() >= 2 && numChannelsPrepared >= 2)
                               ? juce::dsp::Convolution::Stereo::yes
                               : juce::dsp::Convolution::Stereo::no;

    // Normalise::yes: JUCE scales the IR's energy to a consistent reference
    // level, so switching between wildly different real-world cabinet IRs
    // doesn't also produce wildly different output levels.
    convolution.loadImpulseResponse (std::move (irBuffer),
                                      irSampleRate,
                                      isStereo,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::yes);

    anyImpulseResponseLoaded = true;
}

void CabConvolutionEngine::loadDefaultImpulseResponse()
{
    // Normalise::no is essential here: normalising a unit impulse would
    // rescale it away from exact unity gain (JUCE's normalisation targets a
    // fixed reference energy, not "leave amplitude 1.0 alone"), which would
    // break the passthrough guarantee the default IR exists to provide.
    convolution.loadImpulseResponse (makeDeltaImpulseResponse(),
                                      sampleRate,
                                      juce::dsp::Convolution::Stereo::no,
                                      juce::dsp::Convolution::Trim::no,
                                      juce::dsp::Convolution::Normalise::no);

    anyImpulseResponseLoaded = true;
}

void CabConvolutionEngine::process (juce::dsp::AudioBlock<float>& block)
{
    const auto numSamples = block.getNumSamples();

    if (numSamples == 0)
        return;

    // Coefficient recomputation involves trig calls (tan/cos), so filter
    // frequencies are smoothed and re-derived once per block rather than
    // per sample - a standard real-time-safe compromise for IIR filters.
    const auto loCutHz = loCutFrequencySmoothed.skip (static_cast<int> (numSamples));
    const auto hiCutHz = hiCutFrequencySmoothed.skip (static_cast<int> (numSamples));
    const auto wetMix = mixSmoothed.skip (static_cast<int> (numSamples));

    // LoCut at its minimum and HiCut at its maximum are each an explicit
    // "off" position: skip the IIR processing entirely (rather than merely
    // computing an extreme-but-active cutoff) so the default/wide-open state
    // is a true bit-accurate passthrough, not just a filter with negligible
    // colouration. This is what tests/EngineTests.cpp's null test relies on.
    const bool loCutBypassed = loCutHz <= loCutMinHz + bypassEpsilonHz;
    const bool hiCutBypassed = hiCutHz >= hiCutMaxHz - bypassEpsilonHz;

    // Reset a filter's IIR state exactly when it transitions from bypassed
    // to engaged, so it starts from a clean, predictable state rather than
    // reusing whatever memory it was last left in an arbitrary number of
    // blocks ago.
    if (! loCutBypassed && ! loCutEngagedPreviously)
        loCutFilter.reset();

    if (! hiCutBypassed && ! hiCutEngagedPreviously)
        hiCutFilter.reset();

    loCutEngagedPreviously = ! loCutBypassed;
    hiCutEngagedPreviously = ! hiCutBypassed;

    if (! loCutBypassed)
        *loCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, clampBelowNyquist (loCutHz, sampleRate), filterQ);

    if (! hiCutBypassed)
        *hiCutFilter.state = *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, clampBelowNyquist (hiCutHz, sampleRate), filterQ);

    dryWetMixer.setWetMixProportion (wetMix);

    juce::dsp::ProcessContextReplacing<float> context (block);

    // Capture the pre-processing signal as "dry" before convolution or
    // filtering touches `block`. DryWetMixer internally delays this by
    // getLatencySamples() (set via setWetLatency in prepare()) so it stays
    // time-aligned with the wet path below, whatever that latency is.
    dryWetMixer.pushDrySamples (block);

    convolution.process (context);

    if (! loCutBypassed)
        loCutFilter.process (context);

    if (! hiCutBypassed)
        hiCutFilter.process (context);

    dryWetMixer.mixWetSamples (block);

    outputLevel.process (context);
}
