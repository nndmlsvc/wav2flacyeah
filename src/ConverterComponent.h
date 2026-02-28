#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ConversionJob.h"
#include "ConversionThread.h"

class ConverterComponent : public juce::Component,
                           public juce::FileDragAndDropTarget,
                           public juce::ListBoxModel
{
public:
    ConverterComponent();
    ~ConverterComponent() override;

    // Component
    void paint   (juce::Graphics& g) override;
    void resized () override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped           (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter          (const juce::StringArray& files, int x, int y) override;
    void fileDragExit           (const juce::StringArray& files) override;

    // ListBoxModel
    int  getNumRows () override;
    void paintListBoxItem (int row, juce::Graphics& g,
                           int width, int height, bool selected) override;

private:
    // Settings panel controls
    juce::Label    srLabel    { {}, "Sample Rate" };
    juce::ComboBox srCombo;
    juce::Label    bdLabel    { {}, "Bit Depth" };
    juce::ComboBox bdCombo;
    juce::Label    qualLabel  { {}, "Compression Level (0-8)" };
    juce::Slider   qualSlider;

    // Action buttons
    juce::TextButton browseBtn   { "Add Files..." };
    juce::TextButton clearBtn    { "Clear" };
    juce::TextButton convertBtn  { "Convert All" };
    juce::TextButton cancelBtn   { "Cancel" };

    // File queue
    juce::ListBox  fileList;

    // Progress
    double         perFileProg  { 0.0 };
    double         overallProg  { 0.0 };
    juce::ProgressBar perFileBar  { perFileProg };
    juce::ProgressBar overallBar  { overallProg };
    juce::Label    perFileLabel { {}, "File:" };
    juce::Label    overallLabel { {}, "Overall:" };
    juce::Label    statusLabel;

    // State
    bool                       dragHover { false };
    juce::Array<ConversionJob> jobs;
    int                        currentJobIdx { -1 };

    ConversionThread convThread;

    std::unique_ptr<juce::FileChooser> fileChooser;

    // Helpers
    void addFiles           (const juce::StringArray& paths);
    void startConversion    ();
    void stopConversion     ();
    ConversionSettings buildSettings () const;
    void onProgress         (int jobIdx, float fileProg, float totalProg,
                             JobStatus status, const juce::String& errMsg);
    void updateButtons      ();

    static constexpr int kSettingsW = 220;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConverterComponent)
};
