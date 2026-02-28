#include "ConversionThread.h"
#include <juce_audio_basics/juce_audio_basics.h>

ConversionThread::ConversionThread()
    : juce::Thread ("Wav2FlacYeah Worker")
{
    formatManager.registerBasicFormats();
}

ConversionThread::~ConversionThread()
{
    stopThread (4000);
}

void ConversionThread::setJobs (juce::Array<ConversionJob> newJobs,
                                ConversionSettings         newSettings,
                                ProgressCallback           callback)
{
    juce::ScopedLock sl (lock);
    jobs             = std::move (newJobs);
    settings         = newSettings;
    progressCallback = std::move (callback);
}

void ConversionThread::run()
{
    juce::Array<ConversionJob> localJobs;
    ConversionSettings         localSettings;
    ProgressCallback           localCallback;

    {
        juce::ScopedLock sl (lock);
        localJobs     = jobs;
        localSettings = settings;
        localCallback = progressCallback;
    }

    const int total = localJobs.size();

    for (int i = 0; i < total; ++i)
    {
        if (threadShouldExit())
            break;

        auto& job = localJobs.getReference (i);
        job.status = JobStatus::Converting;

        juce::MessageManager::callAsync ([cb = localCallback, i, total]
        {
            cb (i, 0.0f, float (i) / float (total), JobStatus::Converting, {});
        });

        bool ok = convertFile (job, localSettings);
        job.status = ok ? JobStatus::Done : JobStatus::Error;

        const float overall  = float (i + 1) / float (total);
        const auto  status   = job.status;
        const auto  errMsg   = job.errorMessage;

        juce::MessageManager::callAsync ([cb = localCallback, i, overall, status, errMsg]
        {
            cb (i, 1.0f, overall, status, errMsg);
        });
    }
}

bool ConversionThread::convertFile (ConversionJob& job, const ConversionSettings& s)
{
    // Open reader
    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (job.inputFile));

    if (reader == nullptr)
    {
        job.errorMessage = "Cannot read: " + job.inputFile.getFileName();
        return false;
    }

    const double srcRate    = reader->sampleRate;
    const int    srcBits    = int (reader->bitsPerSample);
    const int    numCh      = int (reader->numChannels);
    const int64_t numFrames = reader->lengthInSamples;

    const double outRate    = (s.targetSampleRate > 0) ? double (s.targetSampleRate) : srcRate;
    // FLAC max is 24-bit; clamp to 24 if reader has 32-bit float
    int outBits = (s.targetBitDepth > 0) ? s.targetBitDepth : juce::jmin (srcBits, 24);
    outBits = juce::jmin (outBits, 24);

    const juce::File outFile = job.inputFile.withFileExtension ("flac");

    auto outStream = std::make_unique<juce::FileOutputStream> (outFile);
    if (outStream->failedToOpen())
    {
        job.errorMessage = "Cannot write: " + outFile.getFullPathName();
        return false;
    }
    outStream->setPosition (0);
    outStream->truncate();

    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        flac.createWriterFor (outStream.get(),
                              outRate,
                              unsigned (numCh),
                              outBits,
                              {},
                              s.flacQuality));

    if (writer == nullptr)
    {
        job.errorMessage = "FLAC writer failed (bit depth " + juce::String (outBits) + " unsupported?)";
        return false;
    }
    outStream.release(); // writer now owns the stream

    const bool needsResample = (outRate != srcRate);
    const int  blockSize     = 8192;

    if (!needsResample)
    {
        juce::AudioBuffer<float> buf (numCh, blockSize);
        int64_t pos = 0;

        while (pos < numFrames && !threadShouldExit())
        {
            int n = int (juce::jmin (int64_t (blockSize), numFrames - pos));
            reader->read (&buf, 0, n, pos, true, true);

            if (!writer->writeFromAudioSampleBuffer (buf, 0, n))
            {
                job.errorMessage = "Write error";
                return false;
            }
            pos += n;

            const float fp      = float (pos) / float (numFrames);
            const int   jobIdx  = -1; // progress-only signal; UI uses atomic ref
            // Throttle async calls: update every ~0.5% to avoid flooding
            if (int (fp * 200) != int ((fp - float (n) / float (numFrames)) * 200))
            {
                juce::MessageManager::callAsync ([cb = progressCallback,
                                                  fp,
                                                  jobIdx] () mutable
                {
                    // jobIdx == -1 means file-progress-only update
                    cb (jobIdx, fp, -1.0f, JobStatus::Converting, {});
                });
            }
        }
    }
    else
    {
        // Load entire file into memory, then resample
        juce::AudioBuffer<float> srcBuf (numCh, int (numFrames));
        reader->read (&srcBuf, 0, int (numFrames), 0, true, true);

        juce::MemoryAudioSource   memSrc (srcBuf, false, false);
        juce::ResamplingAudioSource resampler (&memSrc, false, numCh);
        resampler.setResamplingRatio (srcRate / outRate);
        resampler.prepareToPlay (blockSize, outRate);

        const int64_t outFrames = int64_t (double (numFrames) * outRate / srcRate + 0.5);
        juce::AudioBuffer<float> block (numCh, blockSize);
        int64_t written = 0;

        while (written < outFrames && !threadShouldExit())
        {
            int n = int (juce::jmin (int64_t (blockSize), outFrames - written));
            juce::AudioSourceChannelInfo info (&block, 0, n);
            resampler.getNextAudioBlock (info);

            if (!writer->writeFromAudioSampleBuffer (block, 0, n))
            {
                job.errorMessage = "Write error during resample";
                resampler.releaseResources();
                return false;
            }
            written += n;

            const float fp = float (written) / float (outFrames);
            if (int (fp * 200) != int ((fp - float (n) / float (outFrames)) * 200))
            {
                juce::MessageManager::callAsync ([cb = progressCallback, fp] () mutable
                {
                    cb (-1, fp, -1.0f, JobStatus::Converting, {});
                });
            }
        }

        resampler.releaseResources();
    }

    return !threadShouldExit();
}
