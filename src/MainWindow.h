#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ConverterComponent.h"

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name);
    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
