#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_core/juce_core.h>
#include "ConversionJob.h"
#include <functional>

class ConversionThread : public juce::Thread
{
public:
    using ProgressCallback = std::function<void (int jobIndex,
                                                  float fileProgress,
                                                  float overallProgress,
                                                  JobStatus status,
                                                  juce::String errorMessage)>;

    ConversionThread();
    ~ConversionThread() override;

    // Call before startThread(). Thread receives its own copy of the job list.
    void setJobs (juce::Array<ConversionJob> jobs,
                  ConversionSettings         settings,
                  ProgressCallback           callback);

    void run() override;

private:
    bool convertFile (ConversionJob& job, const ConversionSettings& s);

    juce::Array<ConversionJob>  jobs;
    ConversionSettings          settings;
    ProgressCallback            progressCallback;
    juce::CriticalSection       lock;

    juce::AudioFormatManager    formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConversionThread)
};
