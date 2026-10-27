#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace avel::time
{
    /** Factory presets (built in) and user presets (XML files in Documents/Avel/Avel Time/Presets). */
    class PresetManager
    {
    public:
        explicit PresetManager (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

        struct Factory { juce::String name, category; std::vector<std::pair<juce::String, float>> values; };
        static const std::vector<Factory>& factory();

        static juce::File userFolder();
        juce::Array<juce::File> userPresets() const;

        void loadFactory (int index);
        bool loadUser (const juce::File& f);
        bool saveUser (const juce::String& name);
        void resetToDefaults();

        /** Steps through factory then user presets. */
        void step (int dir);

        juce::String currentName = "Default";
        int currentFactory = -1;
        juce::File currentFile;

    private:
        void applyValues (const std::vector<std::pair<juce::String, float>>& v);
        juce::AudioProcessorValueTreeState& apvts;
    };
}
