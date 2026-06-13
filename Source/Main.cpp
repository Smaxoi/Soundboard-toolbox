#include <JuceHeader.h>
#include "MainComponent.h"

//==============================================================================
// 程式進入點：建立一個視窗，把音效板元件放進去。
class SoundboardApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Soundboard"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        // 指定預設字型為含中文字符的「微軟正黑體」，避免中文顯示成方框。
        juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypefaceName ("Microsoft JhengHei UI");

        mainWindow.reset (new MainWindow (juce::CharPointer_UTF8 ("VoiceMod 工具箱"),
                                          new MainComponent(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }

    //==========================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name, juce::Component* content, JUCEApplication& a)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons),
              app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (content, true);   // 視窗大小依 content 自己的 setSize
            setResizable (true, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SoundboardApplication)
