#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs for
// Nave. See docs/architecture.md for the corresponding signal-flow diagram.
//
// FROZEN AS OF THE v0.1 PARAMETER LAYOUT:
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, and skew MAY still be refined during voicing/tuning milestones;
// only the IDs themselves are frozen.
namespace ParamIDs
{
    // Post-convolution high-pass. Range spans its own "off" position at the
    // minimum (20 Hz, the default): CabConvolutionEngine bypasses the filter
    // entirely rather than merely setting a subsonic cutoff, so the default
    // state is a true passthrough (see docs/architecture.md, "Filter bypass
    // at the range extremes").
    inline constexpr auto loCut = "loCut";

    // Post-convolution low-pass. Symmetric to loCut: its "off" position is
    // its maximum (20 kHz, the default), also a true bypass.
    inline constexpr auto hiCut = "hiCut";

    // Dry/wet mix. Default 100% (fully wet) - a cabinet IR is normally run
    // fully in the signal path.
    inline constexpr auto mix = "mix";

    // Output trim, applied after the dry/wet mix.
    inline constexpr auto level = "level";

    // NOT an APVTS parameter: the currently loaded IR file's absolute path is
    // stored as a plain property directly on apvts.state (see
    // PluginProcessor::loadImpulseResponseFromFile/getStateInformation), so
    // it round-trips through session/preset state without needing a float
    // parameter to represent a file path.
    inline constexpr auto irFilePathProperty = "irFilePath";
}
