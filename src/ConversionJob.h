#pragma once
#include <juce_core/juce_core.h>

enum class JobStatus { Queued, Converting, Done, Error };

struct ConversionJob
{
    juce::File   inputFile;
    juce::String errorMessage;
    JobStatus    status { JobStatus::Queued };
};

struct ConversionSettings
{
    int targetSampleRate { 0 };   // 0 = keep original
    int targetBitDepth   { 0 };   // 0 = keep original; valid: 16, 24
    int flacQuality      { 5 };   // 0–8 compression level index
};
