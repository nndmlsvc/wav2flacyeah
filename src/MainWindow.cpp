#include "MainWindow.h"

MainWindow::MainWindow (const juce::String& name)
    : DocumentWindow (name,
                      juce::Colours::darkgrey,
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new ConverterComponent(), true);
    setResizable (true, false);
    setResizeLimits (600, 420, 2000, 1600);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
