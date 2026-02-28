#include "ConverterComponent.h"
#include <BinaryData.h>

using namespace juce;

// ─── Palette ─────────────────────────────────────────────────────────────────
static const Colour kBg        { 0xff1e1e2e };
static const Colour kPanel     { 0xff28283e };
static const Colour kBorder    { 0xff44446a };
static const Colour kText      { 0xffdedeff };
static const Colour kSubtext   { 0xff8888aa };
static const Colour kAccent    { 0xff7c88ff };
static const Colour kGreen     { 0xff50fa7b };
static const Colour kOrange    { 0xffffb86c };
static const Colour kRed       { 0xffff5555 };

// ─── Constructor ─────────────────────────────────────────────────────────────
ConverterComponent::ConverterComponent()
{
    // Load logo from binary data
    logo = ImageCache::getFromMemory (BinaryData::GK_png, BinaryData::GK_pngSize);

    // Sample rate combo
    srCombo.addItem ("Keep original", 1);
    srCombo.addItem ("44100 Hz",      2);
    srCombo.addItem ("48000 Hz",      3);
    srCombo.addItem ("88200 Hz",      4);
    srCombo.addItem ("96000 Hz",      5);
    srCombo.addItem ("192000 Hz",     6);
    srCombo.setSelectedId (1, dontSendNotification);

    // Bit depth combo  (FLAC max is 24-bit)
    bdCombo.addItem ("Keep original", 1);
    bdCombo.addItem ("16-bit",        2);
    bdCombo.addItem ("24-bit",        3);
    bdCombo.setSelectedId (1, dontSendNotification);

    // Quality slider
    qualSlider.setRange (0.0, 8.0, 1.0);
    qualSlider.setValue (5.0, dontSendNotification);
    qualSlider.setSliderStyle (Slider::LinearHorizontal);
    qualSlider.setTextBoxStyle (Slider::TextBoxRight, false, 28, 20);

    // Label colours
    for (auto* l : { &srLabel, &bdLabel, &qualLabel })
    {
        l->setFont (FontOptions (11.5f));
        l->setColour (Label::textColourId, kSubtext);
    }

    statusLabel.setFont (FontOptions (11.0f));
    statusLabel.setColour (Label::textColourId, kSubtext);
    statusLabel.setJustificationType (Justification::centredLeft);

    perFileLabel.setFont (FontOptions (10.0f));
    perFileLabel.setColour (Label::textColourId, kSubtext);
    overallLabel.setFont (FontOptions (10.0f));
    overallLabel.setColour (Label::textColourId, kSubtext);

    // Buttons
    browseBtn.onClick = [this]
    {
        fileChooser = std::make_unique<FileChooser> ("Select WAV files",
            File::getSpecialLocation (File::userMusicDirectory), "*.wav;*.WAV");
        fileChooser->launchAsync (
            FileBrowserComponent::openMode |
            FileBrowserComponent::canSelectFiles |
            FileBrowserComponent::canSelectMultipleItems,
            [this] (const FileChooser& fc)
            {
                StringArray paths;
                for (auto& f : fc.getResults())
                    paths.add (f.getFullPathName());
                addFiles (paths);
            });
    };

    clearBtn.onClick = [this]
    {
        if (convThread.isThreadRunning()) return;
        jobs.clear();
        perFileProg = overallProg = 0.0;
        statusLabel.setText ({}, dontSendNotification);
        fileList.updateContent();
        updateButtons();
    };

    convertBtn.onClick = [this] { startConversion(); };
    cancelBtn.onClick  = [this] { stopConversion(); };

    // File list
    fileList.setModel (this);
    fileList.setRowHeight (24);
    fileList.setColour (ListBox::backgroundColourId, kPanel);
    fileList.setColour (ListBox::outlineColourId,    kBorder);
    fileList.setOutlineThickness (1);

    for (auto* c : { &srLabel, &bdLabel, &qualLabel,
                     &perFileLabel, &overallLabel, &statusLabel })
        addAndMakeVisible (c);
    addAndMakeVisible (srCombo);
    addAndMakeVisible (bdCombo);
    addAndMakeVisible (qualSlider);
    addAndMakeVisible (browseBtn);
    addAndMakeVisible (clearBtn);
    addAndMakeVisible (convertBtn);
    addAndMakeVisible (cancelBtn);
    addAndMakeVisible (fileList);
    addAndMakeVisible (perFileBar);
    addAndMakeVisible (overallBar);

    updateButtons();
    setSize (760, 580);
}

ConverterComponent::~ConverterComponent()
{
    convThread.stopThread (4000);
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void ConverterComponent::resized()
{
    auto full = getLocalBounds();

    // Header bar at top (logo lives here, drawn in paint())
    full.removeFromTop (kHeaderH);

    // Right panel: settings
    auto panel = full.removeFromRight (kSettingsW);
    full.removeFromRight (1); // separator

    panel = panel.reduced (12, 12);

    auto row = [&] (int h) { return panel.removeFromTop (h); };

    // Clear "Settings" title space
    panel.removeFromTop (26);

    // Sample rate
    srLabel.setBounds (row (18));
    panel.removeFromTop (2);
    srCombo.setBounds (row (24));
    panel.removeFromTop (10);

    // Bit depth
    bdLabel.setBounds (row (18));
    panel.removeFromTop (2);
    bdCombo.setBounds (row (24));
    panel.removeFromTop (10);

    // Compression level
    qualLabel.setBounds (row (18));
    panel.removeFromTop (2);
    qualSlider.setBounds (row (26));
    panel.removeFromTop (18);

    // Buttons
    convertBtn.setBounds (row (28));
    panel.removeFromTop (6);
    cancelBtn.setBounds (row (28));

    // Left: file area
    auto left = full.reduced (10, 10);

    // Top button row
    auto btnRow = left.removeFromTop (28);
    browseBtn.setBounds (btnRow.removeFromLeft (110));
    btnRow.removeFromLeft (6);
    clearBtn.setBounds (btnRow.removeFromLeft (70));
    left.removeFromTop (8);

    // Progress rows at bottom
    auto bottom = left.removeFromBottom (18);
    auto ol = bottom.removeFromLeft (54);
    overallLabel.setBounds (ol);
    overallBar.setBounds (bottom);

    left.removeFromBottom (4);
    bottom = left.removeFromBottom (18);
    auto fl = bottom.removeFromLeft (54);
    perFileLabel.setBounds (fl);
    perFileBar.setBounds (bottom);

    left.removeFromBottom (4);
    statusLabel.setBounds (left.removeFromBottom (16));
    left.removeFromBottom (4);

    // File list fills remaining space
    fileList.setBounds (left);
}

// ─── Paint ───────────────────────────────────────────────────────────────────
void ConverterComponent::paint (Graphics& g)
{
    g.fillAll (kBg);

    // ── Header bar ──────────────────────────────────────────────────────────
    auto headerRect = getLocalBounds().removeFromTop (kHeaderH);
    g.setColour (kPanel);
    g.fillRect (headerRect);

    // Bottom border on header
    g.setColour (kBorder);
    g.fillRect (headerRect.removeFromBottom (1).toFloat());

    // Logo: 300px wide, proportional height, vertically centred, 12px left margin
    if (logo.isValid())
    {
        const int logoW = 300;
        const int logoH = int (float (logoW) * float (logo.getHeight()) / float (logo.getWidth()));
        const int logoX = 12;
        const int logoY = (kHeaderH - logoH) / 2;
        g.drawImage (logo, logoX, logoY, logoW, logoH,
                     0, 0, logo.getWidth(), logo.getHeight());
    }

    // ── Settings panel background ────────────────────────────────────────────
    auto panelRect = getLocalBounds().withTrimmedTop (kHeaderH).removeFromRight (kSettingsW).toFloat();
    g.setColour (kPanel);
    g.fillRect (panelRect);

    // Separator line
    g.setColour (kBorder);
    g.fillRect (getLocalBounds().withTrimmedTop (kHeaderH)
                                .removeFromRight (kSettingsW + 1)
                                .removeFromLeft (1).toFloat());

    // Panel title
    g.setFont (FontOptions (13.0f, Font::bold));
    g.setColour (kText);
    g.drawText ("Settings", panelRect.removeFromTop (32).reduced (12, 6),
                Justification::centredLeft, false);

    // ── Drop zone highlight ──────────────────────────────────────────────────
    if (dragHover)
    {
        auto zone = getLocalBounds().withTrimmedTop (kHeaderH)
                                    .withTrimmedRight (kSettingsW + 1)
                                    .reduced (6).toFloat();
        g.setColour (kAccent.withAlpha (0.15f));
        g.fillRoundedRectangle (zone, 6.0f);
        g.setColour (kAccent.withAlpha (0.8f));
        g.drawRoundedRectangle (zone, 6.0f, 2.0f);

        g.setFont (FontOptions (14.0f));
        g.setColour (kAccent);
        g.drawText ("Drop WAV files here", zone, Justification::centred, false);
    }

    // ── Empty state hint ────────────────────────────────────────────────────
    if (jobs.isEmpty() && !dragHover)
    {
        auto zone = fileList.getBounds().toFloat();
        g.setFont (FontOptions (13.0f));
        g.setColour (kSubtext);
        g.drawText ("Drag & drop WAV files here, or click \"Add Files...\"",
                    zone, Justification::centred, false);
    }
}

// ─── FileDragAndDropTarget ───────────────────────────────────────────────────
bool ConverterComponent::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav"))
            return true;
    return false;
}

void ConverterComponent::fileDragEnter (const StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void ConverterComponent::fileDragExit (const StringArray&)
{
    dragHover = false;
    repaint();
}

void ConverterComponent::filesDropped (const StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    addFiles (files);
}

// ─── ListBoxModel ────────────────────────────────────────────────────────────
int ConverterComponent::getNumRows()
{
    return jobs.size();
}

void ConverterComponent::paintListBoxItem (int row, Graphics& g,
                                           int width, int height, bool selected)
{
    if (row < 0 || row >= jobs.size()) return;
    const auto& job = jobs[row];

    g.setColour (selected ? kAccent.withAlpha (0.2f)
                          : (row % 2 == 0 ? kPanel : kBg));
    g.fillRect (0, 0, width, height);

    Colour dot;
    String statusText;
    switch (job.status)
    {
        case JobStatus::Queued:     dot = kSubtext;  statusText = "Queued";     break;
        case JobStatus::Converting: dot = kOrange;   statusText = "Converting"; break;
        case JobStatus::Done:       dot = kGreen;    statusText = "Done";       break;
        case JobStatus::Error:      dot = kRed;      statusText = "Error";      break;
    }

    const float cy = float (height) * 0.5f;
    g.setColour (dot);
    g.fillEllipse (8.0f, cy - 4.0f, 8.0f, 8.0f);

    g.setFont (FontOptions (12.0f));
    g.setColour (kText);
    g.drawText (job.inputFile.getFileName(),
                22, 0, width - 130, height,
                Justification::centredLeft, true);

    g.setFont (FontOptions (11.0f));
    g.setColour (dot);
    String rightText = job.status == JobStatus::Error ? job.errorMessage.substring (0, 20)
                                                      : statusText;
    g.drawText (rightText, width - 120, 0, 114, height,
                Justification::centredRight, true);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
void ConverterComponent::addFiles (const StringArray& paths)
{
    for (auto& p : paths)
    {
        File f (p);
        if (!f.hasFileExtension ("wav") || !f.existsAsFile())
            continue;
        bool dup = false;
        for (auto& j : jobs)
            if (j.inputFile == f) { dup = true; break; }
        if (!dup)
            jobs.add ({ f, {}, JobStatus::Queued });
    }
    fileList.updateContent();
    updateButtons();
    repaint();
}

ConversionSettings ConverterComponent::buildSettings() const
{
    ConversionSettings s;

    static const int srVals[] = { 0, 44100, 48000, 88200, 96000, 192000 };
    int sid = srCombo.getSelectedId();
    s.targetSampleRate = (sid >= 1 && sid <= 6) ? srVals[sid - 1] : 0;

    static const int bdVals[] = { 0, 16, 24 };
    int bid = bdCombo.getSelectedId();
    s.targetBitDepth = (bid >= 1 && bid <= 3) ? bdVals[bid - 1] : 0;

    s.flacQuality = int (qualSlider.getValue());
    return s;
}

void ConverterComponent::startConversion()
{
    if (jobs.isEmpty() || convThread.isThreadRunning())
        return;

    for (auto& j : jobs)
        j.status = JobStatus::Queued;

    perFileProg = overallProg = 0.0;
    currentJobIdx = -1;
    statusLabel.setText ("Starting...", dontSendNotification);
    fileList.updateContent();

    convThread.setJobs (jobs, buildSettings(),
        [this] (int idx, float fp, float op, JobStatus st, String err)
        {
            onProgress (idx, fp, op, st, err);
        });

    convThread.startThread (Thread::Priority::normal);
    updateButtons();
}

void ConverterComponent::stopConversion()
{
    convThread.signalThreadShouldExit();
    convThread.stopThread (4000);
    statusLabel.setText ("Cancelled.", dontSendNotification);
    updateButtons();
}

void ConverterComponent::onProgress (int jobIdx, float fp, float op,
                                     JobStatus status, const String& errMsg)
{
    if (jobIdx >= 0 && jobIdx < jobs.size())
    {
        jobs.getReference (jobIdx).status = status;
        if (status == JobStatus::Error)
            jobs.getReference (jobIdx).errorMessage = errMsg;
        fileList.repaintRow (jobIdx);
        currentJobIdx = jobIdx;
    }

    if (fp >= 0.0f)  perFileProg = double (fp);
    if (op >= 0.0f)  overallProg = double (op);

    perFileBar.repaint();
    overallBar.repaint();

    if (jobIdx >= 0)
    {
        String msg;
        if (status == JobStatus::Converting)
            msg = "Converting: " + jobs[jobIdx].inputFile.getFileName();
        else if (status == JobStatus::Done && jobIdx == jobs.size() - 1)
            msg = "All done! " + String (jobs.size()) + " file(s) converted.";
        else if (status == JobStatus::Error)
            msg = "Error: " + errMsg;
        statusLabel.setText (msg, dontSendNotification);
    }

    if (op >= 1.0f)
        updateButtons();
}

void ConverterComponent::updateButtons()
{
    const bool running = convThread.isThreadRunning();
    const bool hasJobs = !jobs.isEmpty();

    browseBtn.setEnabled (!running);
    clearBtn.setEnabled  (!running && hasJobs);
    convertBtn.setEnabled (!running && hasJobs);
    cancelBtn.setEnabled  (running);
}
